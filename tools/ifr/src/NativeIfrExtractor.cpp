#include "ocb/tools/ifr/NativeIfrExtractor.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ocb::tools::ifr {
namespace {

constexpr std::uint8_t kHiiPackageForms = 0x02;
constexpr std::uint8_t kHiiPackageStrings = 0x04;
constexpr std::uint8_t kHiiPackageEnd = 0xDF;

constexpr std::uint8_t kSibtEnd = 0x00;
constexpr std::uint8_t kSibtStringScsu = 0x10;
constexpr std::uint8_t kSibtStringScsuFont = 0x11;
constexpr std::uint8_t kSibtStringsScsu = 0x12;
constexpr std::uint8_t kSibtStringsScsuFont = 0x13;
constexpr std::uint8_t kSibtStringUcs2 = 0x14;
constexpr std::uint8_t kSibtStringUcs2Font = 0x15;
constexpr std::uint8_t kSibtStringsUcs2 = 0x16;
constexpr std::uint8_t kSibtStringsUcs2Font = 0x17;
constexpr std::uint8_t kSibtDuplicate = 0x20;
constexpr std::uint8_t kSibtSkip2 = 0x21;
constexpr std::uint8_t kSibtSkip1 = 0x22;
constexpr std::uint8_t kSibtExt1 = 0x30;
constexpr std::uint8_t kSibtExt2 = 0x31;
constexpr std::uint8_t kSibtExt4 = 0x32;

constexpr std::uint8_t kIfrOneOfOp = 0x05;
constexpr std::uint8_t kIfrCheckboxOp = 0x06;
constexpr std::uint8_t kIfrNumericOp = 0x07;
constexpr std::uint8_t kIfrOneOfOptionOp = 0x09;
constexpr std::uint8_t kIfrFormSetOp = 0x0E;
constexpr std::uint8_t kIfrVarStoreOp = 0x24;
constexpr std::uint8_t kIfrEndOp = 0x29;

constexpr std::uint8_t kIfrLengthMask = 0x7F;
constexpr std::uint8_t kIfrScopeMask = 0x80;

struct ByteView {
    std::span<const std::uint8_t> bytes;

    [[nodiscard]] bool has(std::size_t offset, std::size_t length) const {
        return offset <= bytes.size() && length <= bytes.size() - offset;
    }

    [[nodiscard]] std::uint8_t u8(std::size_t offset) const {
        return has(offset, 1) ? bytes[offset] : 0;
    }

    [[nodiscard]] std::uint16_t u16(std::size_t offset) const {
        return static_cast<std::uint16_t>(u8(offset))
            | (static_cast<std::uint16_t>(u8(offset + 1)) << 8);
    }

    [[nodiscard]] std::uint32_t u24(std::size_t offset) const {
        return static_cast<std::uint32_t>(u8(offset))
            | (static_cast<std::uint32_t>(u8(offset + 1)) << 8)
            | (static_cast<std::uint32_t>(u8(offset + 2)) << 16);
    }

    [[nodiscard]] std::uint32_t u32(std::size_t offset) const {
        return static_cast<std::uint32_t>(u16(offset))
            | (static_cast<std::uint32_t>(u16(offset + 2)) << 16);
    }

    [[nodiscard]] std::uint64_t u64(std::size_t offset) const {
        return static_cast<std::uint64_t>(u32(offset))
            | (static_cast<std::uint64_t>(u32(offset + 4)) << 32);
    }
};

struct StringPackage {
    std::string language;
    std::vector<std::string> stringsById{std::string{}};
};

struct FormPackage {
    std::size_t begin{};
    std::size_t end{};
};

struct PackageList {
    std::vector<StringPackage> stringPackages;
    std::vector<FormPackage> formPackages;
};

struct ScopeFrame {
    std::optional<std::size_t> optionQuestionIndex;
};

[[nodiscard]] bool isKnownPackageType(std::uint8_t type) {
    return type == kHiiPackageForms
        || type == kHiiPackageStrings
        || type == kHiiPackageEnd
        || type <= 0x0F;
}

void appendUtf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint == 0) {
        return;
    }
    if (codepoint <= 0x7F) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

[[nodiscard]] std::string readAsciiNull(ByteView view, std::size_t& offset, std::size_t end) {
    std::string value;
    while (offset < end) {
        const auto byte = view.u8(offset++);
        if (byte == 0) {
            break;
        }
        value.push_back(static_cast<char>(byte));
    }
    return value;
}

