#include <mouse_sync/backend.hpp>

#ifdef _WIN32
#include "windows_backend.hpp"
#endif

namespace mouse_sync
{
namespace
{

std::optional<std::string> backend_os(const std::string &backend_id)
{
    if (backend_id == kWindowsBackendId)
    {
        return std::string{kWindowsBackendId};
    }
    if (backend_id == kLinuxOsId)
    {
        return std::string{kLinuxOsId};
    }
    if (backend_id == "kde-wayland" || backend_id == "x11-cinnamon")
    {
        return std::string{kLinuxOsId};
    }
    return std::nullopt;
}

std::vector<std::string> backends_for_os(const std::string &os_id)
{
    std::vector<std::string> matches;
    for (const auto &backend_id : available_backends())
    {
        auto candidate_os = backend_os(backend_id);
        if (candidate_os.has_value() && *candidate_os == os_id)
        {
            matches.push_back(backend_id);
        }
    }
    return matches;
}

[[noreturn]] void throw_backend_unavailable(const std::string &backend_id)
{
    throw BackendError(backend_id, "Backend '" + backend_id + "' is not available on this platform.");
}

[[noreturn]] void throw_backend_not_implemented(const std::string &backend_id)
{
    throw BackendError(backend_id, "Backend '" + backend_id + "' is not implemented yet.");
}

} // namespace

std::vector<std::string> available_backends()
{
    std::vector<std::string> backends;
#ifdef _WIN32
    backends.emplace_back(kWindowsBackendId);
#endif
    return backends;
}

std::optional<std::string> detect_current_os()
{
#ifdef _WIN32
    return std::string{kWindowsBackendId};
#elif defined(__linux__)
    return std::string{kLinuxOsId};
#else
    return std::nullopt;
#endif
}

std::optional<std::string> auto_detect_backend()
{
    const auto backends = available_backends();
    if (backends.size() == 1)
    {
        return backends.front();
    }

    return std::nullopt;
}

BackendSelection resolve_backend_selection(const std::string &requested_os, const std::string &requested_backend)
{
    const bool os_is_auto = requested_os.empty() || requested_os == "auto";
    const bool backend_is_auto = requested_backend.empty() || requested_backend == "auto";

    std::optional<std::string> resolved_os;
    std::optional<std::string> resolved_backend;

    if (!os_is_auto)
    {
        resolved_os = requested_os;
    }
    if (!backend_is_auto)
    {
        resolved_backend = requested_backend;
    }

    if (!resolved_os.has_value())
    {
        if (resolved_backend.has_value())
        {
            resolved_os = backend_os(*resolved_backend);
            if (!resolved_os.has_value())
            {
                throw BackendError(*resolved_backend, "Cannot infer OS for backend '" + *resolved_backend + "'.");
            }
        }
        else
        {
            resolved_os = detect_current_os();
            if (!resolved_os.has_value())
            {
                throw BackendError("auto", "Cannot determine the current OS automatically.");
            }
        }
    }

    if (!resolved_backend.has_value())
    {
        auto detected_backend = auto_detect_backend();
        if (detected_backend.has_value())
        {
            auto detected_os = backend_os(*detected_backend);
            if (detected_os.has_value() && *detected_os == *resolved_os)
            {
                resolved_backend = *detected_backend;
            }
        }
    }

    if (!resolved_backend.has_value())
    {
        const auto candidates = backends_for_os(*resolved_os);
        if (candidates.empty())
        {
            throw BackendError(*resolved_os, "No supported backend is available for OS '" + *resolved_os + "'.");
        }
        if (candidates.size() > 1)
        {
            throw BackendError(*resolved_os, "Cannot determine backend automatically for OS '" + *resolved_os +
                                                 "'. Please specify --backend explicitly.");
        }
        resolved_backend = candidates.front();
    }

    const auto backend_resolved_os = backend_os(*resolved_backend);
    if (!backend_resolved_os.has_value())
    {
        throw BackendError(*resolved_backend, "Unknown backend '" + *resolved_backend + "'.");
    }

    if (*backend_resolved_os != *resolved_os)
    {
        throw BackendError(*resolved_backend,
                           "Requested OS '" + *resolved_os + "' does not match backend '" + *resolved_backend + "'.");
    }

    return BackendSelection{*resolved_os, *resolved_backend};
}

MouseProfile capture_profile(const std::string &backend_id, const std::string &created_at)
{
    if (backend_id == kWindowsBackendId)
    {
#ifdef _WIN32
        try
        {
            MouseProfile profile;
            profile.created_at = created_at;
            profile.source_os = kWindowsBackendId;
            profile.source_backend = std::string{kWindowsBackendId};
            profile.windows = windows::capture();
            validate_profile(profile);
            return profile;
        }
        catch (const windows::WindowsError &ex)
        {
            throw BackendError(kWindowsBackendId, ex.what(), static_cast<long>(ex.error_code));
        }
#else
        throw_backend_unavailable(backend_id);
#endif
    }

    if (backend_id == kLinuxOsId)
    {
        throw_backend_not_implemented(backend_id);
    }

    throw BackendError(backend_id, "Unknown backend '" + backend_id + "'.");
}

void apply_profile(const std::string &backend_id, const MouseProfile &profile)
{
    validate_profile_for_apply(profile, backend_id);

    if (backend_id == kWindowsBackendId)
    {
#ifdef _WIN32
        try
        {
            windows::apply(*profile.windows);
            return;
        }
        catch (const windows::WindowsError &ex)
        {
            throw BackendError(kWindowsBackendId, ex.what(), static_cast<long>(ex.error_code));
        }
#else
        throw_backend_unavailable(backend_id);
#endif
    }

    if (backend_id == kLinuxOsId)
    {
        throw_backend_not_implemented(backend_id);
    }

    throw BackendError(backend_id, "Unknown backend '" + backend_id + "'.");
}

} // namespace mouse_sync