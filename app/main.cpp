#include "AppController.hpp"
#include "ocb/BuildInfo.hpp"
#include "ocb/core/OcbException.hpp"
#include "ocb/core/FieldValidation.hpp"
#include "ocb/core/PresetFile.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <fcntl.h>
#include <io.h>
#include <shellapi.h>
#endif

#include <GLFW/glfw3.h>

namespace {

constexpr std::size_t pathBufferSize = 1024;
constexpr std::size_t valueBufferSize = 64;
constexpr float defaultFontSize = 16.0F;

struct CliOptions {
    std::optional<std::filesystem::path> input;
    std::optional<std::filesystem::path> output;
    std::optional<std::string> presetName;
    std::optional<std::filesystem::path> presetFile;
    bool compensateChecksums{true};
};

struct GuiState {
    ocb::AppController controller;
    std::vector<ocb::core::Preset> importedPresets;
    std::string selectedFieldId;
    std::string status = "Ready.";
    std::string error;
    std::array<char, pathBufferSize> ocbPath{};
    std::array<char, pathBufferSize> biosPath{};
    std::array<char, pathBufferSize> ifrPath{};
    std::array<char, pathBufferSize> outputPath{};
    std::array<char, pathBufferSize> presetPath{};
    std::array<char, pathBufferSize> search{};
    std::array<char, valueBufferSize> value{};
    int selectedPreset{};
    bool compensateChecksums{true};
};

void setBuffer(std::array<char, pathBufferSize>& buffer, const std::string& value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
}

void setBuffer(std::array<char, valueBufferSize>& buffer, const std::string& value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
}

[[nodiscard]] std::string bufferText(const auto& buffer) {
    return std::string(buffer.data());
}

#if defined(_WIN32)
[[nodiscard]] std::string utf8FromWide(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        throw ocb::core::OcbException("Failed to convert text to UTF-8.");
    }

    std::string output(static_cast<std::size_t>(size), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), output.data(), size, nullptr, nullptr);
    if (written != size) {
        throw ocb::core::OcbException("Failed to convert text to UTF-8.");
    }
    return output;
}

[[nodiscard]] std::wstring wideFromUtf8(std::string_view value) {
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        throw ocb::core::OcbException("Failed to convert text from UTF-8.");
    }

    std::wstring output(static_cast<std::size_t>(size), L'\0');
    const int written = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), output.data(), size);
    if (written != size) {
        throw ocb::core::OcbException("Failed to convert text from UTF-8.");
    }
    return output;
}

[[nodiscard]] std::filesystem::path pathFromUtf8(std::string_view value) {
    return std::filesystem::path(wideFromUtf8(value));
}

[[nodiscard]] std::optional<std::string> nativeFileDialog(
    bool save,
    const wchar_t* title,
    const wchar_t* filter,
    const std::string& defaultPath = {}) {
    std::array<wchar_t, 32768> path{};
    if (!defaultPath.empty()) {
        const auto widePath = wideFromUtf8(defaultPath);
        wcsncpy_s(path.data(), path.size(), widePath.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path.data();
    ofn.nMaxFile = static_cast<DWORD>(path.size());
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!save) {
        ofn.Flags |= OFN_FILEMUSTEXIST;
    } else {
        ofn.Flags |= OFN_OVERWRITEPROMPT;
    }

    const BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
    if (!ok) {
        return std::nullopt;
    }
    return utf8FromWide(path.data());
}

[[nodiscard]] bool fileExists(const wchar_t* path) {
    std::ifstream input(std::filesystem::path(path), std::ios::binary);
    return input.good();
}

void attachConsoleForCli() {
    auto hasUsableHandle = [](DWORD id) {
        const HANDLE handle = GetStdHandle(id);
        return handle != nullptr && handle != INVALID_HANDLE_VALUE && GetFileType(handle) != FILE_TYPE_UNKNOWN;
    };

    if (hasUsableHandle(STD_OUTPUT_HANDLE) || hasUsableHandle(STD_ERROR_HANDLE)) {
        return;
    }

    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        (void)AllocConsole();
    }

    auto bindStream = [](DWORD id, FILE* stream, int fd, int flags) {
        const HANDLE handle = GetStdHandle(id);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE || GetFileType(handle) == FILE_TYPE_UNKNOWN) {
            return;
        }
        const int osHandle = _open_osfhandle(reinterpret_cast<intptr_t>(handle), flags);
        if (osHandle < 0) {
            return;
        }
        (void)_dup2(osHandle, fd);
        (void)_close(osHandle);
        setvbuf(stream, nullptr, _IONBF, 0);
    };

    bindStream(STD_OUTPUT_HANDLE, stdout, _fileno(stdout), _O_TEXT);
    bindStream(STD_ERROR_HANDLE, stderr, _fileno(stderr), _O_TEXT);
    bindStream(STD_INPUT_HANDLE, stdin, _fileno(stdin), _O_TEXT);
    std::ios::sync_with_stdio(true);
}
#else
[[nodiscard]] std::filesystem::path pathFromUtf8(std::string_view value) {
    return std::filesystem::path(std::string(value));
}