[[nodiscard]] std::string readUcs2Null(ByteView view, std::size_t& offset, std::size_t end) {
    std::string value;
    while (offset + 1 < end) {
        const auto codeUnit = view.u16(offset);
        offset += 2;
        if (codeUnit == 0) {
            break;
        }
        appendUtf8(value, codeUnit);
    }
    return value;
}

void ensureStringId(std::vector<std::string>& strings, std::size_t id) {
    if (strings.size() <= id) {
        strings.resize(id + 1);
    }
}

void assignString(std::vector<std::string>& strings, std::uint32_t& currentId, std::string value) {
    ensureStringId(strings, currentId);
    strings[currentId] = std::move(value);
    ++currentId;
}

[[nodiscard]] std::string parseLanguage(ByteView view, std::size_t packageBegin, std::size_t packageEnd) {
    const auto headerSize = view.u32(packageBegin + 4);
    if (headerSize == 0 || packageBegin + headerSize > packageEnd) {
        return {};
    }

    const auto languageBegin = packageBegin + 4 + 4 + 4 + (16 * 2) + 2;
    if (languageBegin >= packageBegin + headerSize) {
        return {};
    }

    std::string language;
    for (auto offset = languageBegin; offset < packageBegin + headerSize; ++offset) {
        const auto byte = view.u8(offset);
        if (byte == 0) {
            break;
        }
        language.push_back(static_cast<char>(byte));
    }
    return language;
}

[[nodiscard]] StringPackage parseStringPackage(ByteView view, std::size_t packageBegin, std::size_t packageEnd) {
    StringPackage package;
    package.language = parseLanguage(view, packageBegin, packageEnd);

    const auto stringInfoOffset = view.u32(packageBegin + 8);
    if (stringInfoOffset == 0 || packageBegin + stringInfoOffset >= packageEnd) {
        return package;
    }

    auto currentId = std::uint32_t{1};
    auto offset = packageBegin + stringInfoOffset;
    while (offset < packageEnd) {
        const auto blockType = view.u8(offset++);
        switch (blockType) {
        case kSibtEnd:
            return package;
        case kSibtSkip1:
            currentId += view.u8(offset);
            offset += 1;
            break;
        case kSibtSkip2:
            currentId += view.u16(offset);
            offset += 2;
            break;
        case kSibtDuplicate: {
            const auto sourceId = view.u16(offset);
            offset += 2;
            const auto value = sourceId < package.stringsById.size() ? package.stringsById[sourceId] : std::string{};
            assignString(package.stringsById, currentId, value);
            break;
        }
        case kSibtStringScsu:
            assignString(package.stringsById, currentId, readAsciiNull(view, offset, packageEnd));
            break;
        case kSibtStringScsuFont:
            offset += 1;
            assignString(package.stringsById, currentId, readAsciiNull(view, offset, packageEnd));
            break;
        case kSibtStringsScsu: {
            const auto count = view.u16(offset);
            offset += 2;
            for (auto i = std::uint16_t{}; i < count && offset < packageEnd; ++i) {
                assignString(package.stringsById, currentId, readAsciiNull(view, offset, packageEnd));
            }
            break;
        }
        case kSibtStringsScsuFont: {
            offset += 1;
            const auto count = view.u16(offset);
            offset += 2;
            for (auto i = std::uint16_t{}; i < count && offset < packageEnd; ++i) {
                assignString(package.stringsById, currentId, readAsciiNull(view, offset, packageEnd));
            }
            break;
        }
        case kSibtStringUcs2:
            assignString(package.stringsById, currentId, readUcs2Null(view, offset, packageEnd));
            break;
        case kSibtStringUcs2Font:
            offset += 1;
            assignString(package.stringsById, currentId, readUcs2Null(view, offset, packageEnd));
            break;
        case kSibtStringsUcs2: {
            const auto count = view.u16(offset);
            offset += 2;
            for (auto i = std::uint16_t{}; i < count && offset < packageEnd; ++i) {
                assignString(package.stringsById, currentId, readUcs2Null(view, offset, packageEnd));
            }
            break;
        }
        case kSibtStringsUcs2Font: {
            offset += 1;
            const auto count = view.u16(offset);
            offset += 2;
            for (auto i = std::uint16_t{}; i < count && offset < packageEnd; ++i) {
                assignString(package.stringsById, currentId, readUcs2Null(view, offset, packageEnd));
            }
            break;
        }
        case kSibtExt1:
            offset += view.u8(offset);
            break;
        case kSibtExt2:
            offset += view.u16(offset);
            break;
        case kSibtExt4:
            offset += view.u32(offset);
            break;
        default:
            return package;
        }
    }

    return package;
}

