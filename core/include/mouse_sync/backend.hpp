#pragma once

#include <mouse_sync/profile.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mouse_sync
{

struct BackendSelection
{
    std::string os_id;
    std::string backend_id;
};

struct BackendError : std::runtime_error
{
    std::string backend_id;
    std::optional<long> error_code;

    BackendError(std::string backend_id_value, std::string message, std::optional<long> code = std::nullopt)
        : std::runtime_error(std::move(message)), backend_id(std::move(backend_id_value)), error_code(code)
    {
    }
};

std::vector<std::string> available_backends();
std::optional<std::string> detect_current_os();
std::optional<std::string> auto_detect_backend();
BackendSelection resolve_backend_selection(const std::string &requested_os, const std::string &requested_backend);
MouseProfile capture_profile(const std::string &backend_id, const std::string &created_at);
void apply_profile(const std::string &backend_id, const MouseProfile &profile);

} // namespace mouse_sync