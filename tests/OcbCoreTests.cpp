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
        throw std::runtime_error("Не удалось прочитать fixture-файл: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

const OcbField& fieldByPrompt(const std::string& prompt) {
    const auto& fields = ocb::core::builtinFields();
    const auto it = std::find_if(fields.begin(), fields.end(), [&](const OcbField& field) {
        return field.prompt == prompt;
    });
    if (it == fields.end()) {
        throw std::runtime_error("Отсутствует встроенное поле: " + prompt);
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

    expect(profile.read(fieldByPrompt("Long Duration Power Limit (W)")) == 200, "PL1 должен быть 200 W");
    expect(profile.read(fieldByPrompt("Short Duration Power Limit (W)")) == 220, "PL2 должен быть 220 W");
    expect(profile.read(fieldByPrompt("CPU Current Limit (A)")) == 502, "Лимит тока CPU должен быть 502 A");
    expect(profile.read(fieldByPrompt("CPU Lite Load")) == 30, "CPU Lite Load должен иметь значение режима 4");
    expect(profile.read(fieldByPrompt("Game Boost")) == 0, "Game Boost должен быть отключен");
}

void testConservativePresetMatchesWorkingTry02() {
    auto profile = OcbProfile::loadFromFile(testDataDir() / "MsOcFile.ocb");
    ocb::core::applyPreset(profile, "Консервативный 200/220W 307A");

    const auto produced = profile.exportBytes(true);
    const auto expected = readAll(testDataDir() / "try_02_conservative_sum_comp" / "MsOcFile.ocb");

    expect(produced == expected, "Консервативный пресет должен побайтно совпадать с известным выводом try02, принятым BIOS");
    expect(
        ocb::core::ChecksumCompensator::compute(produced) == profile.targetSums(),
        "Суммы в стиле контрольных сумм должны совпадать с загруженным оригинальным профилем");
}

void testInvalidInputRejected() {
    bool rejected = false;
    try {
        OcbProfile profile(std::vector<std::uint8_t>(128, 0));
    } catch (const ocb::core::OcbException&) {
        rejected = true;
    }
    expect(rejected, "Некорректный профиль должен быть отклонен");
}

void testIfrTextParserReadsSetupMap() {
    std::ifstream input(testDataDir() / "Setup_IFR.txt");
    if (!input) {
        throw std::runtime_error("Отсутствует fixture-файл Setup_IFR.txt");
    }

    const auto questions = ocb::tools::ifr::IfrTextParser{}.parse(input);

    expect(questions.size() > 4000, "IFR-парсер должен извлечь сопоставленные вопросы Setup");

    const auto hasCpuCurrent = std::any_of(questions.begin(), questions.end(), [](const auto& question) {
        return question.prompt == "CPU Lite Load"
            && question.varStoreName == "Setup"
            && question.varOffset == 0xF64
            && question.sizeBits == 8
            && !question.options.empty();
    });
    expect(hasCpuCurrent, "IFR-парсер должен извлечь опции CPU Lite Load");
}

void testIfrQuestionsMapIntoFieldCatalog() {
    std::ifstream input(testDataDir() / "Setup_IFR.txt");
    if (!input) {
        throw std::runtime_error("Отсутствует fixture-файл Setup_IFR.txt");
    }

    const auto questions = ocb::tools::ifr::IfrTextParser{}.parse(input);
    const auto fields = ocb::core::IfrFieldMapper{}.mapQuestions(questions);

    expect(fields.size() > 4000, "Сопоставитель IFR должен сохранить количество сопоставленных полей");

    ocb::core::FieldCatalog catalog;
    catalog.merge(fields);

    const auto* liteLoad = catalog.findByPrompt("CPU Lite Load");
    expect(liteLoad != nullptr, "FieldCatalog должен найти CPU Lite Load");
    expect(liteLoad->varStore == "Setup", "CPU Lite Load должен сопоставляться с Setup");
    expect(liteLoad->varOffset == 0xF64, "Смещение CPU Lite Load должно быть 0xF64");
    expect(!liteLoad->options.empty(), "У CPU Lite Load должны быть опции");

    const auto cepResults = catalog.search("CEP");
    expect(!cepResults.empty(), "Поиск FieldCatalog должен находить поля CEP");
}

void testUefiExtractorFindsSetupPe32() {
    const auto bios = readAll(testDataDir() / "E7D89IMS.A91");
    const ocb::tools::uefi::UefiToolExtractor extractor;
    const auto tree = extractor.parseImage(bios);
    const auto setup = extractor.findBestSetupModule(tree);

    expect(setup.has_value(), "UEFI-экстрактор должен найти модуль Setup PE32");
    expect(setup->pe32Body.size() > 2'000'000, "Модуль Setup PE32 должен быть крупным телом setup");
}

void testNativeIfrExtractorReadsSetupPe32() {
    const auto bios = readAll(testDataDir() / "E7D89IMS.A91");
    const ocb::tools::uefi::UefiToolExtractor uefiExtractor;
    const auto tree = uefiExtractor.parseImage(bios);
    const auto setup = uefiExtractor.findBestSetupModule(tree);
    expect(setup.has_value(), "UEFI-экстрактор должен найти модуль Setup PE32");

    const auto questions = ocb::tools::ifr::NativeIfrExtractor{}.extractQuestions(setup->pe32Body);
    expect(questions.size() > 4000, "Нативный IFR-экстрактор должен прочитать вопросы Setup из BIOS");

    const auto fields = ocb::core::IfrFieldMapper{}.mapQuestions(questions);
    ocb::core::FieldCatalog catalog;
    catalog.merge(fields);

    const auto* liteLoad = catalog.findByPrompt("CPU Lite Load");
    expect(liteLoad != nullptr, "Нативный конвейер BIOS IFR должен найти CPU Lite Load");
    expect(liteLoad->varStore == "Setup", "Нативный CPU Lite Load должен сопоставляться с Setup");
    expect(liteLoad->varOffset == 0xF64, "Смещение нативного CPU Lite Load должно быть 0xF64");
    expect(!liteLoad->options.empty(), "У нативного CPU Lite Load должны быть опции");
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
        std::cerr << "ОШИБКА: " << error.what() << '\n';
        return 1;
    }

    std::cout << "ocb_core_tests: успешно\n";
    return 0;
}