[[nodiscard]] std::optional<std::string> nativeFileDialog(bool, const char*, const char*, const std::string& = {}) {
    return std::nullopt;
}

[[nodiscard]] bool fileExists(const char* path) {
    std::ifstream input(path, std::ios::binary);
    return input.good();
}
#endif

void printHelp(std::ostream& out) {
    out
        << "OCB Studio " << OCB_BUILD_VERSION << "\n"
        << "Dear ImGui MSI OCB profile tool with command line fallback.\n\n"
        << "Run without arguments to open the GUI.\n\n"
        << "Usage:\n"
        << "  ocb_studio --list-presets\n"
        << "  ocb_studio --export-preset <preset-name> --output <file.ocbpreset>\n"
        << "  ocb_studio --input <MsOcFile.ocb> --output <out.ocb> --preset <preset-name>\n"
        << "  ocb_studio --input <MsOcFile.ocb> --output <out.ocb> --preset-file <file.ocbpreset>\n"
        << "  ocb_studio --input <MsOcFile.ocb> --output <out.ocb> --write <field-id-or-prompt> <value>\n\n"
        << "Options:\n"
        << "  --no-compensate       Save without checksum compensation.\n"
        << "  --version             Print version.\n"
        << "  --help                Print this help.\n";
}

[[nodiscard]] std::uint64_t parseUnsigned(std::string_view text) {
    std::uint64_t value = 0;
    const int base = text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0 ? 16 : 10;
    const auto input = base == 16 ? text.substr(2) : text;
    const auto result = std::from_chars(input.data(), input.data() + input.size(), value, base);
    if (input.empty() || result.ec != std::errc{} || result.ptr != input.data() + input.size()) {
        throw ocb::core::OcbException("Invalid numeric value: " + std::string(text));
    }
    return value;
}

