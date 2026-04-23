#include "ocb/core/ChecksumCompensator.hpp"
#include "ocb/core/BiosAnalysisService.hpp"
#include "ocb/core/FieldCatalog.hpp"
#include "ocb/core/FieldValidation.hpp"
#include "ocb/core/IfrFieldMapper.hpp"
#include "ocb/core/OcbException.hpp"
#include "ocb/core/OcbProfile.hpp"
#include "ocb/core/Preset.hpp"
#include "ocb/core/PresetFile.hpp"
#include "ocb/tools/ifr/IfrTextParser.hpp"
#include "ocb/tools/ifr/NativeIfrExtractor.hpp"
#include "ocb/tools/uefi/UefiExtractor.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using ocb::core::OcbField;
using ocb::core::OcbProfile;

class FakeUefiExtractor final : public ocb::tools::uefi::UefiExtractor {
public:
    [[nodiscard]] ocb::tools::uefi::FirmwareNode parseImage(std::span<const std::uint8_t>) const override {
        return {};
    }

    [[nodiscard]] std::optional<ocb::tools::uefi::SetupModule> findBestSetupModule(
        const ocb::tools::uefi::FirmwareNode&) const override {
        ocb::tools::uefi::SetupModule setup;
        setup.pathHint = "root/MSI Setup/PE32 image";
        setup.pe32Body = {1, 2, 3, 4, 5};
        return setup;
    }
};

class FakeIfrExtractor final : public ocb::tools::ifr::IfrExtractor {
public:
    [[nodiscard]] std::vector<ocb::tools::ifr::IfrQuestion> extractQuestions(
        std::span<const std::uint8_t>) const override {
        ocb::tools::ifr::IfrQuestion question;
        question.prompt = "CPU Lite Load";
        question.questionId = 0x1234;
        question.varStoreName = "Setup";
        question.varOffset = 0xF64;
        question.sizeBits = 8;
        return {question};
    }
};

std::filesystem::path testDataDir() {
    return std::filesystem::path(OCB_TEST_DATA_DIR);
}

