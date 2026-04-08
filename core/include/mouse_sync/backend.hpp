#pragma once

#include <mouse_sync/profile.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mouse_sync
{

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
MouseProfile capture_profile(const std::string &backend_id, const std::string &created_at);
void apply_profile(const std::string &backend_id, const MouseProfile &profile);

} // namespace mouse_sync