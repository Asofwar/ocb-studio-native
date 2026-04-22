#include "ocb/core/Preset.hpp"

#include "ocb/core/OcbException.hpp"

#include <algorithm>
#include <cctype>

namespace ocb::core {
namespace {

std::string normalizedPrompt(std::string_view prompt) {
    std::string normalized;
    normalized.reserve(prompt.size());

    for (unsigned char ch : prompt) {
        if (std::isalnum(ch)) {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return normalized;
}

std::string canonicalPresetPrompt(std::string_view prompt) {
    auto normalized = normalizedPrompt(prompt);
    if (normalized == "corecepenable") {
        return "iacepenable";
    }
    if (normalized == "coreiccenable" || normalized == "coreiccunlimitedmode") {
        return "iaiccunlimitedmode";
    }
    if (normalized == "longdurationmaintaineds") {
        return "longdurationmaintained";
    }
    return normalized;
}

} // namespace

const std::vector<Preset>& builtinPresets() {
    static const std::vector<Preset> presets{
        {"Консервативный 200/220W 307A",
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
        {"Лимиты Intel 253/253W 307A",
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
        {"Производительность 253/253W 400A",
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
        {"Отключить только помощники boost",
         {
             {"Enhanced Turbo", 1},
             {"Game Boost", 0},
             {"IA ICC Unlimited Mode", 0},
             {"GT ICC Unlimited Mode", 0},
         }},
    };
    return presets;
}

void applyPreset(OcbProfile& profile, std::span<const OcbField> fields, const Preset& preset) {
    for (const auto& [prompt, value] : preset.valuesByPrompt) {
        const auto normalized = canonicalPresetPrompt(prompt);
        const auto field = std::find_if(fields.begin(), fields.end(), [&](const OcbField& candidate) {
            return candidate.prompt == prompt || canonicalPresetPrompt(candidate.prompt) == normalized;
        });
        if (field == fields.end()) {
            throw OcbException("Пресет ссылается на неизвестное поле: " + prompt);
        }
        profile.write(*field, value);
    }
}

void applyPreset(OcbProfile& profile, const Preset& preset) {
    applyPreset(profile, builtinFields(), preset);
}

void applyPreset(OcbProfile& profile, const std::string& presetName) {
    const auto& presets = builtinPresets();
    const auto preset = std::find_if(presets.begin(), presets.end(), [&](const Preset& candidate) {
        return candidate.name == presetName;
    });
    if (preset == presets.end()) {
        throw OcbException("Неизвестный пресет: " + presetName);
    }
    applyPreset(profile, *preset);
}

} // namespace ocb::core
