#include "ocb/core/PresetFile.hpp"

#include "ocb/core/OcbException.hpp"

#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ocb::core {
namespace {

class JsonReader final {
public:
    explicit JsonReader(std::string_view input)
        : input_(input) {}

    [[nodiscard]] Preset readPreset() {
        Preset preset;

        expect('{');
        skipWhitespace();
        while (!consume('}')) {
            const auto key = readString();
            expect(':');

            if (key == "name") {
                preset.name = readString();
            } else if (key == "values" || key == "valuesByPrompt") {
                preset.valuesByPrompt = readValues();
            } else {
                skipValue();
            }

            skipWhitespace();
            if (consume('}')) {
                break;
            }
            expect(',');
        }
        skipWhitespace();
        if (!atEnd()) {
            fail("лишние данные после JSON-объекта");
        }
        if (preset.name.empty()) {
            fail("в файле пресета отсутствует имя");
        }
        if (preset.valuesByPrompt.empty()) {
            fail("файл пресета не содержит значений");
        }
        return preset;
    }

private:
    std::string_view input_;
    std::size_t position_{};

    [[nodiscard]] bool atEnd() const noexcept {
        return position_ >= input_.size();
    }

    void skipWhitespace() noexcept {
        while (!atEnd() && std::isspace(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    [[nodiscard]] bool consume(char expected) {
        skipWhitespace();
        if (!atEnd() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(char expected) {
        if (!consume(expected)) {
            fail(std::string("ожидался символ '") + expected + "'");
        }
    }

    [[noreturn]] void fail(const std::string& message) const {
        throw OcbException("Некорректный файл пресета: " + message);
    }

    [[nodiscard]] std::string readString() {
        skipWhitespace();
        if (atEnd() || input_[position_] != '"') {
            fail("ожидалась строка");
        }
        ++position_;

        std::string output;
        while (!atEnd()) {
            const char ch = input_[position_++];
            if (ch == '"') {
                return output;
            }
            if (ch != '\\') {
                output.push_back(ch);
                continue;
            }
            if (atEnd()) {
                fail("незавершенная escape-последовательность");
            }

            const char escaped = input_[position_++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                output.push_back(escaped);
                break;
            case 'b':
                output.push_back('\b');
                break;
            case 'f':
                output.push_back('\f');
                break;
            case 'n':
                output.push_back('\n');
                break;
            case 'r':
                output.push_back('\r');
                break;
            case 't':
                output.push_back('\t');
                break;
            case 'u':
                readUnicodeEscape(output);
                break;
            default:
                fail("неизвестная escape-последовательность");
            }
        }
        fail("незавершенная строка");
    }

    void readUnicodeEscape(std::string& output) {
        if (position_ + 4 > input_.size()) {
            fail("незавершенная unicode escape-последовательность");
        }

        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const char ch = input_[position_++];
            value <<= 4U;
            if (ch >= '0' && ch <= '9') {
                value |= static_cast<unsigned>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                value |= static_cast<unsigned>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                value |= static_cast<unsigned>(ch - 'A' + 10);
            } else {
                fail("некорректная unicode escape-последовательность");
            }
        }

        if (value <= 0x7FU) {
            output.push_back(static_cast<char>(value));
        } else if (value <= 0x7FFU) {
            output.push_back(static_cast<char>(0xC0U | (value >> 6U)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        } else {
            output.push_back(static_cast<char>(0xE0U | (value >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
    }

    [[nodiscard]] std::map<std::string, std::uint64_t> readValues() {
        std::map<std::string, std::uint64_t> values;

        expect('{');
        skipWhitespace();
        while (!consume('}')) {
            auto prompt = readString();
            if (prompt.empty()) {
                fail("пустое имя поля");
            }
            expect(':');
            values[std::move(prompt)] = readUnsignedValue();

            skipWhitespace();
            if (consume('}')) {
                break;
            }
            expect(',');
        }
        return values;
    }

    [[nodiscard]] std::uint64_t readUnsignedValue() {
        skipWhitespace();
        if (atEnd()) {
            fail("ожидалось значение поля");
        }
        if (input_[position_] == '"') {
            return parseUnsigned(readString());
        }

        const auto start = position_;
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
        if (start == position_) {
            fail("значение поля должно быть целым неотрицательным числом или строкой с числом");
        }
        return parseUnsigned(std::string(input_.substr(start, position_ - start)));
    }

    [[nodiscard]] std::uint64_t parseUnsigned(const std::string& text) const {
        std::uint64_t value = 0;
        const int base = text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0 ? 16 : 10;
        const auto digits = base == 16 ? std::string_view(text).substr(2) : std::string_view(text);
        const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
        if (digits.empty() || result.ec != std::errc{} || result.ptr != digits.data() + digits.size()) {
            fail("некорректное числовое значение: " + text);
        }
        return value;
    }

    void skipValue() {
        skipWhitespace();
        if (atEnd()) {
            fail("ожидалось JSON-значение");
        }

        if (input_[position_] == '"') {
            (void)readString();
            return;
        }
        if (consume('{')) {
            skipWhitespace();
            while (!consume('}')) {
                (void)readString();
                expect(':');
                skipValue();
                skipWhitespace();
                if (consume('}')) {
                    break;
                }
                expect(',');
            }
            return;
        }
        if (consume('[')) {
            skipWhitespace();
            while (!consume(']')) {
                skipValue();
                skipWhitespace();
                if (consume(']')) {
                    break;
                }
                expect(',');
            }
            return;
        }

        const auto start = position_;
        while (!atEnd() && !std::isspace(static_cast<unsigned char>(input_[position_]))
               && input_[position_] != ',' && input_[position_] != '}' && input_[position_] != ']') {
            ++position_;
        }
        if (start == position_) {
            fail("некорректное JSON-значение");
        }
    }
};

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw OcbException("Не удалось открыть файл пресета: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        throw OcbException("Не удалось прочитать файл пресета: " + path.string());
    }
    auto text = buffer.str();
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == 0xEFU
        && static_cast<unsigned char>(text[1]) == 0xBBU
        && static_cast<unsigned char>(text[2]) == 0xBFU) {
        text.erase(0, 3);
    }
    return text;
}

[[nodiscard]] std::string escapeJson(std::string_view value) {
    std::string output;
    output.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20U) {
                constexpr char hex[] = "0123456789ABCDEF";
                output += "\\u00";
                output.push_back(hex[(static_cast<unsigned char>(ch) >> 4U) & 0xFU]);
                output.push_back(hex[static_cast<unsigned char>(ch) & 0xFU]);
            } else {
                output.push_back(ch);
            }
        }
    }
    return output;
}

} // namespace

Preset loadPresetFromFile(const std::filesystem::path& path) {
    return JsonReader(readTextFile(path)).readPreset();
}

void savePresetToFile(const std::filesystem::path& path, const Preset& preset) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw OcbException("Не удалось создать файл пресета: " + path.string());
    }

    output << "{\n"
           << "  \"format\": \"OCB Studio Preset\",\n"
           << "  \"version\": 1,\n"
           << "  \"name\": \"" << escapeJson(preset.name) << "\",\n"
           << "  \"values\": {\n";

    for (auto it = preset.valuesByPrompt.begin(); it != preset.valuesByPrompt.end(); ++it) {
        output << "    \"" << escapeJson(it->first) << "\": " << it->second;
        if (std::next(it) != preset.valuesByPrompt.end()) {
            output << ',';
        }
        output << '\n';
    }

    output << "  }\n"
           << "}\n";

    if (!output) {
        throw OcbException("Не удалось записать файл пресета: " + path.string());
    }
}

} // namespace ocb::core
