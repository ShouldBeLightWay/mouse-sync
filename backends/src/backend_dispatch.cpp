#include <mouse_sync/backend.hpp>

#ifdef _WIN32
#include "windows_backend.hpp"
#endif

namespace mouse_sync
{
namespace
{

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