[[nodiscard]] std::optional<std::uint64_t> tryParseUnsigned(std::string_view text) {
    try {
        return parseUnsigned(text);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

[[nodiscard]] const ocb::core::Preset* findBuiltinPreset(std::string_view name) {
    const auto& presets = ocb::core::builtinPresets();
    const auto found = std::find_if(presets.begin(), presets.end(), [&](const ocb::core::Preset& preset) {
        return preset.name == name;
    });
    return found == presets.end() ? nullptr : &*found;
}

[[nodiscard]] const ocb::core::Preset* selectedPreset(const GuiState& state) {
    if (state.selectedPreset <= 0) {
        return nullptr;
    }

    const int builtinIndex = state.selectedPreset - 1;
    const auto& builtin = ocb::core::builtinPresets();
    if (builtinIndex < static_cast<int>(builtin.size())) {
        return &builtin.at(static_cast<std::size_t>(builtinIndex));
    }

    const int importedIndex = builtinIndex - static_cast<int>(builtin.size());
    if (importedIndex < 0 || importedIndex >= static_cast<int>(state.importedPresets.size())) {
        return nullptr;
    }
    return &state.importedPresets.at(static_cast<std::size_t>(importedIndex));
}

[[nodiscard]] std::string fileStemFromPath(const std::string& pathText) {
    if (pathText.empty()) {
        return "Current profile";
    }

    auto path = pathFromUtf8(pathText);
    const auto stem = path.stem();
#if defined(_WIN32)
    auto name = utf8FromWide(stem.wstring());
#else
    auto name = stem.string();
#endif
    return name.empty() ? "Current profile" : name;
}

[[nodiscard]] std::string pathWithDefaultExtension(const std::string& pathText, std::string_view extension) {
    if (pathText.empty()) {
        return pathText;
    }

    auto path = pathFromUtf8(pathText);
    if (!path.has_extension()) {
        path += extension;
    }
#if defined(_WIN32)
    return utf8FromWide(path.wstring());
#else
    return path.string();
#endif
}

[[nodiscard]] ocb::core::Preset presetFromCurrentProfile(const GuiState& state) {
    if (!state.controller.hasProfile()) {
        throw ocb::core::OcbException("OCB profile is not loaded.");
    }

    ocb::core::Preset preset;
    preset.name = fileStemFromPath(bufferText(state.presetPath));
    for (const auto& field : state.controller.catalog().fields()) {
        preset.valuesByPrompt[field.prompt] = state.controller.profile().read(field);
    }
    if (preset.valuesByPrompt.empty()) {
        throw ocb::core::OcbException("Field catalog is empty.");
    }
    return preset;
}

void setStatus(GuiState& state, std::string message) {
    state.status = std::move(message);
    state.error.clear();
}

void setError(GuiState& state, const std::exception& error) {
    state.error = error.what();
}

[[nodiscard]] const ocb::core::OcbOption* optionForValue(
    const ocb::core::OcbField& field,
    std::uint64_t value) {
    const auto found = std::find_if(field.options.begin(), field.options.end(), [&](const auto& option) {
        return option.value == value;
    });
    return found == field.options.end() ? nullptr : &*found;
}

[[nodiscard]] std::string formatValue(const ocb::core::OcbField& field, std::uint64_t value) {
    std::string text = std::to_string(value);
    if (const auto* option = optionForValue(field, value); option != nullptr && !option->label.empty()) {
        text += " = " + option->label;
    }
    return text;
}

[[nodiscard]] std::string formatOptionsPreview(const ocb::core::OcbField& field) {
    if (field.options.empty()) {
        return field.kind == ocb::core::FieldKind::OneOf ? "No IFR options" : "Numeric";
    }

    std::string text;
    const auto limit = std::min<std::size_t>(field.options.size(), 4);
    for (std::size_t i = 0; i < limit; ++i) {
        if (!text.empty()) {
            text += ", ";
        }
        text += std::to_string(field.options[i].value);
        if (!field.options[i].label.empty()) {
            text += "=" + field.options[i].label;
        }
    }
    if (field.options.size() > limit) {
        text += ", ...";
    }
    return text;
}

[[nodiscard]] std::string lowerAscii(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const auto ch : text) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return result;
}

[[nodiscard]] bool containsAny(std::string_view text, std::initializer_list<std::string_view> needles) {
    const auto lowered = lowerAscii(text);
    return std::any_of(needles.begin(), needles.end(), [&](std::string_view needle) {
        return lowered.find(needle) != std::string::npos;
    });
}

[[nodiscard]] std::string fieldGuidance(const ocb::core::OcbField& field) {
    if (!field.help.empty()) {
        return field.help;
    }

    const auto& prompt = field.prompt;
    if (containsAny(prompt, {"avx"})) {
        return "Настройки AVX влияют на частоты и поведение процессора под AVX-нагрузкой. Обычно больший offset снижает частоту в тяжелых AVX-задачах, уменьшая нагрев и риск нестабильности.";
    }
    if (containsAny(prompt, {"cep"})) {
        return "CEP (Current Excursion Protection) контролирует защитное поведение CPU при просадках напряжения и всплесках тока. Отключение может помочь разгону/undervolt, но повышает риск нестабильности и некорректного поведения под нагрузкой.";
    }
    if (containsAny(prompt, {"icc", "current limit"})) {
        return "Лимит тока ограничивает максимальный ток, который CPU может запросить у VRM. Более высокие значения уменьшают throttling, но повышают нагрузку на питание и температуру.";
    }
    if (containsAny(prompt, {"power limit", "pl1", "pl2", "long duration", "short duration"})) {
        return "Лимит мощности задает, сколько ватт процессор может потреблять в длительной или кратковременной нагрузке. Больше - выше производительность и нагрев; меньше - тише и безопаснее.";
    }
    if (containsAny(prompt, {"lite load", "loadline", "load line", "ac loadline", "dc loadline"})) {
        return "Loadline влияет на расчет и поведение напряжения под нагрузкой. Меньшие/более агрессивные значения могут снизить напряжение и температуру, но требуют проверки стабильности.";
    }
    if (containsAny(prompt, {"ratio", "multiplier", "turbo", "e-core", "p-core", "ring"})) {
        return "Ratio/множитель влияет на частоту соответствующего домена CPU. Более высокое значение повышает производительность, но требует больше напряжения и охлаждения.";
    }
    if (containsAny(prompt, {"boost"})) {
        return "Boost-настройки обычно включают автоматическое повышение частот. Это может дать производительность ценой температуры, напряжения и потребления.";
    }
    if (containsAny(prompt, {"boot option"})) {
        return "Boot Option задает порядок загрузки. Для разгона CPU обычно не требуется менять эти поля.";
    }
    if (field.kind == ocb::core::FieldKind::OneOf) {
        return "Поле с фиксированным набором вариантов из BIOS IFR. Выбирайте вариант по описанию ниже; если назначение неизвестно, безопаснее оставить текущее значение.";
    }
    return "Числовое поле BIOS Setup. Меняйте только если понимаете диапазон и влияние параметра; перед экспериментами сохраните исходный профиль.";
}

[[nodiscard]] std::string optionGuidance(const ocb::core::OcbField& field, const ocb::core::OcbOption& option) {
    const auto label = lowerAscii(option.label);
    if (label.find("auto") != std::string::npos || option.label == "Авто") {
        return "BIOS сам выбирает значение по своей логике. Обычно это самый безопасный вариант для неизвестного параметра.";
    }
    if (label.find("enabled") != std::string::npos || option.label == "Включено") {
        return "Включает функцию. Эффект зависит от поля; для защитных функций это обычно безопаснее, для boost/разгона может повысить нагрев.";
    }
    if (label.find("disabled") != std::string::npos || option.label == "Отключено") {
        return "Отключает функцию. Для защитных функций это может повысить риск нестабильности; для boost-функций обычно снижает агрессивность.";
    }
    if (containsAny(field.prompt, {"lite load"}) && option.value > 0) {
        return "Режим CPU Lite Load. Обычно меньшие режимы дают меньше напряжения и температуры, но требуют стресс-теста.";
    }
    if (containsAny(field.prompt, {"ratio", "multiplier", "ring", "e-core", "p-core"})) {
        return "Значение связано с множителем/частотой. Чем выше, тем выше потенциальная производительность и требования к стабильности.";
    }
    return "Описание эффекта не найдено в IFR. Сравните с текущим значением и документацией BIOS/платы перед применением.";
}

void listPresets() {
    for (const auto& preset : ocb::core::builtinPresets()) {
        std::cout << preset.name << '\n';
    }
}

void exportPreset(const std::string& name, const std::filesystem::path& path) {
    const auto* preset = findBuiltinPreset(name);
    if (preset == nullptr) {
        throw ocb::core::OcbException("Unknown built-in preset: " + name);
    }
    ocb::core::savePresetToFile(path, *preset);
}

void applyPresetToFile(const CliOptions& options) {
    if (!options.input || !options.output) {
        throw ocb::core::OcbException("--input and --output are required when applying a preset.");
    }
    if (options.presetName.has_value() == options.presetFile.has_value()) {
        throw ocb::core::OcbException("Choose exactly one preset source: --preset or --preset-file.");
    }

    ocb::AppController controller;
    controller.openOcb(*options.input);
    if (options.presetFile) {
        controller.applyPreset(ocb::core::loadPresetFromFile(*options.presetFile));
    } else {
        controller.applyPreset(*options.presetName);
    }
    controller.saveOcb(*options.output, options.compensateChecksums);
}

void writeFieldToFile(const CliOptions& options, std::string_view fieldName, std::uint64_t value) {
    if (!options.input || !options.output) {
        throw ocb::core::OcbException("--input and --output are required when writing a field.");
    }

    ocb::AppController controller;
    controller.openOcb(*options.input);

    const auto* field = controller.catalog().findById(fieldName);
    if (field == nullptr) {
        field = controller.catalog().findByPrompt(fieldName);
    }
    if (field == nullptr) {
        throw ocb::core::OcbException("Unknown field: " + std::string(fieldName));
    }

    controller.writeField(field->id(), value);
    controller.saveOcb(*options.output, options.compensateChecksums);
}

[[nodiscard]] std::string requireValue(const std::vector<std::string>& args, std::size_t& index, std::string_view option) {
    if (index + 1 >= args.size()) {
        throw ocb::core::OcbException("Missing value for " + std::string(option));
    }
    ++index;
    return args[index];
}

int runCli(std::vector<std::string> args) {
    CliOptions options;
    std::optional<std::pair<std::string, std::uint64_t>> writeRequest;
    bool listRequested = false;
    bool exportRequested = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == "--help" || arg == "-h") {
            printHelp(std::cout);
            return 0;
        }
        if (arg == "--version") {
            std::cout << OCB_BUILD_VERSION << '\n';
            return 0;
        }
        if (arg == "--list-presets") {
            listRequested = true;
        } else if (arg == "--input") {
            options.input = pathFromUtf8(requireValue(args, i, arg));
        } else if (arg == "--output") {
            options.output = pathFromUtf8(requireValue(args, i, arg));
        } else if (arg == "--preset") {
            options.presetName = requireValue(args, i, arg);
        } else if (arg == "--preset-file") {
            options.presetFile = pathFromUtf8(requireValue(args, i, arg));
        } else if (arg == "--export-preset") {
            options.presetName = requireValue(args, i, arg);
            exportRequested = true;
        } else if (arg == "--write") {
            const auto field = requireValue(args, i, arg);
            const auto value = parseUnsigned(requireValue(args, i, arg));
            writeRequest = {field, value};
        } else if (arg == "--no-compensate") {
            options.compensateChecksums = false;
        } else {
            throw ocb::core::OcbException("Unknown option: " + arg);
        }
    }

    if (listRequested) {
        listPresets();
        return 0;
    }
    if (exportRequested) {
        if (!options.presetName || !options.output) {
            throw ocb::core::OcbException("--export-preset requires a preset name and --output.");
        }
        exportPreset(*options.presetName, *options.output);
        return 0;
    }
    if (writeRequest) {
        writeFieldToFile(options, writeRequest->first, writeRequest->second);
        return 0;
    }
    if (options.presetName || options.presetFile) {
        applyPresetToFile(options);
        return 0;
    }

    printHelp(std::cout);
    return 0;
}