std::vector<std::uint8_t> readAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("РќРµ СѓРґР°Р»РѕСЃСЊ РїСЂРѕС‡РёС‚Р°С‚СЊ fixture-С„Р°Р№Р»: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

const OcbField& fieldByPrompt(const std::string& prompt) {
    const auto& fields = ocb::core::builtinFields();
    const auto it = std::find_if(fields.begin(), fields.end(), [&](const OcbField& field) {
        return field.prompt == prompt;
    });
    if (it == fields.end()) {
        throw std::runtime_error("РћС‚СЃСѓС‚СЃС‚РІСѓРµС‚ РІСЃС‚СЂРѕРµРЅРЅРѕРµ РїРѕР»Рµ: " + prompt);
    }
    return *it;
}

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void putAscii(std::vector<std::uint8_t>& bytes, std::size_t offset, std::string_view value) {
    std::copy(value.begin(), value.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

void appendU8(std::vector<std::uint8_t>& bytes, std::uint8_t value) {
    bytes.push_back(value);
}

void appendU16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void appendU24(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
}

void appendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    appendU16(bytes, static_cast<std::uint16_t>(value & 0xFFFF));
    appendU16(bytes, static_cast<std::uint16_t>((value >> 16) & 0xFFFF));
}

void appendAsciiNull(std::vector<std::uint8_t>& bytes, std::string_view value) {
    bytes.insert(bytes.end(), value.begin(), value.end());
    bytes.push_back(0);
}

void appendUcs2Null(std::vector<std::uint8_t>& bytes, std::string_view value) {
    for (const auto ch : value) {
        appendU16(bytes, static_cast<std::uint8_t>(ch));
    }
    appendU16(bytes, 0);
}

void appendPackageHeader(std::vector<std::uint8_t>& bytes, std::uint32_t length, std::uint8_t type) {
    appendU24(bytes, length);
    appendU8(bytes, type);
}

std::vector<std::uint8_t> syntheticProfileBytes() {
    std::vector<std::uint8_t> bytes(0x3000, 0);
    putAscii(bytes, 0, "$MOS$");
    putAscii(bytes, 0x400, "$OCI$");
    return bytes;
}

std::vector<std::uint8_t> syntheticHiiPackageList() {
    std::vector<std::uint8_t> stringPackage;
    appendPackageHeader(stringPackage, 0, 0x04);
    const auto headerSize = static_cast<std::uint32_t>(4 + 4 + 4 + 32 + 2 + 6);
    appendU32(stringPackage, headerSize);
    appendU32(stringPackage, headerSize);
    for (int i = 0; i < 16; ++i) {
        appendU16(stringPackage, 0);
    }
    appendU16(stringPackage, 0);
    appendAsciiNull(stringPackage, "en-US");
    appendU8(stringPackage, 0x14);
    appendUcs2Null(stringPackage, "Synthetic Question");
    appendU8(stringPackage, 0x14);
    appendUcs2Null(stringPackage, "Disabled");
    appendU8(stringPackage, 0x14);
    appendUcs2Null(stringPackage, "Enabled");
    appendU8(stringPackage, 0x00);
    const auto stringPackageLength = static_cast<std::uint32_t>(stringPackage.size());
    stringPackage[0] = static_cast<std::uint8_t>(stringPackageLength & 0xFF);
    stringPackage[1] = static_cast<std::uint8_t>((stringPackageLength >> 8) & 0xFF);
    stringPackage[2] = static_cast<std::uint8_t>((stringPackageLength >> 16) & 0xFF);

    std::vector<std::uint8_t> formPackage;
    appendPackageHeader(formPackage, 0, 0x02);
    appendU8(formPackage, 0x24);
    appendU8(formPackage, 28);
    for (int i = 0; i < 16; ++i) {
        appendU8(formPackage, 0);
    }
    appendU16(formPackage, 1);
    appendU16(formPackage, 0x1000);
    appendAsciiNull(formPackage, "Setup");
    appendU8(formPackage, 0x05);
    appendU8(formPackage, 0x80 | 14);
    appendU16(formPackage, 1);
    appendU16(formPackage, 0);
    appendU16(formPackage, 0x1234);
    appendU16(formPackage, 1);
    appendU16(formPackage, 0x00F6);
    appendU16(formPackage, 0);
    appendU8(formPackage, 0x09);
    appendU8(formPackage, 7);
    appendU16(formPackage, 2);
    appendU8(formPackage, 0);
    appendU8(formPackage, 0);
    appendU8(formPackage, 0);
    appendU8(formPackage, 0x09);
    appendU8(formPackage, 7);
    appendU16(formPackage, 3);
    appendU8(formPackage, 0);
    appendU8(formPackage, 0);
    appendU8(formPackage, 1);
    appendU8(formPackage, 0x29);
    appendU8(formPackage, 2);
    const auto formPackageLength = static_cast<std::uint32_t>(formPackage.size());
    formPackage[0] = static_cast<std::uint8_t>(formPackageLength & 0xFF);
    formPackage[1] = static_cast<std::uint8_t>((formPackageLength >> 8) & 0xFF);
    formPackage[2] = static_cast<std::uint8_t>((formPackageLength >> 16) & 0xFF);

    std::vector<std::uint8_t> bytes(16, 0);
    const auto listLength = static_cast<std::uint32_t>(20 + stringPackage.size() + formPackage.size() + 4);
    appendU32(bytes, listLength);
    bytes.insert(bytes.end(), stringPackage.begin(), stringPackage.end());
    bytes.insert(bytes.end(), formPackage.begin(), formPackage.end());
    appendPackageHeader(bytes, 4, 0xDF);
    return bytes;
}

void testSyntheticProfileMetadata() {
    std::vector<std::uint8_t> bytes(0x1200, 0);
    putAscii(bytes, 0, "$MOS$");
    putAscii(bytes, 0x80, "MPG Z790 EDGE TI MAX WIFI");
    putAscii(bytes, 0x120, "E7D89IMS.A91");
    putAscii(bytes, 0x180, "Daily OC Profile");
    putAscii(bytes, 0x400, "$OCI$");

    const OcbProfile profile(std::move(bytes));
    const auto& metadata = profile.metadata();

    expect(metadata.hasMosHeader, "Р СљР ВµРЎвЂљР В°Р Т‘Р В°Р Р…Р Р…РЎвЂ№Р Вµ Р Т‘Р С•Р В»Р В¶Р Р…РЎвЂ№ Р Р†Р С‘Р Т‘Р ВµРЎвЂљРЎРЉ $MOS$");
    expect(metadata.hasOciSection, "Р СљР ВµРЎвЂљР В°Р Т‘Р В°Р Р…Р Р…РЎвЂ№Р Вµ Р Т‘Р С•Р В»Р В¶Р Р…РЎвЂ№ Р Р†Р С‘Р Т‘Р ВµРЎвЂљРЎРЉ $OCI$");
    expect(metadata.ociOffset == 0x400, "Р РЋР СР ВµРЎвЂ°Р ВµР Р…Р С‘Р Вµ $OCI$ Р Т‘Р С•Р В»Р В¶Р Р…Р С• Р С•Р С—РЎР‚Р ВµР Т‘Р ВµР В»РЎРЏРЎвЂљРЎРЉРЎРѓРЎРЏ");
    expect(metadata.boardName == "MPG Z790 EDGE TI MAX WIFI", "Р СџР В»Р В°РЎвЂљР В° Р Т‘Р С•Р В»Р В¶Р Р…Р В° Р С•Р С—РЎР‚Р ВµР Т‘Р ВµР В»РЎРЏРЎвЂљРЎРЉРЎРѓРЎРЏ Р С‘Р В· РЎРѓРЎвЂљРЎР‚Р С•Р С” Р С—РЎР‚Р С•РЎвЂћР С‘Р В»РЎРЏ");
    expect(metadata.biosVersion == "E7D89IMS.A91", "BIOS ID Р Т‘Р С•Р В»Р В¶Р ВµР Р… Р С•Р С—РЎР‚Р ВµР Т‘Р ВµР В»РЎРЏРЎвЂљРЎРЉРЎРѓРЎРЏ Р С‘Р В· РЎРѓРЎвЂљРЎР‚Р С•Р С” Р С—РЎР‚Р С•РЎвЂћР С‘Р В»РЎРЏ");
    expect(metadata.profileName == "Daily OC Profile", "Р ВР СРЎРЏ Р С—РЎР‚Р С•РЎвЂћР С‘Р В»РЎРЏ Р Т‘Р С•Р В»Р В¶Р Р…Р С• Р С•Р С—РЎР‚Р ВµР Т‘Р ВµР В»РЎРЏРЎвЂљРЎРЉРЎРѓРЎРЏ Р С‘Р В· РЎРѓРЎвЂљРЎР‚Р С•Р С” Р С—РЎР‚Р С•РЎвЂћР С‘Р В»РЎРЏ");
}

void testProfileMetadataFallsBackFromBiosId() {
    std::vector<std::uint8_t> bytes(0x1200, 0);
    putAscii(bytes, 0, "$MOS$");
    putAscii(bytes, 0x80, "Z469");
    putAscii(bytes, 0x120, "E7D89IMS.A91");
    putAscii(bytes, 0x400, "$OCI$");

    const OcbProfile profile(std::move(bytes));

    expect(profile.metadata().boardName == "MSI MPG Z790 CARBON WIFI II", "BIOS ID E7D89IMS РґРѕР»Р¶РµРЅ РґР°РІР°С‚СЊ fallback РґР»СЏ MSI Z790 CARBON WIFI II");
}

void testBiosAnalysisMetadata() {
    std::vector<std::uint8_t> biosImage(512, 0);
    putAscii(biosImage, 0x10, "Z469");
    putAscii(biosImage, 0x20, "MPG Z790 EDGE TI MAX WIFI");
    putAscii(biosImage, 0x80, "E7D89IMS.A91");

    const FakeUefiExtractor uefiExtractor;
    const FakeIfrExtractor ifrExtractor;
    const ocb::core::BiosAnalysisService service(uefiExtractor, ifrExtractor);
    const auto result = service.analyze(biosImage);

    expect(result.metadata.boardName == "MSI MPG Z790 CARBON WIFI II", "BIOS-Р СР ВµРЎвЂљР В°Р Т‘Р В°Р Р…Р Р…РЎвЂ№Р Вµ Р Т‘Р С•Р В»Р В¶Р Р…РЎвЂ№ Р С•Р С—РЎР‚Р ВµР Т‘Р ВµР В»РЎРЏРЎвЂљРЎРЉ Р С—Р В»Р В°РЎвЂљРЎС“");
    expect(result.metadata.biosVersion == "E7D89IMS.A91", "BIOS-Р СР ВµРЎвЂљР В°Р Т‘Р В°Р Р…Р Р…РЎвЂ№Р Вµ Р Т‘Р С•Р В»Р В¶Р Р…РЎвЂ№ Р С•Р С—РЎР‚Р ВµР Т‘Р ВµР В»РЎРЏРЎвЂљРЎРЉ Р Р†Р ВµРЎР‚РЎРѓР С‘РЎР‹");
    expect(result.metadata.setupPath == "root/MSI Setup/PE32 image", "BIOS-Р СР ВµРЎвЂљР В°Р Т‘Р В°Р Р…Р Р…РЎвЂ№Р Вµ Р Т‘Р С•Р В»Р В¶Р Р…РЎвЂ№ РЎРѓР С•РЎвЂ¦РЎР‚Р В°Р Р…РЎРЏРЎвЂљРЎРЉ Р С—РЎС“РЎвЂљРЎРЉ Setup");
    expect(result.metadata.setupPe32Size == 5, "BIOS-Р СР ВµРЎвЂљР В°Р Т‘Р В°Р Р…Р Р…РЎвЂ№Р Вµ Р Т‘Р С•Р В»Р В¶Р Р…РЎвЂ№ РЎРѓР С•РЎвЂ¦РЎР‚Р В°Р Р…РЎРЏРЎвЂљРЎРЉ РЎР‚Р В°Р В·Р СР ВµРЎР‚ Setup PE32");
    expect(result.metadata.questionCount == 1, "BIOS-Р СР ВµРЎвЂљР В°Р Т‘Р В°Р Р…Р Р…РЎвЂ№Р Вµ Р Т‘Р С•Р В»Р В¶Р Р…РЎвЂ№ РЎРѓРЎвЂЎР С‘РЎвЂљР В°РЎвЂљРЎРЉ IFR-Р Р†Р С•Р С—РЎР‚Р С•РЎРѓРЎвЂ№");
    expect(result.metadata.fieldCount == 1, "BIOS-Р СР ВµРЎвЂљР В°Р Т‘Р В°Р Р…Р Р…РЎвЂ№Р Вµ Р Т‘Р С•Р В»Р В¶Р Р…РЎвЂ№ РЎРѓРЎвЂЎР С‘РЎвЂљР В°РЎвЂљРЎРЉ РЎРѓР С•Р С—Р С•РЎРѓРЎвЂљР В°Р Р†Р В»Р ВµР Р…Р Р…РЎвЂ№Р Вµ Р С—Р С•Р В»РЎРЏ");
}

void testReadKnownValues() {
    const auto profile = OcbProfile::loadFromFile(testDataDir() / "MsOcFile.ocb");

    expect(profile.read(fieldByPrompt("Long Duration Power Limit (W)")) == 200, "PL1 РґРѕР»Р¶РµРЅ Р±С‹С‚СЊ 200 W");
    expect(profile.read(fieldByPrompt("Short Duration Power Limit (W)")) == 220, "PL2 РґРѕР»Р¶РµРЅ Р±С‹С‚СЊ 220 W");
    expect(profile.read(fieldByPrompt("CPU Current Limit (A)")) == 307, "CPU current limit must be 307 A");
    expect(profile.read(fieldByPrompt("CPU Lite Load")) == 30, "CPU Lite Load РґРѕР»Р¶РµРЅ РёРјРµС‚СЊ Р·РЅР°С‡РµРЅРёРµ СЂРµР¶РёРјР° 4");
    expect(profile.read(fieldByPrompt("Game Boost")) == 0, "Game Boost РґРѕР»Р¶РµРЅ Р±С‹С‚СЊ РѕС‚РєР»СЋС‡РµРЅ");
}

void testConservativePresetMatchesWorkingTry02() {
    auto profile = OcbProfile::loadFromFile(testDataDir() / "MsOcFile.ocb");
    ocb::core::applyPreset(profile, "РљРѕРЅСЃРµСЂРІР°С‚РёРІРЅС‹Р№ 200/220W 307A");

    const auto produced = profile.exportBytes();
    const auto expected = readAll(testDataDir() / "try_02_conservative_sum_comp" / "MsOcFile.ocb");

    expect(produced == expected, "РљРѕРЅСЃРµСЂРІР°С‚РёРІРЅС‹Р№ РїСЂРµСЃРµС‚ РґРѕР»Р¶РµРЅ РїРѕР±Р°Р№С‚РЅРѕ СЃРѕРІРїР°РґР°С‚СЊ СЃ РёР·РІРµСЃС‚РЅС‹Рј РІС‹РІРѕРґРѕРј try02, РїСЂРёРЅСЏС‚С‹Рј BIOS");
    expect(
        ocb::core::ChecksumCompensator::compute(produced) == profile.targetSums(),
        "РЎСѓРјРјС‹ РІ СЃС‚РёР»Рµ РєРѕРЅС‚СЂРѕР»СЊРЅС‹С… СЃСѓРјРј РґРѕР»Р¶РЅС‹ СЃРѕРІРїР°РґР°С‚СЊ СЃ Р·Р°РіСЂСѓР¶РµРЅРЅС‹Рј РѕСЂРёРіРёРЅР°Р»СЊРЅС‹Рј РїСЂРѕС„РёР»РµРј");
}

void testUnchangedProfileExportsByteIdentical() {
    auto bytes = syntheticProfileBytes();
    bytes.at(ocb::core::ChecksumCompensator::kCompensationOffset + 1) = 0xC3;

    const OcbProfile profile(bytes);

    expect(profile.exportBytes(true) == bytes, "Unchanged profile export must preserve exact original bytes");
}

void testPresetFileRoundTrip() {
    const ocb::core::Preset preset{
        "Custom preset",
        {
            {"CPU Lite Load", 30},
            {"Long Duration Power Limit (W)", 200},
            {"Short Duration Power Limit (W)", 220},
        }};

    const auto path = std::filesystem::temp_directory_path() / "ocb_studio_preset_roundtrip.ocbpreset";
    ocb::core::savePresetToFile(path, preset);
    const auto loaded = ocb::core::loadPresetFromFile(path);
    std::filesystem::remove(path);

    expect(loaded.name == preset.name, "Preset import/export must preserve the preset name");
    expect(loaded.valuesByPrompt == preset.valuesByPrompt, "Preset import/export must preserve field values");
}

void testPresetFileAcceptsStringValues() {
    const auto path = std::filesystem::temp_directory_path() / "ocb_studio_preset_string_values.ocbpreset";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "{\n"
               << "  \"name\": \"String values\",\n"
               << "  \"values\": {\n"
               << "    \"CPU Lite Load\": \"0x1E\"\n"
               << "  }\n"
               << "}\n";
    }

    const auto loaded = ocb::core::loadPresetFromFile(path);
    std::filesystem::remove(path);

    expect(loaded.valuesByPrompt.at("CPU Lite Load") == 30, "Preset import must accept quoted decimal or hex values");
}

void testPresetAppliesPromptsWithSpacingVariants() {
    OcbProfile profile(syntheticProfileBytes());
    const ocb::core::Preset preset{
        "Spacing variants",
        {
            {"CPU Current Limit(A)", 307},
            {"Long Duration Power Limit(W)", 180},
            {"Short Duration Power Limit(W)", 200},
        }};

    ocb::core::applyPreset(profile, preset);

    expect(profile.read(fieldByPrompt("CPU Current Limit (A)")) == 307, "Preset prompts should tolerate missing spaces before units");
    expect(profile.read(fieldByPrompt("Long Duration Power Limit (W)")) == 180, "Preset prompts should map PL1 spacing variants");
    expect(profile.read(fieldByPrompt("Short Duration Power Limit (W)")) == 200, "Preset prompts should map PL2 spacing variants");
}

void testPresetAppliesCommonAliasPrompts() {
    OcbProfile profile(syntheticProfileBytes());
    const ocb::core::Preset preset{
        "Alias prompts",
        {
            {"Core CEP Enable", 0},
            {"Core ICC Unlimited Mode", 0},
        }};

    ocb::core::applyPreset(profile, preset);

    expect(profile.read(fieldByPrompt("IA CEP Enable")) == 0, "Core CEP alias should map to IA CEP Enable");
    expect(profile.read(fieldByPrompt("IA ICC Unlimited Mode")) == 0, "Core ICC alias should map to IA ICC Unlimited Mode");
}

void testPresetAppliesAgainstExtendedCatalog() {
    OcbProfile profile(syntheticProfileBytes());
    const std::vector<OcbField> fields{
        {"Long Duration Maintained", ocb::core::FieldKind::Numeric, "CpuSetup", 0x20, 16},
    };
    const ocb::core::Preset preset{
        "Extended catalog",
        {
            {"Long Duration Maintained(s)", 56},
        }};

    ocb::core::applyPreset(profile, fields, preset);

    expect(profile.read(fields.front()) == 56, "Preset application should use fields from the active catalog");
}

void testInvalidInputRejected() {
    bool rejected = false;
    try {
        OcbProfile profile(std::vector<std::uint8_t>(128, 0));
    } catch (const ocb::core::OcbException&) {
        rejected = true;
    }
    expect(rejected, "РќРµРєРѕСЂСЂРµРєС‚РЅС‹Р№ РїСЂРѕС„РёР»СЊ РґРѕР»Р¶РµРЅ Р±С‹С‚СЊ РѕС‚РєР»РѕРЅРµРЅ");
}

void testFieldValidationRejectsBadOptions() {
    OcbField duplicateOption{"Duplicate option", ocb::core::FieldKind::OneOf, "Setup", 0x10, 8, {}, 0, {{0, "Off"}, {0, "Disabled"}}};
    expect(!ocb::core::validateField(duplicateOption), "Duplicate OneOf option values must be rejected");

    OcbField optionOutOfRange{"Out of range option", ocb::core::FieldKind::OneOf, "Setup", 0x10, 8, {}, 0, {{0, "Off"}, {256, "Too large"}}};
    expect(!ocb::core::validateField(optionOutOfRange), "OneOf option values must fit into the field width");

    OcbField booleanField{"Boolean option", ocb::core::FieldKind::OneOf, "Setup", 0x10, 8, {}, 0, {{0, "Disabled"}, {1, "Enabled"}}};
    expect(ocb::core::valueEditorKind(booleanField) == ocb::core::ValueEditorKind::Boolean, "0/1 OneOf fields should use a boolean editor");
}

void testProfileRejectsInvalidOneOfValue() {
    OcbProfile profile(syntheticProfileBytes());
    OcbField field{"Strict option", ocb::core::FieldKind::OneOf, "Setup", 0x20, 8, {}, 0, {{0, "Disabled"}, {1, "Enabled"}}};

    bool rejected = false;
    try {
        profile.write(field, 2);
    } catch (const ocb::core::OcbException&) {
        rejected = true;
    }
    expect(rejected, "Profile writes must reject values outside OneOf IFR options");
}

void testIfrTextParserReadsSetupMap() {
    std::ifstream input(testDataDir() / "Setup_IFR.txt");
    if (!input) {
        throw std::runtime_error("РћС‚СЃСѓС‚СЃС‚РІСѓРµС‚ fixture-С„Р°Р№Р» Setup_IFR.txt");
    }

    const auto questions = ocb::tools::ifr::IfrTextParser{}.parse(input);

    expect(questions.size() > 4000, "IFR-РїР°СЂСЃРµСЂ РґРѕР»Р¶РµРЅ РёР·РІР»РµС‡СЊ СЃРѕРїРѕСЃС‚Р°РІР»РµРЅРЅС‹Рµ РІРѕРїСЂРѕСЃС‹ Setup");

    const auto hasCpuCurrent = std::any_of(questions.begin(), questions.end(), [](const auto& question) {
        return question.prompt == "CPU Lite Load"
            && question.varStoreName == "Setup"
            && question.varOffset == 0xF64
            && question.sizeBits == 8
            && !question.options.empty();
    });
    expect(hasCpuCurrent, "IFR-РїР°СЂСЃРµСЂ РґРѕР»Р¶РµРЅ РёР·РІР»РµС‡СЊ РѕРїС†РёРё CPU Lite Load");
}

void testIfrQuestionsMapIntoFieldCatalog() {
    std::ifstream input(testDataDir() / "Setup_IFR.txt");
    if (!input) {
        throw std::runtime_error("РћС‚СЃСѓС‚СЃС‚РІСѓРµС‚ fixture-С„Р°Р№Р» Setup_IFR.txt");
    }

    const auto questions = ocb::tools::ifr::IfrTextParser{}.parse(input);
    const auto fields = ocb::core::IfrFieldMapper{}.mapQuestions(questions);

    expect(fields.size() > 4000, "РЎРѕРїРѕСЃС‚Р°РІРёС‚РµР»СЊ IFR РґРѕР»Р¶РµРЅ СЃРѕС…СЂР°РЅРёС‚СЊ РєРѕР»РёС‡РµСЃС‚РІРѕ СЃРѕРїРѕСЃС‚Р°РІР»РµРЅРЅС‹С… РїРѕР»РµР№");

    ocb::core::FieldCatalog catalog;
    catalog.merge(fields);

    const auto* liteLoad = catalog.findByPrompt("CPU Lite Load");
    expect(liteLoad != nullptr, "FieldCatalog РґРѕР»Р¶РµРЅ РЅР°Р№С‚Рё CPU Lite Load");
    expect(liteLoad->varStore == "Setup", "CPU Lite Load РґРѕР»Р¶РµРЅ СЃРѕРїРѕСЃС‚Р°РІР»СЏС‚СЊСЃСЏ СЃ Setup");
    expect(liteLoad->varOffset == 0xF64, "РЎРјРµС‰РµРЅРёРµ CPU Lite Load РґРѕР»Р¶РЅРѕ Р±С‹С‚СЊ 0xF64");
    expect(!liteLoad->options.empty(), "РЈ CPU Lite Load РґРѕР»Р¶РЅС‹ Р±С‹С‚СЊ РѕРїС†РёРё");

    const auto cepResults = catalog.search("CEP");
    expect(!cepResults.empty(), "РџРѕРёСЃРє FieldCatalog РґРѕР»Р¶РµРЅ РЅР°С…РѕРґРёС‚СЊ РїРѕР»СЏ CEP");
}

void testUefiExtractorFindsSetupPe32() {
    const auto bios = readAll(testDataDir() / "E7D89IMS.A91");
    const ocb::tools::uefi::NativeUefiExtractor extractor;
    const auto tree = extractor.parseImage(bios);
    const auto setup = extractor.findBestSetupModule(tree);

    expect(setup.has_value(), "UEFI-СЌРєСЃС‚СЂР°РєС‚РѕСЂ РґРѕР»Р¶РµРЅ РЅР°Р№С‚Рё РјРѕРґСѓР»СЊ Setup PE32");
    expect(setup->pe32Body.size() > 2'000'000, "РњРѕРґСѓР»СЊ Setup PE32 РґРѕР»Р¶РµРЅ Р±С‹С‚СЊ РєСЂСѓРїРЅС‹Рј С‚РµР»РѕРј setup");
}

void testNativeIfrExtractorReadsSetupPe32() {
    const auto bios = readAll(testDataDir() / "E7D89IMS.A91");
    const ocb::tools::uefi::NativeUefiExtractor uefiExtractor;
    const auto tree = uefiExtractor.parseImage(bios);
    const auto setup = uefiExtractor.findBestSetupModule(tree);
    expect(setup.has_value(), "UEFI-СЌРєСЃС‚СЂР°РєС‚РѕСЂ РґРѕР»Р¶РµРЅ РЅР°Р№С‚Рё РјРѕРґСѓР»СЊ Setup PE32");

    const auto questions = ocb::tools::ifr::NativeIfrExtractor{}.extractQuestions(setup->pe32Body);
    expect(questions.size() > 4000, "РќР°С‚РёРІРЅС‹Р№ IFR-СЌРєСЃС‚СЂР°РєС‚РѕСЂ РґРѕР»Р¶РµРЅ РїСЂРѕС‡РёС‚Р°С‚СЊ РІРѕРїСЂРѕСЃС‹ Setup РёР· BIOS");

    const auto fields = ocb::core::IfrFieldMapper{}.mapQuestions(questions);
    ocb::core::FieldCatalog catalog;
    catalog.merge(fields);

    const auto* liteLoad = catalog.findByPrompt("CPU Lite Load");
    expect(liteLoad != nullptr, "РќР°С‚РёРІРЅС‹Р№ РєРѕРЅРІРµР№РµСЂ BIOS IFR РґРѕР»Р¶РµРЅ РЅР°Р№С‚Рё CPU Lite Load");
    expect(liteLoad->varStore == "Setup", "РќР°С‚РёРІРЅС‹Р№ CPU Lite Load РґРѕР»Р¶РµРЅ СЃРѕРїРѕСЃС‚Р°РІР»СЏС‚СЊСЃСЏ СЃ Setup");
    expect(liteLoad->varOffset == 0xF64, "РЎРјРµС‰РµРЅРёРµ РЅР°С‚РёРІРЅРѕРіРѕ CPU Lite Load РґРѕР»Р¶РЅРѕ Р±С‹С‚СЊ 0xF64");
    expect(!liteLoad->options.empty(), "РЈ РЅР°С‚РёРІРЅРѕРіРѕ CPU Lite Load РґРѕР»Р¶РЅС‹ Р±С‹С‚СЊ РѕРїС†РёРё");
}

void testNativeIfrExtractorReadsSyntheticHii() {
    const auto questions = ocb::tools::ifr::NativeIfrExtractor{}.extractQuestions(syntheticHiiPackageList());
    expect(questions.size() == 1, "Synthetic HII package list should produce one IFR question");

    const auto& question = questions.front();
    expect(question.kind == ocb::tools::ifr::IfrQuestionKind::OneOf, "Synthetic question should be OneOf");
    expect(question.prompt == "Synthetic Question", "Synthetic question prompt should come from UCS-2 strings");
    expect(question.questionId == 0x1234, "Synthetic question id should be parsed");
    expect(question.varStoreId == 1, "Synthetic VarStore id should be parsed");
    expect(question.varStoreName == "Setup", "Synthetic VarStore name should be parsed");
    expect(question.varOffset == 0x00F6, "Synthetic VarOffset should be parsed");
    expect(question.sizeBits == 8, "Synthetic OneOf size should be parsed");
    expect(question.options.size() == 2, "Synthetic OneOf options should be parsed");
    expect(question.options[0].value == 0 && question.options[0].label == "Disabled", "Synthetic first option should match");
    expect(question.options[1].value == 1 && question.options[1].label == "Enabled", "Synthetic second option should match");
}

} // namespace

