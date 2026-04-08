#pragma once

#include <nlohmann/json.hpp>

#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mouse_sync
{

inline constexpr const char *kAppVersion = "0.1.0";
inline constexpr int kSchemaVersion = 1;
inline constexpr const char *kMouseSyncVersion = "0.1";
inline constexpr const char *kWindowsBackendId = "windows";
inline constexpr const char *kLinuxOsId = "linux";
inline constexpr const char *kKdeWaylandBackendId = "kde-wayland";
inline constexpr const char *kX11CinnamonBackendId = "x11-cinnamon";

struct WindowsRegistrySnapshot
{
    std::optional<std::string> MouseSensitivity;
    std::optional<std::string> MouseSpeed;
    std::optional<std::string> MouseThreshold1;
    std::optional<std::string> MouseThreshold2;
    std::optional<std::string> SmoothMouseXCurve;
    std::optional<std::string> SmoothMouseYCurve;
};

struct WindowsSnapshot
{
    int mouse_speed{10};
    bool enhance_pointer_precision{false};
    int mouse_threshold1{6};
    int mouse_threshold2{10};
    int mouse_accel{0};
    std::optional<WindowsRegistrySnapshot> registry;
};

struct LinuxPointerDeviceSnapshot
{
    std::string name;
    int vendor{0};
    int product{0};
    std::optional<bool> left_handed;
    std::optional<bool> middle_emulation;
    std::optional<double> accel_speed;
    std::optional<std::string> accel_profile;
    std::optional<bool> natural_scroll;
    std::optional<double> scroll_factor;
    std::optional<bool> scroll_on_button_down;
};

struct LinuxSnapshot
{
    std::optional<double> accel_speed;
    std::optional<std::string> accel_profile;
    std::optional<std::string> backend;
    std::optional<std::vector<LinuxPointerDeviceSnapshot>> pointer_devices;
};

struct MouseProfile
{
    std::string mouse_sync_version{kMouseSyncVersion};
    int schema_version{kSchemaVersion};
    std::string created_at;
    std::string source_os;
    std::optional<std::string> source_backend;
    std::optional<WindowsSnapshot> windows;
    std::optional<LinuxSnapshot> linux;
};

namespace detail
{

template <typename T> void opt_to_json(nlohmann::json &j, const char *key, const std::optional<T> &value)
{
    if (value.has_value())
    {
        j[key] = *value;
    }
}

template <typename T> void opt_from_json(const nlohmann::json &j, const char *key, std::optional<T> &value)
{
    if (j.contains(key) && !j.at(key).is_null())
    {
        value = j.at(key).get<T>();
    }
}

inline bool is_decimal_string(const std::string &value)
{
    if (value.empty())
    {
        return false;
    }

    for (unsigned char ch : value)
    {
        if (!std::isdigit(ch))
        {
            return false;
        }
    }

    return true;
}

inline bool is_hex_string(const std::string &value)
{
    if (value.empty())
    {
        return false;
    }

    for (unsigned char ch : value)
    {
        if (!std::isxdigit(ch))
        {
            return false;
        }
    }

    return true;
}

inline bool has_linux_payload(const LinuxSnapshot &snapshot)
{
    return snapshot.accel_speed.has_value() || snapshot.accel_profile.has_value() || snapshot.backend.has_value() ||
           (snapshot.pointer_devices.has_value() && !snapshot.pointer_devices->empty());
}

template <typename T> void require_optional_range(const std::optional<T> &value, T min_value, T max_value, const char *field)
{
    if (!value.has_value())
    {
        return;
    }

    if (*value < min_value || *value > max_value)
    {
        throw std::runtime_error(std::string(field) + " out of range [" + std::to_string(min_value) + ".." +
                                 std::to_string(max_value) + "]");
    }
}

inline void validate_linux_pointer_device_snapshot(const LinuxPointerDeviceSnapshot &snapshot)
{
    if (snapshot.name.empty())
    {
        throw std::runtime_error("linux pointer device name must not be empty");
    }
    if (snapshot.vendor < 0)
    {
        throw std::runtime_error("linux pointer device vendor must be >= 0");
    }
    if (snapshot.product < 0)
    {
        throw std::runtime_error("linux pointer device product must be >= 0");
    }

    require_optional_range(snapshot.accel_speed, -1.0, 1.0, "linux pointer device accel_speed");
    require_optional_range(snapshot.scroll_factor, 0.0, 20.0, "linux pointer device scroll_factor");

    if (snapshot.accel_profile.has_value())
    {
        const auto &value = *snapshot.accel_profile;
        if (value != "adaptive" && value != "flat" && value != "custom")
        {
            throw std::runtime_error("linux pointer device accel_profile must be one of: adaptive, flat, custom");
        }
    }
}

inline void validate_windows_registry_snapshot(const WindowsRegistrySnapshot &snapshot)
{
    if (snapshot.MouseSensitivity.has_value() && !is_decimal_string(*snapshot.MouseSensitivity))
    {
        throw std::runtime_error("Windows registry field MouseSensitivity must be a decimal string");
    }

    if (snapshot.MouseSpeed.has_value())
    {
        const auto &value = *snapshot.MouseSpeed;
        if (value != "0" && value != "1" && value != "2")
        {
            throw std::runtime_error("Windows registry field MouseSpeed must be one of: 0, 1, 2");
        }
    }

    if (snapshot.MouseThreshold1.has_value() && !is_decimal_string(*snapshot.MouseThreshold1))
    {
        throw std::runtime_error("Windows registry field MouseThreshold1 must be a decimal string");
    }

    if (snapshot.MouseThreshold2.has_value() && !is_decimal_string(*snapshot.MouseThreshold2))
    {
        throw std::runtime_error("Windows registry field MouseThreshold2 must be a decimal string");
    }

    const auto validate_curve = [](const char *field_name, const std::optional<std::string> &value) {
        if (!value.has_value())
        {
            return;
        }

        if (value->size() != 80 || !is_hex_string(*value))
        {
            throw std::runtime_error(std::string("Windows registry field ") + field_name +
                                     " must be an 80-character hex string");
        }
    };

    validate_curve("SmoothMouseXCurve", snapshot.SmoothMouseXCurve);
    validate_curve("SmoothMouseYCurve", snapshot.SmoothMouseYCurve);
}

inline void validate_windows_snapshot(const WindowsSnapshot &snapshot)
{
    if (snapshot.mouse_speed < 1 || snapshot.mouse_speed > 20)
    {
        throw std::runtime_error("mouse_speed out of range [1..20]");
    }
    if (snapshot.mouse_threshold1 < 0)
    {
        throw std::runtime_error("mouse_threshold1 must be >= 0");
    }
    if (snapshot.mouse_threshold2 < 0)
    {
        throw std::runtime_error("mouse_threshold2 must be >= 0");
    }
    if (snapshot.mouse_accel < 0 || snapshot.mouse_accel > 2)
    {
        throw std::runtime_error("mouse_accel out of range [0..2]");
    }

    if (snapshot.registry.has_value())
    {
        validate_windows_registry_snapshot(*snapshot.registry);
    }
}

inline void validate_linux_snapshot(const LinuxSnapshot &snapshot)
{
    require_optional_range(snapshot.accel_speed, -1.0, 1.0, "linux accel_speed");

    if (snapshot.accel_profile.has_value())
    {
        const auto &value = *snapshot.accel_profile;
        if (value != "adaptive" && value != "flat" && value != "custom")
        {
            throw std::runtime_error("linux accel_profile must be one of: adaptive, flat, custom");
        }
    }

    if (snapshot.backend.has_value() && snapshot.backend->empty())
    {
        throw std::runtime_error("linux backend must not be empty");
    }

    if (snapshot.pointer_devices.has_value())
    {
        if (snapshot.pointer_devices->empty())
        {
            throw std::runtime_error("linux pointer_devices must not be empty when present");
        }

        for (const auto &device : *snapshot.pointer_devices)
        {
            validate_linux_pointer_device_snapshot(device);
        }
    }
}

inline void validate_profile_metadata(const MouseProfile &profile)
{
    if (profile.mouse_sync_version.empty())
    {
        throw std::runtime_error("mouse_sync_version must not be empty");
    }
    if (profile.schema_version != kSchemaVersion)
    {
        throw std::runtime_error("Unsupported schema_version: expected " + std::to_string(kSchemaVersion) + ", got " +
                                 std::to_string(profile.schema_version));
    }
    if (profile.created_at.empty())
    {
        throw std::runtime_error("created_at must not be empty");
    }
    if (profile.source_os != kWindowsBackendId && profile.source_os != kLinuxOsId)
    {
        throw std::runtime_error("source_os must be one of: windows, linux");
    }
    if (profile.source_backend.has_value() && profile.source_backend->empty())
    {
        throw std::runtime_error("source_backend must not be empty when present");
    }
}

} // namespace detail

inline void to_json(nlohmann::json &j, const WindowsRegistrySnapshot &snapshot)
{
    j = nlohmann::json::object();
    detail::opt_to_json(j, "MouseSensitivity", snapshot.MouseSensitivity);
    detail::opt_to_json(j, "MouseSpeed", snapshot.MouseSpeed);
    detail::opt_to_json(j, "MouseThreshold1", snapshot.MouseThreshold1);
    detail::opt_to_json(j, "MouseThreshold2", snapshot.MouseThreshold2);
    detail::opt_to_json(j, "SmoothMouseXCurve", snapshot.SmoothMouseXCurve);
    detail::opt_to_json(j, "SmoothMouseYCurve", snapshot.SmoothMouseYCurve);
}

inline void from_json(const nlohmann::json &j, WindowsRegistrySnapshot &snapshot)
{
    detail::opt_from_json(j, "MouseSensitivity", snapshot.MouseSensitivity);
    detail::opt_from_json(j, "MouseSpeed", snapshot.MouseSpeed);
    detail::opt_from_json(j, "MouseThreshold1", snapshot.MouseThreshold1);
    detail::opt_from_json(j, "MouseThreshold2", snapshot.MouseThreshold2);
    detail::opt_from_json(j, "SmoothMouseXCurve", snapshot.SmoothMouseXCurve);
    detail::opt_from_json(j, "SmoothMouseYCurve", snapshot.SmoothMouseYCurve);
}

inline void to_json(nlohmann::json &j, const WindowsSnapshot &snapshot)
{
    j = {
        {"mouse_speed", snapshot.mouse_speed},
        {"enhance_pointer_precision", snapshot.enhance_pointer_precision},
        {"mouse_threshold1", snapshot.mouse_threshold1},
        {"mouse_threshold2", snapshot.mouse_threshold2},
        {"mouse_accel", snapshot.mouse_accel},
    };

    if (snapshot.registry.has_value())
    {
        j["registry"] = *snapshot.registry;
    }
    else
    {
        j["registry"] = nullptr;
    }
}

inline void from_json(const nlohmann::json &j, WindowsSnapshot &snapshot)
{
    if (j.contains("mouse_speed"))
    {
        snapshot.mouse_speed = j.at("mouse_speed").get<int>();
    }
    if (j.contains("enhance_pointer_precision"))
    {
        snapshot.enhance_pointer_precision = j.at("enhance_pointer_precision").get<bool>();
    }
    if (j.contains("mouse_threshold1"))
    {
        snapshot.mouse_threshold1 = j.at("mouse_threshold1").get<int>();
    }
    if (j.contains("mouse_threshold2"))
    {
        snapshot.mouse_threshold2 = j.at("mouse_threshold2").get<int>();
    }
    if (j.contains("mouse_accel"))
    {
        snapshot.mouse_accel = j.at("mouse_accel").get<int>();
    }
    if (j.contains("registry") && !j.at("registry").is_null())
    {
        snapshot.registry = j.at("registry").get<WindowsRegistrySnapshot>();
    }
}

inline void to_json(nlohmann::json &j, const LinuxSnapshot &snapshot)
{
    j = nlohmann::json::object();
    detail::opt_to_json(j, "accel_speed", snapshot.accel_speed);
    detail::opt_to_json(j, "accel_profile", snapshot.accel_profile);
    detail::opt_to_json(j, "backend", snapshot.backend);
    detail::opt_to_json(j, "pointer_devices", snapshot.pointer_devices);
}

inline void from_json(const nlohmann::json &j, LinuxSnapshot &snapshot)
{
    detail::opt_from_json(j, "accel_speed", snapshot.accel_speed);
    detail::opt_from_json(j, "accel_profile", snapshot.accel_profile);
    detail::opt_from_json(j, "backend", snapshot.backend);
    detail::opt_from_json(j, "pointer_devices", snapshot.pointer_devices);
}

inline void to_json(nlohmann::json &j, const LinuxPointerDeviceSnapshot &snapshot)
{
    j = {
        {"name", snapshot.name},
        {"vendor", snapshot.vendor},
        {"product", snapshot.product},
    };

    detail::opt_to_json(j, "left_handed", snapshot.left_handed);
    detail::opt_to_json(j, "middle_emulation", snapshot.middle_emulation);
    detail::opt_to_json(j, "accel_speed", snapshot.accel_speed);
    detail::opt_to_json(j, "accel_profile", snapshot.accel_profile);
    detail::opt_to_json(j, "natural_scroll", snapshot.natural_scroll);
    detail::opt_to_json(j, "scroll_factor", snapshot.scroll_factor);
    detail::opt_to_json(j, "scroll_on_button_down", snapshot.scroll_on_button_down);
}

inline void from_json(const nlohmann::json &j, LinuxPointerDeviceSnapshot &snapshot)
{
    if (j.contains("name"))
    {
        snapshot.name = j.at("name").get<std::string>();
    }
    if (j.contains("vendor"))
    {
        snapshot.vendor = j.at("vendor").get<int>();
    }
    if (j.contains("product"))
    {
        snapshot.product = j.at("product").get<int>();
    }

    detail::opt_from_json(j, "left_handed", snapshot.left_handed);
    detail::opt_from_json(j, "middle_emulation", snapshot.middle_emulation);
    detail::opt_from_json(j, "accel_speed", snapshot.accel_speed);
    detail::opt_from_json(j, "accel_profile", snapshot.accel_profile);
    detail::opt_from_json(j, "natural_scroll", snapshot.natural_scroll);
    detail::opt_from_json(j, "scroll_factor", snapshot.scroll_factor);
    detail::opt_from_json(j, "scroll_on_button_down", snapshot.scroll_on_button_down);
}

inline void to_json(nlohmann::json &j, const MouseProfile &profile)
{
    j = {
        {"mouse_sync_version", profile.mouse_sync_version},
        {"schema_version", profile.schema_version},
        {"created_at", profile.created_at},
        {"source_os", profile.source_os},
    };

    detail::opt_to_json(j, "source_backend", profile.source_backend);
    detail::opt_to_json(j, "windows", profile.windows);

    if (profile.linux.has_value() && detail::has_linux_payload(*profile.linux))
    {
        j["linux"] = *profile.linux;
    }
}

inline void from_json(const nlohmann::json &j, MouseProfile &profile)
{
    if (j.contains("mouse_sync_version"))
    {
        profile.mouse_sync_version = j.at("mouse_sync_version").get<std::string>();
    }
    if (j.contains("schema_version"))
    {
        profile.schema_version = j.at("schema_version").get<int>();
    }
    if (j.contains("created_at"))
    {
        profile.created_at = j.at("created_at").get<std::string>();
    }
    if (j.contains("source_os"))
    {
        profile.source_os = j.at("source_os").get<std::string>();
    }

    detail::opt_from_json(j, "source_backend", profile.source_backend);

    if (j.contains("windows") && !j.at("windows").is_null())
    {
        profile.windows = j.at("windows").get<WindowsSnapshot>();
    }
    if (j.contains("linux") && !j.at("linux").is_null())
    {
        profile.linux = j.at("linux").get<LinuxSnapshot>();
    }
}

inline void validate_profile(const MouseProfile &profile)
{
    detail::validate_profile_metadata(profile);

    if (profile.windows.has_value())
    {
        detail::validate_windows_snapshot(*profile.windows);
    }
    if (profile.linux.has_value())
    {
        detail::validate_linux_snapshot(*profile.linux);
    }

    if (profile.source_os == kWindowsBackendId)
    {
        if (!profile.windows.has_value())
        {
            throw std::runtime_error("windows profile is missing required windows payload");
        }
        if (profile.source_backend.has_value() && *profile.source_backend != kWindowsBackendId)
        {
            throw std::runtime_error("windows profile must use source_backend='windows'");
        }
        if (profile.linux.has_value() && detail::has_linux_payload(*profile.linux))
        {
            throw std::runtime_error("windows profile must not contain a populated linux payload");
        }
        return;
    }

    if (!profile.linux.has_value())
    {
        throw std::runtime_error("linux profile is missing required linux payload");
    }
    if (profile.windows.has_value())
    {
        throw std::runtime_error("linux profile must not contain a windows payload");
    }

    if (profile.source_backend.has_value())
    {
        if (profile.linux->backend.has_value() && *profile.linux->backend != *profile.source_backend)
        {
            throw std::runtime_error("linux payload backend must match source_backend");
        }

        if (*profile.source_backend == kKdeWaylandBackendId)
        {
            if (!profile.linux->pointer_devices.has_value() || profile.linux->pointer_devices->empty())
            {
                throw std::runtime_error("kde-wayland profile must contain at least one linux pointer device snapshot");
            }
        }
    }
}

inline void validate_profile_for_apply(const MouseProfile &profile, const std::string &target_backend)
{
    validate_profile(profile);

    if (target_backend == kWindowsBackendId)
    {
        if (profile.source_os != kWindowsBackendId)
        {
            throw std::runtime_error("refusing to apply a non-Windows profile using the windows backend");
        }
        if (profile.source_backend.has_value() && *profile.source_backend != kWindowsBackendId)
        {
            throw std::runtime_error("refusing to apply a profile captured by a different backend");
        }
        return;
    }

    if (target_backend == kKdeWaylandBackendId)
    {
        if (profile.source_os != kLinuxOsId)
        {
            throw std::runtime_error("refusing to apply a non-Linux profile using the kde-wayland backend");
        }
        if (!profile.source_backend.has_value() || *profile.source_backend != kKdeWaylandBackendId)
        {
            throw std::runtime_error("refusing to apply a profile captured by a different backend");
        }
        if (!profile.linux.has_value() || !profile.linux->pointer_devices.has_value() ||
            profile.linux->pointer_devices->empty())
        {
            throw std::runtime_error("kde-wayland apply requires linux pointer_devices payload");
        }

        for (const auto &device : *profile.linux->pointer_devices)
        {
            if (device.accel_profile.has_value() && *device.accel_profile == "custom")
            {
                throw std::runtime_error("kde-wayland backend does not support accel_profile='custom'");
            }
        }
        return;
    }

    if (profile.source_backend.has_value() && *profile.source_backend != target_backend)
    {
        throw std::runtime_error("refusing to apply a profile captured by a different backend");
    }
}

inline std::string profile_to_json_string(const MouseProfile &profile)
{
    return nlohmann::json(profile).dump(4);
}

inline MouseProfile profile_from_json_string(const std::string &value)
{
    nlohmann::json json;
    try
    {
        json = nlohmann::json::parse(value);
    }
    catch (const nlohmann::json::exception &ex)
    {
        throw std::runtime_error(std::string("JSON parse error: ") + ex.what());
    }

    MouseProfile profile;
    try
    {
        profile = json.get<MouseProfile>();
    }
    catch (const nlohmann::json::exception &ex)
    {
        throw std::runtime_error(std::string("JSON schema error: ") + ex.what());
    }

    validate_profile(profile);
    return profile;
}

} // namespace mouse_sync