void loadGuiFont(ImGuiIO& io) {
    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 2;
    config.PixelSnapH = true;

#if defined(_WIN32)
    constexpr const wchar_t* candidates[] = {
        L"C:\\Windows\\Fonts\\segoeui.ttf",
        L"C:\\Windows\\Fonts\\arial.ttf",
        L"C:\\Windows\\Fonts\\tahoma.ttf",
    };
    for (const auto* path : candidates) {
        if (fileExists(path)) {
            const auto utf8Path = utf8FromWide(path);
            io.Fonts->AddFontFromFileTTF(utf8Path.c_str(), defaultFontSize, &config, io.Fonts->GetGlyphRangesCyrillic());
            return;
        }
    }
#else
    constexpr const char* candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };
    for (const auto* path : candidates) {
        if (fileExists(path)) {
            io.Fonts->AddFontFromFileTTF(path, defaultFontSize, &config, io.Fonts->GetGlyphRangesCyrillic());
            return;
        }
    }
#endif

    io.Fonts->AddFontDefault();
}

void renderTopBar(GuiState& state) {
    ImGui::TextUnformatted("OCB Studio Native");
    ImGui::SameLine();
    ImGui::TextDisabled("v%s", OCB_BUILD_VERSION);
    ImGui::Separator();

    ImGui::InputText("OCB", state.ocbPath.data(), state.ocbPath.size());
    ImGui::SameLine();
    if (ImGui::Button("Open OCB")) {
        try {
#if defined(_WIN32)
            if (auto path = nativeFileDialog(false, L"Open OCB profile", L"MSI OC profile (*.ocb)\0*.ocb\0All files (*.*)\0*.*\0\0")) {
#else
            if (auto path = nativeFileDialog(false, "Open OCB profile", "MSI OC profile (*.ocb)\0*.ocb\0All files (*.*)\0*.*\0\0")) {
#endif
                setBuffer(state.ocbPath, *path);
            }
            state.controller.openOcb(pathFromUtf8(bufferText(state.ocbPath)));
            setStatus(state, "OCB profile loaded.");
        } catch (const std::exception& error) {
            setError(state, error);
        }
    }

    ImGui::InputText("BIOS", state.biosPath.data(), state.biosPath.size());
    ImGui::SameLine();
    if (ImGui::Button("Open BIOS")) {
        try {
#if defined(_WIN32)
            if (auto path = nativeFileDialog(false, L"Open BIOS image", L"BIOS images (*.bin;*.rom;*.cap;*.fd;*.a*)\0*.bin;*.rom;*.cap;*.fd;*.a*\0All files (*.*)\0*.*\0\0")) {
#else
            if (auto path = nativeFileDialog(false, "Open BIOS image", "BIOS images\0*.bin;*.rom;*.cap;*.fd;*.a*\0All files\0*.*\0\0")) {
#endif
                setBuffer(state.biosPath, *path);
            }
            state.controller.openBiosImage(pathFromUtf8(bufferText(state.biosPath)));
            setStatus(state, "BIOS image analyzed.");
        } catch (const std::exception& error) {
            setError(state, error);
        }
    }

    ImGui::InputText("IFR", state.ifrPath.data(), state.ifrPath.size());
    ImGui::SameLine();
    if (ImGui::Button("Open IFR")) {
        try {
#if defined(_WIN32)
            if (auto path = nativeFileDialog(false, L"Open IFR text", L"IFR text (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0")) {
#else
            if (auto path = nativeFileDialog(false, "Open IFR text", "IFR text\0*.txt\0All files\0*.*\0\0")) {
#endif
                setBuffer(state.ifrPath, *path);
            }
            state.controller.openIfrText(pathFromUtf8(bufferText(state.ifrPath)));
            setStatus(state, "IFR fields loaded.");
        } catch (const std::exception& error) {
            setError(state, error);
        }
    }

    ImGui::InputText("Output", state.outputPath.data(), state.outputPath.size());
    ImGui::SameLine();
    if (ImGui::Button("Browse##output")) {
#if defined(_WIN32)
        if (auto path = nativeFileDialog(true, L"Save OCB profile", L"MSI OC profile (*.ocb)\0*.ocb\0All files (*.*)\0*.*\0\0")) {
#else
        if (auto path = nativeFileDialog(true, "Save OCB profile", "MSI OC profile (*.ocb)\0*.ocb\0All files (*.*)\0*.*\0\0")) {
#endif
            setBuffer(state.outputPath, *path);
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Compensate checksums", &state.compensateChecksums);
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.controller.hasProfile());
    if (ImGui::Button("Save OCB")) {
        try {
            setBuffer(state.outputPath, pathWithDefaultExtension(bufferText(state.outputPath), ".ocb"));
            state.controller.saveOcb(pathFromUtf8(bufferText(state.outputPath)), state.compensateChecksums);
            setStatus(state, "OCB profile saved.");
        } catch (const std::exception& error) {
            setError(state, error);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset profile")) {
        state.controller.resetProfile();
        setStatus(state, "Profile reset to original bytes.");
    }
    ImGui::EndDisabled();
}

void renderPresets(GuiState& state) {
    ImGui::SeparatorText("Presets");

    const auto* preset = selectedPreset(state);
    const char* preview = preset == nullptr ? "Select preset..." : preset->name.c_str();
    if (ImGui::BeginCombo("Preset", preview)) {
        if (ImGui::Selectable("Select preset...", state.selectedPreset == 0)) {
            state.selectedPreset = 0;
        }

        int index = 1;
        for (const auto& builtin : ocb::core::builtinPresets()) {
            const auto label = builtin.name + "##builtin-preset-" + std::to_string(index);
            if (ImGui::Selectable(label.c_str(), state.selectedPreset == index)) {
                state.selectedPreset = index;
            }
            ++index;
        }
        for (const auto& imported : state.importedPresets) {
            const auto label = imported.name + " (file)##imported-preset-" + std::to_string(index);
            if (ImGui::Selectable(label.c_str(), state.selectedPreset == index)) {
                state.selectedPreset = index;
            }
            ++index;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!state.controller.hasProfile() || preset == nullptr);
    if (ImGui::Button("Apply")) {
        try {
            state.controller.applyPreset(*preset);
            setStatus(state, "Preset applied.");
        } catch (const std::exception& error) {
            setError(state, error);
        }
    }
    ImGui::EndDisabled();

    ImGui::InputText("Preset file", state.presetPath.data(), state.presetPath.size());
    ImGui::SameLine();
    if (ImGui::Button("Browse##preset")) {
#if defined(_WIN32)
        if (auto path = nativeFileDialog(false, L"Open preset", L"OCB Studio preset (*.ocbpreset;*.json)\0*.ocbpreset;*.json\0All files (*.*)\0*.*\0\0")) {
#else
        if (auto path = nativeFileDialog(false, "Open preset", "OCB Studio preset\0*.ocbpreset;*.json\0All files\0*.*\0\0")) {
#endif
            setBuffer(state.presetPath, *path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Import")) {
        try {
            state.importedPresets.push_back(ocb::core::loadPresetFromFile(pathFromUtf8(bufferText(state.presetPath))));
            state.selectedPreset = static_cast<int>(ocb::core::builtinPresets().size() + state.importedPresets.size());
            setStatus(state, "Preset imported.");
        } catch (const std::exception& error) {
            setError(state, error);
        }
    }
    ImGui::SameLine();
    const bool canExport = !bufferText(state.presetPath).empty()
        && (preset != nullptr || state.controller.hasProfile());
    ImGui::BeginDisabled(!canExport);
    if (ImGui::Button("Export")) {
        try {
            const auto defaultPath = bufferText(state.presetPath);
#if defined(_WIN32)
            const auto selectedPath = nativeFileDialog(
                true,
                L"Export preset",
                L"OCB Studio preset (*.ocbpreset)\0*.ocbpreset\0JSON files (*.json)\0*.json\0All files (*.*)\0*.*\0\0",
                defaultPath);
#else
            const auto selectedPath = nativeFileDialog(
                true,
                "Export preset",
                "OCB Studio preset\0*.ocbpreset\0JSON files\0*.json\0All files\0*.*\0\0",
                defaultPath);
#endif
            if (!selectedPath.has_value()) {
                setStatus(state, "Preset export canceled.");
                ImGui::EndDisabled();
                return;
            }
            setBuffer(state.presetPath, pathWithDefaultExtension(*selectedPath, ".ocbpreset"));

            const auto exportedPreset = preset == nullptr
                ? presetFromCurrentProfile(state)
                : *preset;
            ocb::core::savePresetToFile(pathFromUtf8(bufferText(state.presetPath)), exportedPreset);
            setStatus(state, "Preset exported.");
        } catch (const std::exception& error) {
            setError(state, error);
        }
    }
    ImGui::EndDisabled();
}

void renderFields(GuiState& state) {
    ImGui::SeparatorText("Fields");
    ImGui::InputText("Search", state.search.data(), state.search.size());

    const auto fields = bufferText(state.search).empty()
        ? state.controller.catalog().fields()
        : state.controller.catalog().search(bufferText(state.search));

    if (ImGui::BeginTable("fields", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 360))) {
        ImGui::TableSetupColumn("Prompt");
        ImGui::TableSetupColumn("Store");
        ImGui::TableSetupColumn("Offset");
        ImGui::TableSetupColumn("Bits");
        ImGui::TableSetupColumn("Current");
        ImGui::TableSetupColumn("Options");
        ImGui::TableSetupColumn("ID");
        ImGui::TableHeadersRow();

        for (const auto& field : fields) {
            const auto id = field.id();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool selected = state.selectedFieldId == id;
            const auto label = field.prompt + "##field-" + id;
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                state.selectedFieldId = id;
                if (state.controller.hasProfile()) {
                    try {
                        setBuffer(state.value, std::to_string(state.controller.profile().read(field)));
                    } catch (const std::exception&) {
                        setBuffer(state.value, {});
                    }
                }
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(field.varStore.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("0x%llX", static_cast<unsigned long long>(field.varOffset));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u", field.sizeBits);
            ImGui::TableSetColumnIndex(4);
            if (state.controller.hasProfile()) {
                try {
                    const auto value = state.controller.profile().read(field);
                    const auto text = formatValue(field, value);
                    ImGui::TextUnformatted(text.c_str());
                } catch (const std::exception&) {
                    ImGui::TextUnformatted("-");
                }
            } else {
                ImGui::TextUnformatted("-");
            }
            ImGui::TableSetColumnIndex(5);
            const auto optionsPreview = formatOptionsPreview(field);
            ImGui::TextUnformatted(optionsPreview.c_str());
            ImGui::TableSetColumnIndex(6);
            ImGui::TextUnformatted(id.c_str());
        }
        ImGui::EndTable();
    }

    const auto* selected = state.controller.catalog().findById(state.selectedFieldId);
    if (selected != nullptr) {
        ImGui::Text("Selected: %s", selected->prompt.c_str());
        if (!selected->options.empty()) {
            const auto parsedValue = tryParseUnsigned(bufferText(state.value));
            const auto currentValue = parsedValue.value_or(0);
            const auto* currentOption = parsedValue.has_value() ? optionForValue(*selected, currentValue) : nullptr;
            const auto preview = currentOption == nullptr
                ? bufferText(state.value)
                : formatValue(*selected, currentValue);
            if (ImGui::BeginCombo("Value", preview.c_str())) {
                for (const auto& option : selected->options) {
                    const auto optionLabel = std::to_string(option.value) + " = " + option.label
                        + "##value-option-" + std::to_string(option.value);
                    const bool isSelected = option.value == currentValue;
                    if (ImGui::Selectable(optionLabel.c_str(), isSelected)) {
                        setBuffer(state.value, std::to_string(option.value));
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::InputText("Value", state.value.data(), state.value.size());
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.controller.hasProfile());
        if (ImGui::Button("Write")) {
            try {
                state.controller.writeField(selected->id(), parseUnsigned(bufferText(state.value)));
                setStatus(state, "Field value written.");
            } catch (const std::exception& error) {
                setError(state, error);
            }
        }
        ImGui::EndDisabled();

        if (!selected->options.empty()) {
            ImGui::Spacing();
            ImGui::TextUnformatted("What this affects:");
            ImGui::TextWrapped("%s", fieldGuidance(*selected).c_str());
            ImGui::Spacing();
            ImGui::TextUnformatted("Available values:");
            for (const auto& option : selected->options) {
                ImGui::BulletText("%llu = %s",
                    static_cast<unsigned long long>(option.value),
                    option.label.empty() ? "(no description)" : option.label.c_str());
                ImGui::Indent();
                ImGui::TextWrapped("%s", optionGuidance(*selected, option).c_str());
                ImGui::Unindent();
            }
        } else {
            ImGui::Spacing();
            ImGui::TextUnformatted("What this affects:");
            ImGui::TextWrapped("%s", fieldGuidance(*selected).c_str());
            ImGui::TextUnformatted("Numeric field: enter a non-negative integer value.");
        }
    }
}

void renderMetadata(GuiState& state) {
    ImGui::SeparatorText("Status");
    if (!state.error.empty()) {
        ImGui::TextColored(ImVec4(1.0F, 0.25F, 0.25F, 1.0F), "Error: %s", state.error.c_str());
    } else {
        ImGui::TextUnformatted(state.status.c_str());
    }

    if (state.controller.hasProfile()) {
        const auto& metadata = state.controller.profile().metadata();
        ImGui::Text("OCB: %llu bytes | format: %s | $OCI$: %s 0x%llX",
            static_cast<unsigned long long>(metadata.fileSize),
            metadata.format.c_str(),
            metadata.hasOciSection ? "yes" : "no",
            static_cast<unsigned long long>(metadata.ociOffset));
        ImGui::Text("Board: %s | BIOS: %s | Profile: %s",
            metadata.boardName.empty() ? "unknown" : metadata.boardName.c_str(),
            metadata.biosVersion.empty() ? "unknown" : metadata.biosVersion.c_str(),
            metadata.profileName.empty() ? "unknown" : metadata.profileName.c_str());
    }

    if (state.controller.biosMetadata().has_value()) {
        const auto& metadata = *state.controller.biosMetadata();
        ImGui::Text("BIOS: %llu bytes | Setup PE32: %llu bytes | IFR: %llu questions, %llu fields",
            static_cast<unsigned long long>(metadata.imageSize),
            static_cast<unsigned long long>(metadata.setupPe32Size),
            static_cast<unsigned long long>(metadata.questionCount),
            static_cast<unsigned long long>(metadata.fieldCount));
    }
}

void renderGui(GuiState& state) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("OCB Studio", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    renderTopBar(state);
    renderPresets(state);
    renderFields(state);
    renderMetadata(state);
    ImGui::End();
}

int runGui() {
    if (glfwInit() == GLFW_FALSE) {
        std::cerr << "ERROR: Failed to initialize GLFW.\n";
        return 1;
    }

    constexpr const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 820, "OCB Studio Native", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        std::cerr << "ERROR: Failed to create GLFW window.\n";
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    loadGuiFont(io);
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    GuiState state;
    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderGui(state);

        ImGui::Render();
        int displayW = 0;
        int displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.08F, 0.09F, 0.10F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

#if defined(_WIN32)
[[nodiscard]] std::vector<std::string> argsFromWide(int argc, wchar_t* argv[]) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.push_back(utf8FromWide(argv[i]));
    }
    return args;
}
#endif

} // namespace

#if defined(_WIN32)
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return runGui();
    }

    try {
        if (argc <= 1) {
            LocalFree(argv);
            return runGui();
        }

        attachConsoleForCli();
        auto args = argsFromWide(argc, argv);
        LocalFree(argv);
        argv = nullptr;
        const int result = runCli(std::move(args));
        std::cout.flush();
        std::cerr.flush();
        return result;
    } catch (const std::exception& error) {
        if (argv != nullptr) {
            LocalFree(argv);
        }
        attachConsoleForCli();
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
#else
int main(int argc, char* argv[]) {
    try {
        if (argc <= 1) {
            return runGui();
        }

        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        return runCli(std::move(args));
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
#endif
