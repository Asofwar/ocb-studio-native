#include "ocb/core/OcbField.hpp"

#include "ocb/core/OcbException.hpp"

#include <array>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ocb::core {
namespace {

std::vector<OcbOption> options(std::initializer_list<OcbOption> items) {
    return std::vector<OcbOption>(items);
}

std::vector<OcbOption> liteLoadOptions() {
    std::vector<OcbOption> result;
    result.push_back({0, "Auto"});
    result.push_back({1, "Mode 1"});
    for (std::uint64_t mode = 2; mode <= 23; ++mode) {
        result.push_back({(mode - 1) * 10, "Mode " + std::to_string(mode)});
    }
    return result;
}

const std::array<std::pair<std::string_view, std::size_t>, 7> kVarStoreBases{{
    {"Setup", 0x001F},
    {"CpuSetup", 0x15E2},
    {"SaSetup", 0x19F9},
    {"PchSetup", 0x1F83},
    {"MeSetup", 0x27AF},
    {"DebugConfigData", 0x284D},
    {"UsbSupport", 0x2944},
}};

} // namespace

std::string OcbField::id() const {
    std::ostringstream stream;
    stream << varStore << ':' << std::uppercase << std::hex << std::setw(4)
           << std::setfill('0') << varOffset << ':' << std::dec << sizeBits << ':'
           << prompt;
    return stream.str();
}

std::size_t OcbField::fileOffset() const {
    const auto base = varStoreBase(varStore);
    if (!base.has_value()) {
        throw OcbException("Unknown VarStore: " + varStore);
    }
    return *base + varOffset;
}

std::size_t OcbField::sizeBytes() const {
    if (sizeBits != 8 && sizeBits != 16 && sizeBits != 32) {
        throw OcbException("Unsupported field width: " + std::to_string(sizeBits));
    }
    return sizeBits / 8;
}

std::optional<std::size_t> varStoreBase(std::string_view name) {
    for (const auto& [candidate, base] : kVarStoreBases) {
        if (candidate == name) {
            return base;
        }
    }
    return std::nullopt;
}

const std::vector<OcbField>& builtinFields() {
    static const std::vector<OcbField> fields{
        {"Long Duration Power Limit (W)", FieldKind::Numeric, "CpuSetup", 0x17, 32},
        {"Short Duration Power Limit (W)", FieldKind::Numeric, "CpuSetup", 0x1E, 32},
        {"CPU Current Limit (A)", FieldKind::Numeric, "Setup", 0xF2A, 16},
        {"CPU Lite Load Control", FieldKind::OneOf, "Setup", 0xF63, 8, {}, 0,
         options({{0, "Normal"}, {1, "Advanced"}, {2, "Intel Default"}})},
        {"CPU Lite Load", FieldKind::OneOf, "Setup", 0xF64, 8, {}, 0, liteLoadOptions()},
        {"CPU Cooler Tuning", FieldKind::OneOf, "Setup", 0xF89, 8, {}, 0,
         options({{1, "Intel Default Settings"}, {2, "MSI Performance Settings"}, {3, "MSI Unlimited Settings"}})},
        {"Enhanced Turbo", FieldKind::OneOf, "Setup", 0xCB2, 8, {}, 0,
         options({{0, "Auto"}, {1, "Disabled"}, {2, "Enabled"}})},
        {"Game Boost", FieldKind::OneOf, "Setup", 0x127C, 8, {}, 0,
         options({{0, "Disabled"}, {1, "Enabled"}})},
        {"IA CEP Support", FieldKind::OneOf, "Setup", 0xFBA, 8, {}, 0,
         options({{0, "Auto"}, {1, "Disabled"}, {2, "Enabled"}})},
        {"IA CEP Support For 14th", FieldKind::OneOf, "Setup", 0xFBC, 8, {}, 0,
         options({{0, "Auto"}, {1, "Disabled"}, {2, "Enabled"}})},
        {"IA CEP Enable", FieldKind::OneOf, "CpuSetup", 0x334, 8, {}, 0,
         options({{0, "Disabled"}, {1, "Enabled"}})},
        {"GT CEP Enable", FieldKind::OneOf, "CpuSetup", 0x335, 8, {}, 0,
         options({{0, "Disabled"}, {1, "Enabled"}})},
        {"IA ICC Unlimited Mode", FieldKind::OneOf, "CpuSetup", 0x346, 8, {}, 0,
         options({{0, "Disabled"}, {1, "Enabled"}})},
        {"GT ICC Unlimited Mode", FieldKind::OneOf, "CpuSetup", 0x349, 8, {}, 0,
         options({{0, "Disabled"}, {1, "Enabled"}})},
        {"CPU AC LOADLINE", FieldKind::Numeric, "CpuSetup", 0x132, 16},
        {"CPU DC LOADLINE", FieldKind::Numeric, "CpuSetup", 0x13C, 16},
        {"CPU VR Voltage Limit", FieldKind::Numeric, "CpuSetup", 0x1BE, 16},
        {"CPU Loadline Calibration Control", FieldKind::OneOf, "Setup", 0x123F, 16, {}, 0,
         options({{240, "Auto"}, {0, "Mode 1"}, {5, "Mode 2"}, {10, "Mode 3"},
                  {15, "Mode 4"}, {20, "Mode 5"}, {50, "Mode 6"}})},
        {"CPU Loadline Saturation Control", FieldKind::Numeric, "Setup", 0x1241, 8},
        {"CPU Over Voltage Protection", FieldKind::Numeric, "Setup", 0x1244, 16},
    };
    return fields;
}

} // namespace ocb::core
