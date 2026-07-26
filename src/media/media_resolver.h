#pragma once

#include <filesystem>

namespace MediaResolver
{
    std::filesystem::path FindBoxArt(
        const std::filesystem::path& romPath);
}