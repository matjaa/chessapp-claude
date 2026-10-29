#include "gui/Assets.hpp"

#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace gui
{
namespace
{
// Folder that contains the running executable (empty if it can't be found).
std::filesystem::path executableDirectory()
{
#ifdef _WIN32
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return {};
        if (length < buffer.size())
            return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
        buffer.resize(buffer.size() * 2); // path was truncated, try a bigger buffer
    }
#else
    std::error_code error;
    const auto exe = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::path{} : exe.parent_path();
#endif
}
} // namespace

std::filesystem::path assetPath(std::string_view relativePath)
{
    static const std::filesystem::path exeDir = executableDirectory();

    const std::filesystem::path relative = std::filesystem::path("assets") / std::filesystem::path(relativePath);

    if (!exeDir.empty())
    {
        const auto besideExe = exeDir / relative;
        std::error_code error;
        if (std::filesystem::exists(besideExe, error))
            return besideExe;
    }
    return relative; // fall back to the current working directory
}
} // namespace gui
