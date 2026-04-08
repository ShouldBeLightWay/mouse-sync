#include "windows_backend.hpp"

#include <windows.h>
#include <winreg.h>

#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace mouse_sync::windows
{

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace
{

/// Convert a null-terminated wide string to a UTF-8 std::string.
std::string wchar_to_utf8(const WCHAR *wstr)
{
    if (!wstr || *wstr == L'\0')
        return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
        return {};
    std::string result(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.data(), needed, nullptr, nullptr);
    return result;
}

/// Format a Windows error code as a human-readable string.
std::string format_win_error(DWORD code)
{
    LPWSTR buf = nullptr;
    DWORD len =
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, code, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);

    std::string result;
    if (len > 0 && buf)
    {
        result = wchar_to_utf8(buf);
        LocalFree(buf);
        // Trim trailing whitespace / CRLF added by FormatMessage.
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
        {
            result.pop_back();
        }
    }
    if (result.empty())
    {
        result = "error " + std::to_string(code);
    }
    return result;
}

/// Throw WindowsError using GetLastError().
[[noreturn]] void throw_last_error(const std::string &context)
{
    DWORD code = GetLastError();
    throw WindowsError(context + ": " + format_win_error(code), code);
}

/// Throw WindowsError using an explicit LSTATUS / DWORD error code.
[[noreturn]] void throw_win_error(const std::string &context, DWORD code)
{
    throw WindowsError(context + ": " + format_win_error(code), code);
}

// ── Hex encoding ──────────────────────────────────────────────────────────────

std::string bytes_to_hex(const std::vector<BYTE> &data)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : data)
    {
        oss << std::setw(2) << static_cast<unsigned>(b);
    }
    return oss.str();
}

std::vector<BYTE> hex_to_bytes(const std::string &hex)
{
    if (hex.size() % 2 != 0)
    {
        throw std::runtime_error("hex string has odd length");
    }
    std::vector<BYTE> data;
    data.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2)
    {
        unsigned val = 0;
        std::istringstream iss(hex.substr(i, 2));
        iss >> std::hex >> val;
        if (iss.fail())
        {
            throw std::runtime_error("invalid hex byte: " + hex.substr(i, 2));
        }
        data.push_back(static_cast<BYTE>(val));
    }
    return data;
}

bool is_decimal_string(const std::string &value)
{
    if (value.empty())
    {
        return false;
    }
    for (unsigned char ch : value)
    {
        if (ch < '0' || ch > '9')
        {
            return false;
        }
    }
    return true;
}

struct PreparedRegistrySnapshot
{
    std::optional<std::string> MouseSensitivity;
    std::optional<std::string> MouseSpeed;
    std::optional<std::string> MouseThreshold1;
    std::optional<std::string> MouseThreshold2;
    std::optional<std::vector<BYTE>> SmoothMouseXCurve;
    std::optional<std::vector<BYTE>> SmoothMouseYCurve;
};

// ── Registry helpers ──────────────────────────────────────────────────────────

constexpr WCHAR kMouseKey[] = L"Control Panel\\Mouse";

