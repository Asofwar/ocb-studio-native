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
    result.push_back({0, "Авто"});
    result.push_back({1, "Режим 1"});
    for (std::uint64_t mode = 2; mode <= 23; ++mode) {
        result.push_back({(mode - 1) * 10, "Режим " + std::to_string(mode)});
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
        throw OcbException("Неизвестный VarStore: " + varStore);
    }
    return *base + varOffset;
}

std::size_t OcbField::sizeBytes() const {
    if (sizeBits != 8 && sizeBits != 16 && sizeBits != 32) {
        throw OcbException("Неподдерживаемая ширина поля: " + std::to_string(sizeBits));
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
         options({{0, "Обычный"}, {1, "Расширенный"}, {2, "Intel по умолчанию"}})},
        {"CPU Lite Load", FieldKind::OneOf, "Setup", 0xF64, 8, {}, 0, liteLoadOptions()},
        {"CPU Cooler Tuning", FieldKind::OneOf, "Setup", 0xF89, 8, {}, 0,
         options({{1, "Настройки Intel по умолчанию"}, {2, "Настройки производительности MSI"}, {3, "Настройки MSI без ограничений"}})},
        {"Enhanced Turbo", FieldKind::OneOf, "Setup", 0xCB2, 8, {}, 0,
         options({{0, "Авто"}, {1, "Отключено"}, {2, "Включено"}})},
        {"Game Boost", FieldKind::OneOf, "Setup", 0x127C, 8, {}, 0,
         options({{0, "Отключено"}, {1, "Включено"}})},
        {"IA CEP Support", FieldKind::OneOf, "Setup", 0xFBA, 8, {}, 0,
         options({{0, "Авто"}, {1, "Отключено"}, {2, "Включено"}})},
        {"IA CEP Support For 14th", FieldKind::OneOf, "Setup", 0xFBC, 8, {}, 0,
         options({{0, "Авто"}, {1, "Отключено"}, {2, "Включено"}})},
        {"IA CEP Enable", FieldKind::OneOf, "CpuSetup", 0x334, 8, {}, 0,
         options({{0, "Отключено"}, {1, "Включено"}})},
        {"GT CEP Enable", FieldKind::OneOf, "CpuSetup", 0x335, 8, {}, 0,
         options({{0, "Отключено"}, {1, "Включено"}})},
        {"IA ICC Unlimited Mode", FieldKind::OneOf, "CpuSetup", 0x346, 8, {}, 0,
         options({{0, "Отключено"}, {1, "Включено"}})},
        {"GT ICC Unlimited Mode", FieldKind::OneOf, "CpuSetup", 0x349, 8, {}, 0,
         options({{0, "Отключено"}, {1, "Включено"}})},
        {"CPU AC LOADLINE", FieldKind::Numeric, "CpuSetup", 0x132, 16},
        {"CPU DC LOADLINE", FieldKind::Numeric, "CpuSetup", 0x13C, 16},
        {"CPU VR Voltage Limit", FieldKind::Numeric, "CpuSetup", 0x1BE, 16},
        {"CPU Loadline Calibration Control", FieldKind::OneOf, "Setup", 0x123F, 16, {}, 0,
         options({{240, "Авто"}, {0, "Режим 1"}, {5, "Режим 2"}, {10, "Режим 3"},
                  {15, "Режим 4"}, {20, "Режим 5"}, {50, "Режим 6"}})},
        {"CPU Loadline Saturation Control", FieldKind::Numeric, "Setup", 0x1241, 8},
        {"CPU Over Voltage Protection", FieldKind::Numeric, "Setup", 0x1244, 16},
    };
    return fields;
}

} // namespace ocb::core
