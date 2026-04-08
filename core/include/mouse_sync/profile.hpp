#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mouse_sync {

// ── Application version ───────────────────────────────────────────────────────
inline constexpr const char* kAppVersion       = "0.1.0";
inline constexpr int         kSchemaVersion    = 1;
inline constexpr const char* kMouseSyncVersion = "0.1";

// ─────────────────────────────────────────────────────────────────────────────
// Windows snapshot
// ─────────────────────────────────────────────────────────────────────────────

/// Registry values from HKCU\Control Panel\Mouse.
/// Binary registry values (curve data) are stored as hex strings.
struct WindowsRegistrySnapshot {
    std::optional<int>         MouseSensitivity;
    std::optional<std::string> MouseSpeed;
    std::optional<std::string> MouseThreshold1;
    std::optional<std::string> MouseThreshold2;
    /// 40-byte binary curve data, stored as lowercase hex string (80 chars).
    std::optional<std::string> SmoothMouseXCurve;
    std::optional<std::string> SmoothMouseYCurve;
};

/// Full Windows mouse settings snapshot.
struct WindowsSnapshot {
    /// Value from SPI_GETMOUSESPEED: 1 (slowest) .. 20 (fastest). Default 10.
    int mouse_speed{10};

    /// Whether Enhance Pointer Precision is enabled (SPI_GETMOUSE accel flag).
    bool enhance_pointer_precision{false};

    /// Threshold 1 from SPI_GETMOUSE array[0]. Default 6.
    int mouse_threshold1{6};

    /// Threshold 2 from SPI_GETMOUSE array[1]. Default 10.
    int mouse_threshold2{10};

    /// Acceleration flag from SPI_GETMOUSE array[2]. 0 = off, 1 = on, 2 = on.
    int mouse_accel{0};

    /// Raw registry snapshot (best-effort; may be absent if access denied).
    std::optional<WindowsRegistrySnapshot> registry;
};

// ─────────────────────────────────────────────────────────────────────────────
// Linux snapshot (placeholder — backends not yet implemented)
// ─────────────────────────────────────────────────────────────────────────────

struct LinuxSnapshot {
    /// libinput accel speed, typically -1.0 (slowest) .. 1.0 (fastest).
    std::optional<double>      accel_speed;

    /// libinput accel profile: "adaptive", "flat", or "custom".
    std::optional<std::string> accel_profile;

    /// KDE / gsettings backend identifier used when capturing.
    std::optional<std::string> backend;
};

// ─────────────────────────────────────────────────────────────────────────────
// Top-level profile
// ─────────────────────────────────────────────────────────────────────────────

struct MouseProfile {
    /// Semver string of the application that wrote this file.
    std::string mouse_sync_version{kMouseSyncVersion};

    /// Integer version of the JSON schema, incremented on breaking changes.
    int schema_version{kSchemaVersion};

    /// ISO-8601 UTC timestamp when this profile was captured.
    std::string created_at;

    /// OS that produced this profile: "windows" | "linux".
    std::string source_os;

    WindowsSnapshot windows;
    LinuxSnapshot   linux;
};

// ─────────────────────────────────────────────────────────────────────────────
// JSON serialization helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace detail {

/// Write an optional value to a JSON object only when it has a value.
template <typename T>
void opt_to_json(nlohmann::json& j, const char* key,
                 const std::optional<T>& val)
{
    if (val.has_value()) {
        j[key] = *val;
    }
}

/// Read an optional value from a JSON object; leave unchanged if key absent.
template <typename T>
void opt_from_json(const nlohmann::json& j, const char* key,
                   std::optional<T>& val)
{
    if (j.contains(key) && !j.at(key).is_null()) {
        val = j.at(key).get<T>();
    }
}

} // namespace detail

// WindowsRegistrySnapshot ─────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const WindowsRegistrySnapshot& r)
{
    j = nlohmann::json::object();
    detail::opt_to_json(j, "MouseSensitivity",   r.MouseSensitivity);
    detail::opt_to_json(j, "MouseSpeed",         r.MouseSpeed);
    detail::opt_to_json(j, "MouseThreshold1",    r.MouseThreshold1);
    detail::opt_to_json(j, "MouseThreshold2",    r.MouseThreshold2);
    detail::opt_to_json(j, "SmoothMouseXCurve",  r.SmoothMouseXCurve);
    detail::opt_to_json(j, "SmoothMouseYCurve",  r.SmoothMouseYCurve);
}