/// Read a REG_SZ value as std::string (UTF-8).  Returns empty optional if
/// the value does not exist; throws on other errors.
std::optional<std::string> reg_read_sz(HKEY hKey, const WCHAR *name)
{
    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS st = RegQueryValueExW(hKey, name, nullptr, &type, nullptr, &bytes);
    if (st == ERROR_FILE_NOT_FOUND)
    {
        return std::nullopt;
    }
    if (st != ERROR_SUCCESS)
    {
        throw_win_error("RegQueryValueExW (size probe) for " + wchar_to_utf8(name), static_cast<DWORD>(st));
    }
    if (type != REG_SZ)
    {
        return std::nullopt; // unexpected type — skip gracefully
    }

    std::vector<WCHAR> buf(bytes / sizeof(WCHAR) + 1, L'\0');
    st = RegQueryValueExW(hKey, name, nullptr, &type, reinterpret_cast<LPBYTE>(buf.data()), &bytes);
    if (st != ERROR_SUCCESS)
    {
        throw_win_error("RegQueryValueExW for " + wchar_to_utf8(name), static_cast<DWORD>(st));
    }

    // Convert to UTF-8
    int needed = WideCharToMultiByte(CP_UTF8, 0, buf.data(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
    {
        return std::string{};
    }
    std::string result(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buf.data(), -1, result.data(), needed, nullptr, nullptr);
    return result;
}

/// Read a REG_BINARY value as hex string.  Returns empty optional if absent.
std::optional<std::string> reg_read_binary_hex(HKEY hKey, const WCHAR *name)
{
    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS st = RegQueryValueExW(hKey, name, nullptr, &type, nullptr, &bytes);
    if (st == ERROR_FILE_NOT_FOUND)
    {
        return std::nullopt;
    }
    if (st != ERROR_SUCCESS)
    {
        throw_win_error("RegQueryValueExW (size probe) for " + wchar_to_utf8(name), static_cast<DWORD>(st));
    }
    if (type != REG_BINARY)
    {
        return std::nullopt;
    }

    std::vector<BYTE> buf(bytes);
    st = RegQueryValueExW(hKey, name, nullptr, &type, buf.data(), &bytes);
    if (st != ERROR_SUCCESS)
    {
        throw_win_error("RegQueryValueExW for " + wchar_to_utf8(name), static_cast<DWORD>(st));
    }
    return bytes_to_hex(buf);
}

/// Write a REG_SZ value (UTF-8 → wide).
void reg_write_sz(HKEY hKey, const WCHAR *name, const std::string &value)
{
    int wlen = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::vector<WCHAR> wbuf(static_cast<size_t>(wlen));
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wbuf.data(), wlen);

    DWORD bytes = static_cast<DWORD>(wbuf.size() * sizeof(WCHAR));
    LSTATUS st = RegSetValueExW(hKey, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(wbuf.data()), bytes);
    if (st != ERROR_SUCCESS)
    {
        throw_win_error("RegSetValueExW for " + wchar_to_utf8(name), static_cast<DWORD>(st));
    }
}

/// Write a REG_BINARY value.
void reg_write_binary(HKEY hKey, const WCHAR *name, const std::vector<BYTE> &data)
{
    LSTATUS st = RegSetValueExW(hKey, name, 0, REG_BINARY, data.data(), static_cast<DWORD>(data.size()));
    if (st != ERROR_SUCCESS)
    {
        throw_win_error("RegSetValueExW for " + wchar_to_utf8(name), static_cast<DWORD>(st));
    }
}

std::optional<PreparedRegistrySnapshot> prepare_registry_snapshot(
    const std::optional<WindowsRegistrySnapshot> &snapshot)
{
    if (!snapshot.has_value())
    {
        return std::nullopt;
    }

    PreparedRegistrySnapshot prepared;

    if (snapshot->MouseSensitivity.has_value())
    {
        if (!is_decimal_string(*snapshot->MouseSensitivity))
        {
            throw std::runtime_error("MouseSensitivity must be a decimal string");
        }
        prepared.MouseSensitivity = snapshot->MouseSensitivity;
    }

    if (snapshot->MouseSpeed.has_value())
    {
        const auto &value = *snapshot->MouseSpeed;
        if (value != "0" && value != "1" && value != "2")
        {
            throw std::runtime_error("MouseSpeed must be one of: 0, 1, 2");
        }
        prepared.MouseSpeed = value;
    }

    if (snapshot->MouseThreshold1.has_value())
    {
        if (!is_decimal_string(*snapshot->MouseThreshold1))
        {
            throw std::runtime_error("MouseThreshold1 must be a decimal string");
        }
        prepared.MouseThreshold1 = snapshot->MouseThreshold1;
    }

    if (snapshot->MouseThreshold2.has_value())
    {
        if (!is_decimal_string(*snapshot->MouseThreshold2))
        {
            throw std::runtime_error("MouseThreshold2 must be a decimal string");
        }
        prepared.MouseThreshold2 = snapshot->MouseThreshold2;
    }

    if (snapshot->SmoothMouseXCurve.has_value())
    {
        prepared.SmoothMouseXCurve = hex_to_bytes(*snapshot->SmoothMouseXCurve);
    }
    if (snapshot->SmoothMouseYCurve.has_value())
    {
        prepared.SmoothMouseYCurve = hex_to_bytes(*snapshot->SmoothMouseYCurve);
    }

    return prepared;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

WindowsSnapshot capture()
{
    WindowsSnapshot snap;

    // ── SPI: mouse speed ─────────────────────────────────────────────────────
    UINT speed = 0;
    if (!SystemParametersInfoW(SPI_GETMOUSESPEED, 0, &speed, 0))
    {
        throw_last_error("SystemParametersInfoW(SPI_GETMOUSESPEED)");
    }
    snap.mouse_speed = static_cast<int>(speed);

    // ── SPI: mouse acceleration thresholds + flag ─────────────────────────────
    // SPI_GETMOUSE fills an array of 3 INTs:
    //   [0] = MouseThreshold1, [1] = MouseThreshold2, [2] = MouseAcceleration
    INT mouse_params[3] = {0, 0, 0};
    if (!SystemParametersInfoW(SPI_GETMOUSE, 0, mouse_params, 0))
    {
        throw_last_error("SystemParametersInfoW(SPI_GETMOUSE)");
    }
    snap.mouse_threshold1 = mouse_params[0];
    snap.mouse_threshold2 = mouse_params[1];
    snap.mouse_accel = mouse_params[2];
    snap.enhance_pointer_precision = (mouse_params[2] != 0);

    // ── Registry: HKCU\Control Panel\Mouse ───────────────────────────────────
    HKEY hKey = nullptr;
    LSTATUS st = RegOpenKeyExW(HKEY_CURRENT_USER, kMouseKey, 0, KEY_QUERY_VALUE, &hKey);
    if (st == ERROR_SUCCESS && hKey != nullptr)
    {
        WindowsRegistrySnapshot reg;
        reg.MouseSensitivity = reg_read_sz(hKey, L"MouseSensitivity");
        reg.MouseSpeed = reg_read_sz(hKey, L"MouseSpeed");
        reg.MouseThreshold1 = reg_read_sz(hKey, L"MouseThreshold1");
        reg.MouseThreshold2 = reg_read_sz(hKey, L"MouseThreshold2");
        reg.SmoothMouseXCurve = reg_read_binary_hex(hKey, L"SmoothMouseXCurve");
        reg.SmoothMouseYCurve = reg_read_binary_hex(hKey, L"SmoothMouseYCurve");
        RegCloseKey(hKey);
        snap.registry = reg;
    }
    else if (st != ERROR_FILE_NOT_FOUND && st != ERROR_PATH_NOT_FOUND)
    {
        // Log but do not fail — registry access might be restricted.
        // The SPI values captured above are still valid.
    }

    return snap;
}

void apply(const WindowsSnapshot &snap)
{
    // ── Validate ranges ───────────────────────────────────────────────────────
    if (snap.mouse_speed < 1 || snap.mouse_speed > 20)
    {
        throw std::runtime_error("mouse_speed out of range [1..20]: " + std::to_string(snap.mouse_speed));
    }
    if (snap.mouse_threshold1 < 0 || snap.mouse_threshold2 < 0)
    {
        throw std::runtime_error("mouse thresholds must be >= 0");
    }
    if (snap.mouse_accel < 0 || snap.mouse_accel > 2)
    {
        throw std::runtime_error("mouse_accel out of range [0..2]");
    }

    auto prepared_registry = prepare_registry_snapshot(snap.registry);
    HKEY hKey = nullptr;
    if (prepared_registry.has_value())
    {
        LSTATUS st = RegOpenKeyExW(HKEY_CURRENT_USER, kMouseKey, 0, KEY_SET_VALUE, &hKey);
        if (st != ERROR_SUCCESS || hKey == nullptr)
        {
            throw_win_error("RegOpenKeyExW (write) for Control Panel\\Mouse", static_cast<DWORD>(st));
        }
    }

    // ── SPI: mouse speed ─────────────────────────────────────────────────────
    // SPI_SETMOUSESPEED: uiParam = speed (1..20), pvParam = nullptr.
    if (!SystemParametersInfoW(SPI_SETMOUSESPEED, 0, reinterpret_cast<PVOID>(static_cast<UINT_PTR>(snap.mouse_speed)),
                               SPIF_SENDCHANGE))
    {
        throw_last_error("SystemParametersInfoW(SPI_SETMOUSESPEED)");
    }

    // ── SPI: mouse acceleration thresholds + flag ─────────────────────────────
    INT mouse_params[3] = {
        snap.mouse_threshold1,
        snap.mouse_threshold2,
        snap.mouse_accel,
    };
    if (!SystemParametersInfoW(SPI_SETMOUSE, 0, mouse_params, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE))
    {
        throw_last_error("SystemParametersInfoW(SPI_SETMOUSE)");
    }

    // ── Registry: HKCU\Control Panel\Mouse ───────────────────────────────────
    if (prepared_registry.has_value())
    {
        const auto &reg = *prepared_registry;

        if (reg.MouseSensitivity.has_value())
        {
            reg_write_sz(hKey, L"MouseSensitivity", *reg.MouseSensitivity);
        }
        if (reg.MouseSpeed.has_value())
        {
            reg_write_sz(hKey, L"MouseSpeed", *reg.MouseSpeed);
        }
        if (reg.MouseThreshold1.has_value())
        {
            reg_write_sz(hKey, L"MouseThreshold1", *reg.MouseThreshold1);
        }
        if (reg.MouseThreshold2.has_value())
        {
            reg_write_sz(hKey, L"MouseThreshold2", *reg.MouseThreshold2);
        }
        if (reg.SmoothMouseXCurve.has_value())
        {
            reg_write_binary(hKey, L"SmoothMouseXCurve", *reg.SmoothMouseXCurve);
        }
        if (reg.SmoothMouseYCurve.has_value())
        {
            reg_write_binary(hKey, L"SmoothMouseYCurve", *reg.SmoothMouseYCurve);
        }

        RegCloseKey(hKey);
    }

    // ── Broadcast WM_SETTINGCHANGE ────────────────────────────────────────────
    // This notifies running applications that system parameters have changed.
    // We use a timeout so we don't block indefinitely.
    DWORD_PTR result = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Control Panel\\Mouse"),
                        SMTO_ABORTIFHUNG | SMTO_NOTIMEOUTIFNOTHUNG, 5000, &result);
}

} // namespace mouse_sync::windows
