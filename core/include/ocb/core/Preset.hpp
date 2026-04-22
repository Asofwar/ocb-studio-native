#pragma once

#include "ocb/core/OcbProfile.hpp"

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace ocb::core {

struct Preset {
    std::string name;
    std::map<std::string, std::uint64_t> valuesByPrompt;
};

[[nodiscard]] const std::vector<Preset>& builtinPresets();
void applyPreset(OcbProfile& profile, const Preset& preset);
void applyPreset(OcbProfile& profile, std::span<const OcbField> fields, const Preset& preset);
void applyPreset(OcbProfile& profile, const std::string& presetName);

} // namespace ocb::core
