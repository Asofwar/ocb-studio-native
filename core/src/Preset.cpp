#include "ocb/core/Preset.hpp"

#include "ocb/core/OcbException.hpp"

#include <algorithm>

namespace ocb::core {

const std::vector<Preset>& builtinPresets() {
    static const std::vector<Preset> presets{
        {"Conservative 200/220W 307A",
         {
             {"Long Duration Power Limit (W)", 200},
             {"Short Duration Power Limit (W)", 220},
             {"CPU Current Limit (A)", 307},
             {"CPU Lite Load Control", 0},
             {"CPU Lite Load", 30},
             {"CPU Cooler Tuning", 1},
             {"Enhanced Turbo", 1},
             {"Game Boost", 0},
             {"IA CEP Enable", 1},
             {"GT CEP Enable", 1},
             {"IA ICC Unlimited Mode", 0},
             {"GT ICC Unlimited Mode", 0},
         }},
        {"Intel limits 253/253W 307A",
         {
             {"Long Duration Power Limit (W)", 253},
             {"Short Duration Power Limit (W)", 253},
             {"CPU Current Limit (A)", 307},
             {"CPU Cooler Tuning", 1},
             {"Enhanced Turbo", 1},
             {"Game Boost", 0},
             {"IA CEP Enable", 1},
             {"GT CEP Enable", 1},
             {"IA ICC Unlimited Mode", 0},
             {"GT ICC Unlimited Mode", 0},
         }},
        {"Performance 253/253W 400A",
         {
             {"Long Duration Power Limit (W)", 253},
             {"Short Duration Power Limit (W)", 253},
             {"CPU Current Limit (A)", 400},
             {"CPU Cooler Tuning", 2},
             {"Enhanced Turbo", 1},
             {"Game Boost", 0},
             {"IA ICC Unlimited Mode", 0},
             {"GT ICC Unlimited Mode", 0},
         }},
        {"Disable boost helpers only",
         {
             {"Enhanced Turbo", 1},
             {"Game Boost", 0},
             {"IA ICC Unlimited Mode", 0},
             {"GT ICC Unlimited Mode", 0},
         }},
    };
    return presets;
}

void applyPreset(OcbProfile& profile, const Preset& preset) {
    const auto& fields = builtinFields();

    for (const auto& [prompt, value] : preset.valuesByPrompt) {
        const auto field = std::find_if(fields.begin(), fields.end(), [&](const OcbField& candidate) {
            return candidate.prompt == prompt;
        });
        if (field == fields.end()) {
            throw OcbException("Preset references an unknown field: " + prompt);
        }
        profile.write(*field, value);
    }
}

void applyPreset(OcbProfile& profile, const std::string& presetName) {
    const auto& presets = builtinPresets();
    const auto preset = std::find_if(presets.begin(), presets.end(), [&](const Preset& candidate) {
        return candidate.name == presetName;
    });
    if (preset == presets.end()) {
        throw OcbException("Unknown preset: " + presetName);
    }
    applyPreset(profile, *preset);
}

} // namespace ocb::core