[[nodiscard]] bool parsePackageListAt(ByteView view, std::size_t listBegin, PackageList& packageList) {
    if (!view.has(listBegin, 20)) {
        return false;
    }

    const auto listLength = view.u32(listBegin + 16);
    if (listLength < 24 || listLength > view.bytes.size() - listBegin) {
        return false;
    }

    const auto listEnd = listBegin + listLength;
    auto offset = listBegin + 20;
    bool sawEnd = false;

    PackageList candidate;
    while (offset + 4 <= listEnd) {
        const auto packageLength = view.u24(offset);
        const auto packageType = view.u8(offset + 3);
        if (packageLength < 4 || packageLength > listEnd - offset || !isKnownPackageType(packageType)) {
            return false;
        }

        const auto packageEnd = offset + packageLength;
        if (packageType == kHiiPackageStrings) {
            candidate.stringPackages.push_back(parseStringPackage(view, offset, packageEnd));
        } else if (packageType == kHiiPackageForms) {
            candidate.formPackages.push_back({offset + 4, packageEnd});
        } else if (packageType == kHiiPackageEnd) {
            sawEnd = true;
            offset = packageEnd;
            break;
        }

        offset = packageEnd;
    }

    if (!sawEnd || offset != listEnd || candidate.formPackages.empty()) {
        return false;
    }

    packageList = std::move(candidate);
    return true;
}

[[nodiscard]] std::vector<PackageList> findPackageLists(ByteView view) {
    std::vector<PackageList> lists;
    for (std::size_t offset = 0; offset + 24 <= view.bytes.size(); ++offset) {
        PackageList packageList;
        if (parsePackageListAt(view, offset, packageList)) {
            lists.push_back(std::move(packageList));
        }
    }
    return lists;
}

[[nodiscard]] const StringPackage* selectStringPackage(const PackageList& packageList) {
    if (packageList.stringPackages.empty()) {
        return nullptr;
    }

    const auto english = std::find_if(packageList.stringPackages.begin(), packageList.stringPackages.end(), [](const auto& item) {
        return item.language.rfind("en", 0) == 0 || item.language.rfind("eng", 0) == 0;
    });
    if (english != packageList.stringPackages.end()) {
        return &*english;
    }
    return &packageList.stringPackages.front();
}

[[nodiscard]] std::string stringById(const StringPackage* stringPackage, std::uint16_t stringId) {
    if (stringPackage == nullptr || stringId >= stringPackage->stringsById.size()) {
        return {};
    }
    return stringPackage->stringsById[stringId];
}

[[nodiscard]] std::string readNullTerminatedAscii(ByteView view, std::size_t offset, std::size_t end) {
    std::string value;
    for (auto index = offset; index < end && view.u8(index) != 0; ++index) {
        value.push_back(static_cast<char>(view.u8(index)));
    }
    return value;
}

[[nodiscard]] std::uint32_t numericSizeBits(std::uint8_t flags) {
    switch (flags & 0x03U) {
    case 0:
        return 8;
    case 1:
        return 16;
    case 2:
        return 32;
    case 3:
        return 64;
    default:
        return 0;
    }
}

[[nodiscard]] IfrOption parseOption(ByteView view, std::size_t opOffset, const StringPackage* stringPackage) {
    const auto type = view.u8(opOffset + 5);
    std::uint64_t value{};
    switch (type) {
    case 0x00:
        value = view.u8(opOffset + 6);
        break;
    case 0x01:
        value = view.u16(opOffset + 6);
        break;
    case 0x02:
        value = view.u32(opOffset + 6);
        break;
    case 0x03:
        value = view.u64(opOffset + 6);
        break;
    default:
        break;
    }

    return {value, stringById(stringPackage, view.u16(opOffset + 2))};
}

