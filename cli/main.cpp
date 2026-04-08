#include <mouse_sync/backend.hpp>
#include <mouse_sync/profile.hpp>

#include <CLI/CLI.hpp>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Utilities
// ─────────────────────────────────────────────────────────────────────────────

static std::string current_utc_iso8601()
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buf;
}

static std::string read_file(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
    {
        throw std::runtime_error("Cannot open file for reading: " + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void write_file(const std::string &path, const std::string &content)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open())
    {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    f << content;
}

static std::string join_strings(const std::vector<std::string> &values)
{
    if (values.empty())
    {
        return "(none on this platform)";
    }

    std::ostringstream ss;
    for (size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0)
        {
            ss << ", ";
        }
        ss << values[index];
    }

    return ss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON schema description
// ─────────────────────────────────────────────────────────────────────────────

static const char *kSchemaDescription = R"(mouse-sync JSON schema (version 1)
====================================

{
  "mouse_sync_version": string   // semver of the app that wrote this file
  "schema_version":     integer  // schema revision (currently 1)
  "created_at":         string   // ISO-8601 UTC timestamp
  "source_os":          string   // "windows" | "linux"
    "source_backend":     string   // e.g. "windows", "kde-wayland", "x11-cinnamon"

  "windows": {
    "mouse_speed":               integer  // SPI_GETMOUSESPEED: 1..20 (default 10)
    "enhance_pointer_precision": boolean  // true = EPP / mouse acceleration on
    "mouse_threshold1":          integer  // SPI_GETMOUSE [0] (default 6)
    "mouse_threshold2":          integer  // SPI_GETMOUSE [1] (default 10)
    "mouse_accel":               integer  // SPI_GETMOUSE [2] (0=off, 1=on)

        "registry": {                // HKCU\Control Panel\Mouse  (null if unavailable)
            "MouseSensitivity":   string  | null   // decimal string, e.g. "10"
      "MouseSpeed":         string  | null   // "0", "1", or "2"
      "MouseThreshold1":    string  | null   // decimal string, e.g. "6"
      "MouseThreshold2":    string  | null   // decimal string, e.g. "10"
      "SmoothMouseXCurve":  string  | null   // 40-byte EPP curve, hex-encoded
      "SmoothMouseYCurve":  string  | null   // 40-byte EPP curve, hex-encoded
    }
  }

    "linux": {                     // present only for Linux profiles
    "accel_speed":   number | null   // libinput accel speed: -1.0 .. 1.0
    "accel_profile": string | null   // "adaptive" | "flat" | "custom"
    "backend":       string | null   // e.g. "kde-wayland", "gnome", "x11"
  }
}
)";

// ─────────────────────────────────────────────────────────────────────────────
// Command implementations
// ─────────────────────────────────────────────────────────────────────────────

static int cmd_capture(const std::string &backend_id, const std::string &out_path)
{
    try
    {
        auto profile = mouse_sync::capture_profile(backend_id, current_utc_iso8601());
        std::string json = mouse_sync::profile_to_json_string(profile);
        write_file(out_path, json);
        std::cout << "Captured backend '" << backend_id << "' settings -> " << out_path << "\n";
        return 0;
    }
    catch (const mouse_sync::BackendError &ex)
    {
        std::cerr << "Backend error [" << ex.backend_id << "]: " << ex.what();
        if (ex.error_code.has_value())
        {
            std::cerr << " (code " << *ex.error_code << ")";
        }
        std::cerr << "\n";
        return 1;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

static int cmd_apply(const std::string &backend_id, const std::string &in_path)
{
    try
    {
        std::string json = read_file(in_path);
        auto profile = mouse_sync::profile_from_json_string(json);
        mouse_sync::apply_profile(backend_id, profile);
        std::cout << "Applied backend '" << backend_id << "' settings from " << in_path << "\n";
        return 0;
    }
    catch (const mouse_sync::BackendError &ex)
    {
        std::cerr << "Backend error [" << ex.backend_id << "]: " << ex.what();
        if (ex.error_code.has_value())
        {
            std::cerr << " (code " << *ex.error_code << ")";
        }
        std::cerr << "\n";
        return 1;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

static int cmd_print(const std::string &in_path)
{
    try
    {
        std::string json = read_file(in_path);
        auto profile = mouse_sync::profile_from_json_string(json);

        // Re-serialize with indentation for a canonical pretty-print.
        std::cout << mouse_sync::profile_to_json_string(profile) << "\n";
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char **argv)
{
    CLI::App app{"mouse-sync: synchronize mouse pointer settings between OSes"};
    app.set_version_flag("--version,-V", std::string("mouse-sync ") + MOUSE_SYNC_VERSION);

    const auto backend_help = std::string("Target backend (") + join_strings(mouse_sync::available_backends()) + ")";

    // ── capture ───────────────────────────────────────────────────────────────
    {
        auto *sub = app.add_subcommand("capture", "Capture mouse settings from the current OS");
        std::string backend_val;
        std::string out_val;
        sub->add_option("--backend,--os", backend_val, backend_help)->required();
        sub->add_option("--out", out_val, "Output JSON file path")->required();
        sub->callback([&backend_val, &out_val]() { std::exit(cmd_capture(backend_val, out_val)); });
    }

    // ── apply ─────────────────────────────────────────────────────────────────
    {
        auto *sub = app.add_subcommand("apply", "Apply mouse settings from a JSON file");
        std::string backend_val;
        std::string in_val;
        sub->add_option("--backend,--os", backend_val, backend_help)->required();
        sub->add_option("--in", in_val, "Input JSON file path")->required();
        sub->callback([&backend_val, &in_val]() { std::exit(cmd_apply(backend_val, in_val)); });
    }

    // ── print ─────────────────────────────────────────────────────────────────
    {
        auto *sub = app.add_subcommand("print", "Pretty-print a mouse-sync JSON profile");
        std::string in_val;
        sub->add_option("--in", in_val, "Input JSON file path")->required();
        sub->callback([&in_val]() { std::exit(cmd_print(in_val)); });
    }

    // ── schema ────────────────────────────────────────────────────────────────
    {
        auto *sub = app.add_subcommand("schema", "Print the JSON profile schema description");
        sub->callback([]() {
            std::cout << kSchemaDescription;
            std::exit(0);
        });
    }

    // ── version (subcommand form) ─────────────────────────────────────────────
    {
        auto *sub = app.add_subcommand("version", "Print version information");
        sub->callback([]() {
            std::cout << "mouse-sync " << MOUSE_SYNC_VERSION << "\n";
            std::cout << "schema version: " << mouse_sync::kSchemaVersion << "\n";
            std::cout << "backends: " << join_strings(mouse_sync::available_backends()) << "\n";
            std::exit(0);
        });
    }

    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);
    return 0;
}
