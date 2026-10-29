#pragma once

#include <filesystem>
#include <string_view>

namespace gui
{
// Returns the full path of a file inside the assets folder, e.g.
// assetPath("fonts/DejaVuSans.ttf").
//
// The assets folder is looked up next to the executable first (that is where
// the CMake build copies it), and then in the current working directory, so
// the game runs no matter which folder Visual Studio or VS Code starts it in.
std::filesystem::path assetPath(std::string_view relativePath);
} // namespace gui
