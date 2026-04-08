#pragma once

#include <mouse_sync/profile.hpp>

#include <stdexcept>
#include <string>

namespace mouse_sync::windows {

/// Thrown when a Windows API call fails.
struct WindowsError : std::runtime_error {
    unsigned long error_code;
    WindowsError(const std::string& msg, unsigned long code)
        : std::runtime_error(msg), error_code(code) {}
};

/// Read current mouse settings from the Windows system (SPI + registry).
/// May be called from a standard user account.
/// Throws WindowsError on API failures.
WindowsSnapshot capture();

/// Apply the settings contained in @p snap to the Windows system.
/// Writes SPI values, updates relevant registry keys under
/// HKCU\Control Panel\Mouse, and broadcasts WM_SETTINGCHANGE.
/// May require the calling process to run as the interactive user.
/// Throws WindowsError on API failures.
void apply(const WindowsSnapshot& snap);

} // namespace mouse_sync::windows