int main() {
    try {
        std::cout << "running testSyntheticProfileMetadata" << std::endl;
        testSyntheticProfileMetadata();
        std::cout << "running testProfileMetadataFallsBackFromBiosId" << std::endl;
        testProfileMetadataFallsBackFromBiosId();
        std::cout << "running testBiosAnalysisMetadata" << std::endl;
        testBiosAnalysisMetadata();
        std::cout << "running testReadKnownValues" << std::endl;
        testReadKnownValues();
        std::cout << "running testConservativePresetMatchesWorkingTry02" << std::endl;
        testConservativePresetMatchesWorkingTry02();
        std::cout << "running testUnchangedProfileExportsByteIdentical" << std::endl;
        testUnchangedProfileExportsByteIdentical();
        std::cout << "running testPresetFileRoundTrip" << std::endl;
        testPresetFileRoundTrip();
        std::cout << "running testPresetFileAcceptsStringValues" << std::endl;
        testPresetFileAcceptsStringValues();
        std::cout << "running testPresetAppliesPromptsWithSpacingVariants" << std::endl;
        testPresetAppliesPromptsWithSpacingVariants();
        std::cout << "running testPresetAppliesCommonAliasPrompts" << std::endl;
        testPresetAppliesCommonAliasPrompts();
        std::cout << "running testPresetAppliesAgainstExtendedCatalog" << std::endl;
        testPresetAppliesAgainstExtendedCatalog();
        std::cout << "running testInvalidInputRejected" << std::endl;
        testInvalidInputRejected();
        std::cout << "running testFieldValidationRejectsBadOptions" << std::endl;
        testFieldValidationRejectsBadOptions();
        std::cout << "running testProfileRejectsInvalidOneOfValue" << std::endl;
        testProfileRejectsInvalidOneOfValue();
        std::cout << "running testIfrTextParserReadsSetupMap" << std::endl;
        testIfrTextParserReadsSetupMap();
        std::cout << "running testIfrQuestionsMapIntoFieldCatalog" << std::endl;
        testIfrQuestionsMapIntoFieldCatalog();
        std::cout << "running testUefiExtractorFindsSetupPe32" << std::endl;
        testUefiExtractorFindsSetupPe32();
        std::cout << "running testNativeIfrExtractorReadsSetupPe32" << std::endl;
        testNativeIfrExtractorReadsSetupPe32();
        std::cout << "running testNativeIfrExtractorReadsSyntheticHii" << std::endl;
        testNativeIfrExtractorReadsSyntheticHii();
    } catch (const std::exception& error) {
        std::cerr << "РћРЁРР‘РљРђ: " << error.what() << '\n';
        return 1;
    }

    std::cout << "ocb_core_tests: СѓСЃРїРµС€РЅРѕ\n";
    return 0;
}