inline void from_json(const nlohmann::json& j, WindowsRegistrySnapshot& r)
{
    detail::opt_from_json(j, "MouseSensitivity",  r.MouseSensitivity);
    detail::opt_from_json(j, "MouseSpeed",        r.MouseSpeed);
    detail::opt_from_json(j, "MouseThreshold1",   r.MouseThreshold1);
    detail::opt_from_json(j, "MouseThreshold2",   r.MouseThreshold2);
    detail::opt_from_json(j, "SmoothMouseXCurve", r.SmoothMouseXCurve);
    detail::opt_from_json(j, "SmoothMouseYCurve", r.SmoothMouseYCurve);
}

// WindowsSnapshot ─────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const WindowsSnapshot& s)
{
    j = {
        {"mouse_speed",               s.mouse_speed},
        {"enhance_pointer_precision", s.enhance_pointer_precision},
        {"mouse_threshold1",          s.mouse_threshold1},
        {"mouse_threshold2",          s.mouse_threshold2},
        {"mouse_accel",               s.mouse_accel},
    };
    if (s.registry.has_value()) {
        j["registry"] = *s.registry;
    } else {
        j["registry"] = nullptr;
    }
}

inline void from_json(const nlohmann::json& j, WindowsSnapshot& s)
{
    if (j.contains("mouse_speed"))               s.mouse_speed               = j.at("mouse_speed").get<int>();
    if (j.contains("enhance_pointer_precision"))  s.enhance_pointer_precision = j.at("enhance_pointer_precision").get<bool>();
    if (j.contains("mouse_threshold1"))           s.mouse_threshold1          = j.at("mouse_threshold1").get<int>();
    if (j.contains("mouse_threshold2"))           s.mouse_threshold2          = j.at("mouse_threshold2").get<int>();
    if (j.contains("mouse_accel"))                s.mouse_accel               = j.at("mouse_accel").get<int>();
    if (j.contains("registry") && !j.at("registry").is_null()) {
        s.registry = j.at("registry").get<WindowsRegistrySnapshot>();
    }
}

// LinuxSnapshot ───────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const LinuxSnapshot& s)
{
    j = nlohmann::json::object();
    detail::opt_to_json(j, "accel_speed",    s.accel_speed);
    detail::opt_to_json(j, "accel_profile",  s.accel_profile);
    detail::opt_to_json(j, "backend",        s.backend);
}

inline void from_json(const nlohmann::json& j, LinuxSnapshot& s)
{
    detail::opt_from_json(j, "accel_speed",   s.accel_speed);
    detail::opt_from_json(j, "accel_profile", s.accel_profile);
    detail::opt_from_json(j, "backend",       s.backend);
}

// MouseProfile ────────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const MouseProfile& p)
{
    j = {
        {"mouse_sync_version", p.mouse_sync_version},
        {"schema_version",     p.schema_version},
        {"created_at",         p.created_at},
        {"source_os",          p.source_os},
        {"windows",            p.windows},
        {"linux",              p.linux},
    };
}

inline void from_json(const nlohmann::json& j, MouseProfile& p)
{
    if (j.contains("mouse_sync_version")) p.mouse_sync_version = j.at("mouse_sync_version").get<std::string>();
    if (j.contains("schema_version"))     p.schema_version     = j.at("schema_version").get<int>();
    if (j.contains("created_at"))         p.created_at         = j.at("created_at").get<std::string>();
    if (j.contains("source_os"))          p.source_os          = j.at("source_os").get<std::string>();
    if (j.contains("windows"))            p.windows            = j.at("windows").get<WindowsSnapshot>();
    if (j.contains("linux"))              p.linux              = j.at("linux").get<LinuxSnapshot>();
}

// ─────────────────────────────────────────────────────────────────────────────
// Profile I/O helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Serialize a MouseProfile to a pretty-printed JSON string.
inline std::string profile_to_json_string(const MouseProfile& p)
{
    return nlohmann::json(p).dump(4);
}

/// Deserialize a MouseProfile from a JSON string.
/// Throws std::runtime_error on parse or validation errors.
inline MouseProfile profile_from_json_string(const std::string& s)
{
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(s);
    } catch (const nlohmann::json::exception& ex) {
        throw std::runtime_error(std::string("JSON parse error: ") + ex.what());
    }

    MouseProfile p;
    try {
        p = j.get<MouseProfile>();
    } catch (const nlohmann::json::exception& ex) {
        throw std::runtime_error(std::string("JSON schema error: ") + ex.what());
    }
    return p;
}

} // namespace mouse_sync
