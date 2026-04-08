#include <mouse_sync/profile.hpp>

#ifdef MOUSE_SYNC_WINDOWS_BACKEND
#include "windows_backend.hpp"
#endif

#include <CLI/CLI.hpp>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Utilities
// ─────────────────────────────────────────────────────────────────────────────

static std::string current_utc_iso8601()
{
    auto now  = std::chrono::system_clock::now();
    auto tt   = std::chrono::system_clock::to_time_t(now);
    struct tm utc {};
#ifdef _WIN32
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buf;
}

static std::string read_file(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open file for reading: " + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void write_file(const std::string& path, const std::string& content)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    f << content;
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON schema description
// ─────────────────────────────────────────────────────────────────────────────

static const char* kSchemaDescription = R"(mouse-sync JSON schema (version 1)
====================================

{
  "mouse_sync_version": string   // semver of the app that wrote this file
  "schema_version":     integer  // schema revision (currently 1)
  "created_at":         string   // ISO-8601 UTC timestamp
  "source_os":          string   // "windows" | "linux"

  "windows": {
    "mouse_speed":               integer  // SPI_GETMOUSESPEED: 1..20 (default 10)
    "enhance_pointer_precision": boolean  // true = EPP / mouse acceleration on
    "mouse_threshold1":          integer  // SPI_GETMOUSE [0] (default 6)
    "mouse_threshold2":          integer  // SPI_GETMOUSE [1] (default 10)
    "mouse_accel":               integer  // SPI_GETMOUSE [2] (0=off, 1=on)

    "registry": {                // HKCU\Control Panel\Mouse  (null if unavailable)
      "MouseSensitivity":   integer | null
      "MouseSpeed":         string  | null   // "0", "1", or "2"
      "MouseThreshold1":    string  | null   // decimal string, e.g. "6"
      "MouseThreshold2":    string  | null   // decimal string, e.g. "10"
      "SmoothMouseXCurve":  string  | null   // 40-byte EPP curve, hex-encoded
      "SmoothMouseYCurve":  string  | null   // 40-byte EPP curve, hex-encoded
    }
  }

  "linux": {                     // placeholder — Linux backend not yet implemented
    "accel_speed":   number | null   // libinput accel speed: -1.0 .. 1.0
    "accel_profile": string | null   // "adaptive" | "flat" | "custom"
    "backend":       string | null   // e.g. "kde-wayland", "gnome", "x11"
  }
}
)";

// ─────────────────────────────────────────────────────────────────────────────
// Command implementations
// ─────────────────────────────────────────────────────────────────────────────

static int cmd_capture(const std::string& os, const std::string& out_path)
{
    if (os == "windows") {
#ifdef MOUSE_SYNC_WINDOWS_BACKEND
        try {
            mouse_sync::MouseProfile profile;
            profile.created_at = current_utc_iso8601();
            profile.source_os  = "windows";
            profile.windows    = mouse_sync::windows::capture();

            std::string json = mouse_sync::profile_to_json_string(profile);
            write_file(out_path, json);
            std::cout << "Captured Windows mouse settings -> " << out_path
                      << "\n";
            return 0;
        } catch (const mouse_sync::windows::WindowsError& ex) {
            std::cerr << "Windows API error: " << ex.what()
                      << " (code " << ex.error_code << ")\n";
            return 1;
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << "\n";
            return 1;
        }
#else
        std::cerr << "Error: Windows backend is not available on this platform.\n"
                  << "Build on Windows to enable the Windows backend.\n";
        return 1;
#endif
    }

    // ── Future OS backends ────────────────────────────────────────────────────
    if (os == "linux") {
        std::cerr << "Error: Linux backend is not yet implemented.\n";
        return 1;
    }

    std::cerr << "Error: Unknown OS '" << os
              << "'. Supported: windows (linux coming soon).\n";
    return 1;
}

static int cmd_apply(const std::string& os, const std::string& in_path)
{
    if (os == "windows") {
#ifdef MOUSE_SYNC_WINDOWS_BACKEND
        try {
            std::string json    = read_file(in_path);
            auto        profile = mouse_sync::profile_from_json_string(json);
            mouse_sync::windows::apply(profile.windows);
            std::cout << "Applied Windows mouse settings from " << in_path
                      << "\n";
            return 0;
        } catch (const mouse_sync::windows::WindowsError& ex) {
            std::cerr << "Windows API error: " << ex.what()
                      << " (code " << ex.error_code << ")\n";
            return 1;
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << "\n";
            return 1;
        }
#else
        std::cerr << "Error: Windows backend is not available on this platform.\n"
                  << "Build on Windows to enable the Windows backend.\n";
        return 1;
#endif
    }

    if (os == "linux") {
        std::cerr << "Error: Linux backend is not yet implemented.\n";
        return 1;
    }

    std::cerr << "Error: Unknown OS '" << os
              << "'. Supported: windows (linux coming soon).\n";
    return 1;
}

static int cmd_print(const std::string& in_path)
{
    try {
        std::string json    = read_file(in_path);
        auto        profile = mouse_sync::profile_from_json_string(json);

        // Re-serialize with indentation for a canonical pretty-print.
        std::cout << mouse_sync::profile_to_json_string(profile) << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    CLI::App app{"mouse-sync: synchronize mouse pointer settings between OSes"};
    app.set_version_flag("--version,-V",
                         std::string("mouse-sync ") + MOUSE_SYNC_VERSION);

    // ── capture ───────────────────────────────────────────────────────────────
    {
        auto*       sub    = app.add_subcommand("capture",
                                         "Capture mouse settings from the current OS");
        std::string os_val;
        std::string out_val;
        sub->add_option("--os", os_val, "Target OS (windows)")->required();
        sub->add_option("--out", out_val, "Output JSON file path")->required();
        sub->callback([&os_val, &out_val]() {
            std::exit(cmd_capture(os_val, out_val));
        });
    }

    // ── apply ─────────────────────────────────────────────────────────────────
    {
        auto*       sub    = app.add_subcommand("apply",
                                         "Apply mouse settings from a JSON file");
        std::string os_val;
        std::string in_val;
        sub->add_option("--os", os_val, "Target OS (windows)")->required();
        sub->add_option("--in", in_val, "Input JSON file path")->required();
        sub->callback([&os_val, &in_val]() {
            std::exit(cmd_apply(os_val, in_val));
        });
    }

    // ── print ─────────────────────────────────────────────────────────────────
    {
        auto*       sub   = app.add_subcommand("print",
                                        "Pretty-print a mouse-sync JSON profile");
        std::string in_val;
        sub->add_option("--in", in_val, "Input JSON file path")->required();
        sub->callback([&in_val]() {
            std::exit(cmd_print(in_val));
        });
    }

    // ── schema ────────────────────────────────────────────────────────────────
    {
        auto* sub = app.add_subcommand("schema",
                                       "Print the JSON profile schema description");
        sub->callback([]() {
            std::cout << kSchemaDescription;
            std::exit(0);
        });
    }

    // ── version (subcommand form) ─────────────────────────────────────────────
    {
        auto* sub = app.add_subcommand("version", "Print version information");
        sub->callback([]() {
            std::cout << "mouse-sync " << MOUSE_SYNC_VERSION << "\n";
            std::cout << "schema version: " << mouse_sync::kSchemaVersion << "\n";
#ifdef MOUSE_SYNC_WINDOWS_BACKEND
            std::cout << "backends: windows\n";
#else
            std::cout << "backends: (none on this platform)\n";
#endif
            std::exit(0);
        });
    }

    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);
    return 0;
}