[[nodiscard]] IfrQuestion parseQuestion(
    IfrQuestionKind kind,
    ByteView view,
    std::size_t opOffset,
    const std::map<std::uint32_t, std::string>& varStores,
    const StringPackage* stringPackage) {
    const auto varStoreId = view.u16(opOffset + 8);
    const auto store = varStores.find(varStoreId);

    IfrQuestion question;
    question.kind = kind;
    question.prompt = stringById(stringPackage, view.u16(opOffset + 2));
    question.questionId = view.u16(opOffset + 6);
    question.varStoreId = varStoreId;
    question.varStoreName = store == varStores.end() ? std::string{} : store->second;
    question.varOffset = view.u16(opOffset + 10);
    question.sizeBits = view.u8(opOffset) == kIfrCheckboxOp ? 8 : numericSizeBits(view.u8(opOffset + 13));
    question.sourceLine = static_cast<std::uint32_t>(opOffset);
    return question;
}

[[nodiscard]] std::optional<std::size_t> activeOptionQuestion(const std::vector<ScopeFrame>& scopeStack) {
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        if (it->optionQuestionIndex.has_value()) {
            return it->optionQuestionIndex;
        }
    }
    return std::nullopt;
}

void parseFormPackage(
    ByteView view,
    const FormPackage& formPackage,
    const StringPackage* stringPackage,
    std::vector<IfrQuestion>& questions) {
    std::map<std::uint32_t, std::string> varStores;
    std::vector<ScopeFrame> scopeStack;

    for (auto offset = formPackage.begin; offset + 1 < formPackage.end;) {
        const auto opcode = view.u8(offset);
        const auto header = view.u8(offset + 1);
        const auto length = static_cast<std::size_t>(header & kIfrLengthMask);
        const auto hasScope = (header & kIfrScopeMask) != 0;
        if (length < 2 || length > formPackage.end - offset) {
            break;
        }

        std::optional<std::size_t> scopedQuestion;
        if (opcode == kIfrVarStoreOp && length >= 22) {
            const auto varStoreId = view.u16(offset + 18);
            varStores[varStoreId] = readNullTerminatedAscii(view, offset + 22, offset + length);
        } else if (opcode == kIfrOneOfOp && length >= 14) {
            questions.push_back(parseQuestion(IfrQuestionKind::OneOf, view, offset, varStores, stringPackage));
            scopedQuestion = questions.size() - 1;
        } else if (opcode == kIfrNumericOp && length >= 14) {
            questions.push_back(parseQuestion(IfrQuestionKind::Numeric, view, offset, varStores, stringPackage));
        } else if (opcode == kIfrCheckboxOp && length >= 14) {
            auto question = parseQuestion(IfrQuestionKind::OneOf, view, offset, varStores, stringPackage);
            question.options = {{0, "Отключено"}, {1, "Включено"}};
            questions.push_back(std::move(question));
        } else if (opcode == kIfrOneOfOptionOp && length >= 7) {
            if (const auto target = activeOptionQuestion(scopeStack); target.has_value()) {
                questions[*target].options.push_back(parseOption(view, offset, stringPackage));
            }
        } else if (opcode == kIfrFormSetOp) {
            varStores.clear();
        } else if (opcode == kIfrEndOp) {
            if (!scopeStack.empty()) {
                scopeStack.pop_back();
            }
        }

        if (hasScope) {
            scopeStack.push_back({scopedQuestion});
        }
        offset += length;
    }
}

} // namespace

std::vector<IfrQuestion> NativeIfrExtractor::extractQuestions(
    std::span<const std::uint8_t> setupPe32Body) const {
    const ByteView view{setupPe32Body};
    const auto packageLists = findPackageLists(view);

    std::vector<IfrQuestion> questions;
    for (const auto& packageList : packageLists) {
        const auto* stringPackage = selectStringPackage(packageList);
        for (const auto& formPackage : packageList.formPackages) {
            parseFormPackage(view, formPackage, stringPackage, questions);
        }
    }

    questions.erase(std::remove_if(questions.begin(), questions.end(), [](const auto& question) {
        return question.prompt.empty() || question.varStoreName.empty();
    }), questions.end());

    if (questions.empty()) {
        throw std::runtime_error("В модуле Setup PE32 не найдены IFR-вопросы.");
    }

    return questions;
}

} // namespace ocb::tools::ifr
