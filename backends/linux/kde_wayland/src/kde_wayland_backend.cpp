#include "kde_wayland_backend.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace mouse_sync::linux::kde_wayland
{
namespace
{

struct CommandResult
{
    int exit_code{0};
    std::string output;
};

struct RuntimeDevice
{
    std::string sys_name;
    std::map<std::string, std::string> properties;
};

std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool contains_case_insensitive(const std::string &value, const std::string &needle)
{
    return to_lower_copy(value).find(to_lower_copy(needle)) != std::string::npos;
}

const char *env_or_empty(const char *name)
{
    const char *value = std::getenv(name);
    return value ? value : "";
}

CommandResult run_command(const std::vector<std::string> &args)
{
    if (args.empty())
    {
        throw std::runtime_error("cannot execute empty command");
    }

    int pipe_fds[2] = {-1, -1};
    if (pipe(pipe_fds) != 0)
    {
        throw std::runtime_error(std::string("pipe() failed: ") + std::strerror(errno));
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        throw std::runtime_error(std::string("fork() failed: ") + std::strerror(errno));
    }

    if (pid == 0)
    {
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);

        std::vector<char *> argv;
        argv.reserve(args.size() + 1);
        for (const auto &arg : args)
        {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv.front(), argv.data());
        _exit(127);
    }

    close(pipe_fds[1]);

    std::string output;
    char buffer[4096];
    ssize_t read_count = 0;
    while ((read_count = read(pipe_fds[0], buffer, sizeof(buffer))) > 0)
    {
        output.append(buffer, static_cast<std::size_t>(read_count));
    }
    close(pipe_fds[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        throw std::runtime_error(std::string("waitpid() failed: ") + std::strerror(errno));
    }

    CommandResult result;
    result.output = output;

    if (WIFEXITED(status))
    {
        result.exit_code = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        result.exit_code = 128 + WTERMSIG(status);
    }
    else
    {
        result.exit_code = 1;
    }

    return result;
}

std::string require_command_success(const std::vector<std::string> &args, const std::string &context)
{
    const auto result = run_command(args);
    if (result.exit_code != 0)
    {
        throw std::runtime_error(context + ": " + trim(result.output));
    }
    return result.output;
}

std::vector<std::string> split_lines(const std::string &value)
{
    std::vector<std::string> lines;
    std::string current;
    for (char ch : value)
    {
        if (ch == '\n')
        {
            lines.push_back(trim(current));
            current.clear();
            continue;
        }
        if (ch != '\r')
        {
            current.push_back(ch);
        }
    }

    if (!current.empty())
    {
        lines.push_back(trim(current));
    }

    return lines;
}

std::optional<bool> parse_bool(const std::map<std::string, std::string> &properties, const std::string &key)
{
    const auto it = properties.find(key);
    if (it == properties.end())
    {
        return std::nullopt;
    }

    if (it->second == "true")
    {
        return true;
    }
    if (it->second == "false")
    {
        return false;
    }
    return std::nullopt;
}

std::optional<int> parse_int(const std::map<std::string, std::string> &properties, const std::string &key)
{
    const auto it = properties.find(key);
    if (it == properties.end())
    {
        return std::nullopt;
    }

    try
    {
        return std::stoi(it->second);
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

std::optional<double> parse_double(const std::map<std::string, std::string> &properties, const std::string &key)
{
    const auto it = properties.find(key);
    if (it == properties.end())
    {
        return std::nullopt;
    }

    try
    {
        return std::stod(it->second);
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

bool supports(const RuntimeDevice &device, const std::string &property)
{
    return parse_bool(device.properties, property).value_or(false);
}

std::vector<std::string> list_pointer_devices()
{
    const auto output = require_command_success(
        {"qdbus6", "org.kde.KWin", "/org/kde/KWin/InputDevice", "org.kde.KWin.InputDeviceManager.ListPointers"},
        "Failed to list KWin pointer devices");

    std::vector<std::string> devices;
    for (auto line : split_lines(output))
    {
        if (!line.empty())
        {
            devices.push_back(std::move(line));
        }
    }
    return devices;
}

RuntimeDevice load_runtime_device(const std::string &sys_name)
{
    const auto output = require_command_success(
        {"qdbus6", "org.kde.KWin", "/org/kde/KWin/InputDevice/" + sys_name, "org.freedesktop.DBus.Properties.GetAll",
         "org.kde.KWin.InputDevice"},
        "Failed to inspect KWin input device '" + sys_name + "'");

    RuntimeDevice device;
    device.sys_name = sys_name;

    for (const auto &line : split_lines(output))
    {
        const auto separator = line.find(':');
        if (separator == std::string::npos)
        {
            continue;
        }

        auto key = trim(line.substr(0, separator));
        auto value = trim(line.substr(separator + 1));
        if (!key.empty())
        {
            device.properties.emplace(std::move(key), std::move(value));
        }
    }

    return device;
}

std::vector<RuntimeDevice> discover_mouse_devices()
{
    std::vector<RuntimeDevice> devices;
    for (const auto &sys_name : list_pointer_devices())
    {
        auto device = load_runtime_device(sys_name);

        if (!parse_bool(device.properties, "pointer").value_or(false))
        {
            continue;
        }
        if (parse_bool(device.properties, "touchpad").value_or(false))
        {
            continue;
        }
        if (parse_bool(device.properties, "alphaNumericKeyboard").value_or(false))
        {
            continue;
        }
        if (parse_int(device.properties, "supportedButtons").value_or(0) <= 0)
        {
            continue;
        }

        devices.push_back(std::move(device));
    }

    return devices;
}

std::string determine_accel_profile(const RuntimeDevice &device)
{
    if (parse_bool(device.properties, "pointerAccelerationProfileFlat").value_or(false))
    {
        return "flat";
    }
    if (parse_bool(device.properties, "pointerAccelerationProfileAdaptive").value_or(false))
    {
        return "adaptive";
    }
    return "custom";
}

LinuxPointerDeviceSnapshot capture_device(const RuntimeDevice &device)
{
    LinuxPointerDeviceSnapshot snapshot;
    snapshot.name = device.properties.at("name");
    snapshot.vendor = parse_int(device.properties, "vendor").value_or(0);
    snapshot.product = parse_int(device.properties, "product").value_or(0);

    if (supports(device, "supportsLeftHanded"))
    {
        snapshot.left_handed = parse_bool(device.properties, "leftHanded");
    }
    if (supports(device, "supportsMiddleEmulation"))
    {
        snapshot.middle_emulation = parse_bool(device.properties, "middleEmulation");
    }
    if (supports(device, "supportsPointerAcceleration"))
    {
        snapshot.accel_speed = parse_double(device.properties, "pointerAcceleration");
    }
    if (supports(device, "supportsPointerAccelerationProfileFlat") ||
        supports(device, "supportsPointerAccelerationProfileAdaptive"))
    {
        snapshot.accel_profile = determine_accel_profile(device);
    }
    if (supports(device, "supportsNaturalScroll"))
    {
        snapshot.natural_scroll = parse_bool(device.properties, "naturalScroll");
    }

    snapshot.scroll_factor = parse_double(device.properties, "scrollFactor");

    if (supports(device, "supportsScrollOnButtonDown"))
    {
        snapshot.scroll_on_button_down = parse_bool(device.properties, "scrollOnButtonDown");
    }

    return snapshot;
}

std::optional<std::size_t> find_matching_device_index(const LinuxPointerDeviceSnapshot &target,
                                                      const std::vector<RuntimeDevice> &devices)
{
    for (std::size_t index = 0; index < devices.size(); ++index)
    {
        const auto &device = devices[index];
        if (parse_int(device.properties, "vendor").value_or(-1) == target.vendor &&
            parse_int(device.properties, "product").value_or(-1) == target.product &&
            device.properties.find("name") != device.properties.end() && device.properties.at("name") == target.name)
        {
            return index;
        }
    }

    std::optional<std::size_t> candidate;
    for (std::size_t index = 0; index < devices.size(); ++index)
    {
        const auto &device = devices[index];
        if (parse_int(device.properties, "vendor").value_or(-1) == target.vendor &&
            parse_int(device.properties, "product").value_or(-1) == target.product)
        {
            if (candidate.has_value())
            {
                return std::nullopt;
            }
            candidate = index;
        }
    }

    return candidate;
}

void set_bool_property(const std::string &sys_name, const std::string &property, bool value)
{
    require_command_success({"dbus-send",
                             "--session",
                             "--dest=org.kde.KWin",
                             "--print-reply=literal",
                             "/org/kde/KWin/InputDevice/" + sys_name,
                             "org.freedesktop.DBus.Properties.Set",
                             "string:org.kde.KWin.InputDevice",
                             "string:" + property,
                             std::string("variant:boolean:") + (value ? "true" : "false")},
                            "Failed to set KWin property '" + property + "' on device '" + sys_name + "'");
}

void set_double_property(const std::string &sys_name, const std::string &property, double value)
{
    require_command_success({"dbus-send",
                             "--session",
                             "--dest=org.kde.KWin",
                             "--print-reply=literal",
                             "/org/kde/KWin/InputDevice/" + sys_name,
                             "org.freedesktop.DBus.Properties.Set",
                             "string:org.kde.KWin.InputDevice",
                             "string:" + property,
                             "variant:double:" + std::to_string(value)},
                            "Failed to set KWin property '" + property + "' on device '" + sys_name + "'");
}

void validate_apply_compatibility(const LinuxPointerDeviceSnapshot &profile_device, const RuntimeDevice &runtime_device)
{
    if (profile_device.left_handed.has_value() && !supports(runtime_device, "supportsLeftHanded"))
    {
        throw std::runtime_error("Device '" + profile_device.name + "' does not support left-handed mode");
    }
    if (profile_device.middle_emulation.has_value() && !supports(runtime_device, "supportsMiddleEmulation"))
    {
        throw std::runtime_error("Device '" + profile_device.name + "' does not support middle emulation");
    }
    if (profile_device.accel_speed.has_value() && !supports(runtime_device, "supportsPointerAcceleration"))
    {
        throw std::runtime_error("Device '" + profile_device.name + "' does not support pointer acceleration");
    }
    if (profile_device.accel_profile.has_value())
    {
        if (*profile_device.accel_profile == "flat" && !supports(runtime_device, "supportsPointerAccelerationProfileFlat"))
        {
            throw std::runtime_error("Device '" + profile_device.name + "' does not support flat acceleration profile");
        }
        if (*profile_device.accel_profile == "adaptive" &&
            !supports(runtime_device, "supportsPointerAccelerationProfileAdaptive"))
        {
            throw std::runtime_error("Device '" + profile_device.name + "' does not support adaptive acceleration profile");
        }
        if (*profile_device.accel_profile == "custom")
        {
            throw std::runtime_error("Device '" + profile_device.name + "' uses unsupported accel_profile='custom'");
        }
    }
    if (profile_device.natural_scroll.has_value() && !supports(runtime_device, "supportsNaturalScroll"))
    {
        throw std::runtime_error("Device '" + profile_device.name + "' does not support natural scroll");
    }
    if (profile_device.scroll_on_button_down.has_value() && !supports(runtime_device, "supportsScrollOnButtonDown"))
    {
        throw std::runtime_error("Device '" + profile_device.name + "' does not support scroll-on-button-down");
    }
}

} // namespace

bool is_supported_environment()
{
    const std::string session_type = env_or_empty("XDG_SESSION_TYPE");
    const std::string current_desktop = env_or_empty("XDG_CURRENT_DESKTOP");
    const std::string desktop_session = env_or_empty("DESKTOP_SESSION");

    if (!contains_case_insensitive(session_type, "wayland"))
    {
        return false;
    }

    if (!contains_case_insensitive(current_desktop, "kde") && !contains_case_insensitive(desktop_session, "plasma"))
    {
        return false;
    }

    return run_command({"qdbus6", "org.kde.KWin", "/KWin", "org.freedesktop.DBus.Peer.Ping"}).exit_code == 0;
}

LinuxSnapshot capture()
{
    const auto devices = discover_mouse_devices();
    if (devices.empty())
    {
        throw std::runtime_error("No eligible KDE Wayland mouse devices were found via KWin DBus");
    }

    std::vector<LinuxPointerDeviceSnapshot> snapshots;
    snapshots.reserve(devices.size());
    for (const auto &device : devices)
    {
        snapshots.push_back(capture_device(device));
    }

    LinuxSnapshot snapshot;
    snapshot.backend = std::string{kKdeWaylandBackendId};
    snapshot.pointer_devices = snapshots;
    snapshot.accel_speed = snapshots.front().accel_speed;
    snapshot.accel_profile = snapshots.front().accel_profile;
    return snapshot;
}

void apply(const LinuxSnapshot &snapshot)
{
    if (!snapshot.pointer_devices.has_value() || snapshot.pointer_devices->empty())
    {
        throw std::runtime_error("kde-wayland apply requires pointer_devices in the Linux payload");
    }

    const auto runtime_devices = discover_mouse_devices();
    if (runtime_devices.empty())
    {
        throw std::runtime_error("No eligible KDE Wayland mouse devices were found via KWin DBus");
    }

    std::vector<std::pair<LinuxPointerDeviceSnapshot, RuntimeDevice>> matches;
    matches.reserve(snapshot.pointer_devices->size());

    for (const auto &profile_device : *snapshot.pointer_devices)
    {
        const auto match_index = find_matching_device_index(profile_device, runtime_devices);
        if (!match_index.has_value())
        {
            throw std::runtime_error("Could not find a unique KDE Wayland mouse device match for '" + profile_device.name +
                                     "' (vendor=" + std::to_string(profile_device.vendor) + ", product=" +
                                     std::to_string(profile_device.product) + ")");
        }

        validate_apply_compatibility(profile_device, runtime_devices[*match_index]);
        matches.emplace_back(profile_device, runtime_devices[*match_index]);
    }

    for (const auto &[profile_device, runtime_device] : matches)
    {
        if (profile_device.left_handed.has_value())
        {
            set_bool_property(runtime_device.sys_name, "leftHanded", *profile_device.left_handed);
        }
        if (profile_device.middle_emulation.has_value())
        {
            set_bool_property(runtime_device.sys_name, "middleEmulation", *profile_device.middle_emulation);
        }
        if (profile_device.accel_speed.has_value())
        {
            set_double_property(runtime_device.sys_name, "pointerAcceleration", *profile_device.accel_speed);
        }
        if (profile_device.accel_profile.has_value())
        {
            const bool flat = *profile_device.accel_profile == "flat";
            const bool adaptive = *profile_device.accel_profile == "adaptive";
            if (supports(runtime_device, "supportsPointerAccelerationProfileFlat"))
            {
                set_bool_property(runtime_device.sys_name, "pointerAccelerationProfileFlat", flat);
            }
            if (supports(runtime_device, "supportsPointerAccelerationProfileAdaptive"))
            {
                set_bool_property(runtime_device.sys_name, "pointerAccelerationProfileAdaptive", adaptive);
            }
        }
        if (profile_device.natural_scroll.has_value())
        {
            set_bool_property(runtime_device.sys_name, "naturalScroll", *profile_device.natural_scroll);
        }
        if (profile_device.scroll_factor.has_value())
        {
            set_double_property(runtime_device.sys_name, "scrollFactor", *profile_device.scroll_factor);
        }
        if (profile_device.scroll_on_button_down.has_value())
        {
            set_bool_property(runtime_device.sys_name, "scrollOnButtonDown", *profile_device.scroll_on_button_down);
        }
    }
}

} // namespace mouse_sync::linux::kde_wayland