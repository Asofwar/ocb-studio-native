#include "ocb/core/ChecksumCompensator.hpp"
#include "ocb/core/FieldCatalog.hpp"
#include "ocb/core/IfrFieldMapper.hpp"
#include "ocb/core/OcbException.hpp"
#include "ocb/core/OcbProfile.hpp"
#include "ocb/core/Preset.hpp"
#include "ocb/tools/ifr/IfrTextParser.hpp"
#include "ocb/tools/ifr/NativeIfrExtractor.hpp"
#include "ocb/tools/uefi/UefiExtractor.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

using ocb::core::OcbField;
using ocb::core::OcbProfile;

std::filesystem::path testDataDir() {
    return std::filesystem::path(OCB_TEST_DATA_DIR);
}

std::vector<std::uint8_t> readAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to read fixture: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

const OcbField& fieldByPrompt(const std::string& prompt) {
    const auto& fields = ocb::core::builtinFields();
    const auto it = std::find_if(fields.begin(), fields.end(), [&](const OcbField& field) {
        return field.prompt == prompt;
    });
    if (it == fields.end()) {
        throw std::runtime_error("Missing builtin field: " + prompt);
    }
    return *it;
}

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testReadKnownValues() {
    const auto profile = OcbProfile::loadFromFile(testDataDir() / "MsOcFile.ocb");

    expect(profile.read(fieldByPrompt("Long Duration Power Limit (W)")) == 200, "PL1 should be 200 W");
    expect(profile.read(fieldByPrompt("Short Duration Power Limit (W)")) == 220, "PL2 should be 220 W");
    expect(profile.read(fieldByPrompt("CPU Current Limit (A)")) == 502, "CPU current should be 502 A");
    expect(profile.read(fieldByPrompt("CPU Lite Load")) == 30, "CPU Lite Load should be Mode 4 value");
    expect(profile.read(fieldByPrompt("Game Boost")) == 0, "Game Boost should be disabled");
}

void testConservativePresetMatchesWorkingTry02() {
    auto profile = OcbProfile::loadFromFile(testDataDir() / "MsOcFile.ocb");
    ocb::core::applyPreset(profile, "Conservative 200/220W 307A");

    const auto produced = profile.exportBytes(true);
    const auto expected = readAll(testDataDir() / "try_02_conservative_sum_comp" / "MsOcFile.ocb");

    expect(produced == expected, "Conservative preset must match known BIOS-accepted try02 output byte-for-byte");
    expect(
        ocb::core::ChecksumCompensator::compute(produced) == profile.targetSums(),
        "Checksum-style sums should match the loaded original profile");
}

void testInvalidInputRejected() {
    bool rejected = false;
    try {
        OcbProfile profile(std::vector<std::uint8_t>(128, 0));
    } catch (const ocb::core::OcbException&) {
        rejected = true;
    }
    expect(rejected, "Invalid profile must be rejected");
}

void testIfrTextParserReadsSetupMap() {
    std::ifstream input(testDataDir() / "Setup_IFR.txt");
    if (!input) {
        throw std::runtime_error("Missing Setup_IFR.txt fixture");
    }

    const auto questions = ocb::tools::ifr::IfrTextParser{}.parse(input);

    expect(questions.size() > 4000, "IFR parser should extract the mapped Setup questions");

    const auto hasCpuCurrent = std::any_of(questions.begin(), questions.end(), [](const auto& question) {
        return question.prompt == "CPU Lite Load"
            && question.varStoreName == "Setup"
            && question.varOffset == 0xF64
            && question.sizeBits == 8
            && !question.options.empty();
    });
    expect(hasCpuCurrent, "IFR parser should extract CPU Lite Load options");
}

void testIfrQuestionsMapIntoFieldCatalog() {
    std::ifstream input(testDataDir() / "Setup_IFR.txt");
    if (!input) {
        throw std::runtime_error("Missing Setup_IFR.txt fixture");
    }

    const auto questions = ocb::tools::ifr::IfrTextParser{}.parse(input);
    const auto fields = ocb::core::IfrFieldMapper{}.mapQuestions(questions);

    expect(fields.size() > 4000, "IFR mapper should preserve mapped field count");

    ocb::core::FieldCatalog catalog;
    catalog.merge(fields);

    const auto* liteLoad = catalog.findByPrompt("CPU Lite Load");
    expect(liteLoad != nullptr, "FieldCatalog should find CPU Lite Load");
    expect(liteLoad->varStore == "Setup", "CPU Lite Load should map to Setup");
    expect(liteLoad->varOffset == 0xF64, "CPU Lite Load offset should be 0xF64");
    expect(!liteLoad->options.empty(), "CPU Lite Load should have options");

    const auto cepResults = catalog.search("CEP");
    expect(!cepResults.empty(), "FieldCatalog search should find CEP fields");
}

void testUefiExtractorFindsSetupPe32() {
    const auto bios = readAll(testDataDir() / "E7D89IMS.A91");
    const ocb::tools::uefi::UefiToolExtractor extractor;
    const auto tree = extractor.parseImage(bios);
    const auto setup = extractor.findBestSetupModule(tree);

    expect(setup.has_value(), "UEFI extractor should find Setup PE32 module");
    expect(setup->pe32Body.size() > 2'000'000, "Setup PE32 module should be the large setup body");
}

void testNativeIfrExtractorReadsSetupPe32() {
    const auto bios = readAll(testDataDir() / "E7D89IMS.A91");
    const ocb::tools::uefi::UefiToolExtractor uefiExtractor;
    const auto tree = uefiExtractor.parseImage(bios);
    const auto setup = uefiExtractor.findBestSetupModule(tree);
    expect(setup.has_value(), "UEFI extractor should find Setup PE32 module");

    const auto questions = ocb::tools::ifr::NativeIfrExtractor{}.extractQuestions(setup->pe32Body);
    expect(questions.size() > 4000, "Native IFR extractor should read Setup questions from BIOS");

    const auto fields = ocb::core::IfrFieldMapper{}.mapQuestions(questions);
    ocb::core::FieldCatalog catalog;
    catalog.merge(fields);

    const auto* liteLoad = catalog.findByPrompt("CPU Lite Load");
    expect(liteLoad != nullptr, "Native BIOS IFR pipeline should find CPU Lite Load");
    expect(liteLoad->varStore == "Setup", "Native CPU Lite Load should map to Setup");
    expect(liteLoad->varOffset == 0xF64, "Native CPU Lite Load offset should be 0xF64");
    expect(!liteLoad->options.empty(), "Native CPU Lite Load should have options");
}

} // namespace

int main() {
    try {
        testReadKnownValues();
        testConservativePresetMatchesWorkingTry02();
        testInvalidInputRejected();
        testIfrTextParserReadsSetupMap();
        testIfrQuestionsMapIntoFieldCatalog();
        testUefiExtractorFindsSetupPe32();
        testNativeIfrExtractorReadsSetupPe32();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }

    std::cout << "ocb_core_tests: OK\n";
    return 0;
}
