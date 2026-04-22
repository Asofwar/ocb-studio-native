#pragma once

#include "ocb/core/Preset.hpp"

#include <filesystem>

namespace ocb::core {

[[nodiscard]] Preset loadPresetFromFile(const std::filesystem::path& path);
void savePresetToFile(const std::filesystem::path& path, const Preset& preset);

} // namespace ocb::core
