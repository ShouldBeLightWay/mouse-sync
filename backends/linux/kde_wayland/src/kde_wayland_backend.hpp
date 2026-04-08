#pragma once

#include <mouse_sync/profile.hpp>

namespace mouse_sync::linux::kde_wayland
{

bool is_supported_environment();
LinuxSnapshot capture();
void apply(const LinuxSnapshot &snapshot);

} // namespace mouse_sync::linux::kde_wayland