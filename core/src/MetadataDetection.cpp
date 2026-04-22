#include "MetadataDetection.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace ocb::core::detail {
namespace {

bool isTextByte(std::uint8_t value) {
    return value >= 0x20 && value <= 0x7E;
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::string collapseSpaces(const std::string& value) {
    std::string output;
    output.reserve(value.size());
    bool previousSpace = false;
    for (const unsigned char ch : value) {
        if (std::isspace(ch) != 0) {
            if (!previousSpace) {
                output.push_back(' ');
            }
            previousSpace = true;
        } else {
            output.push_back(static_cast<char>(ch));
            previousSpace = false;
        }
    }
    return trim(output);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void pushString(std::vector<std::string>& strings, std::string value) {
    value = collapseSpaces(value);
    if (value.size() >= 4 && value.size() <= 96) {
        strings.push_back(std::move(value));
    }
}

std::string firstRegexMatch(const std::vector<std::string>& strings, const std::regex& pattern) {
    for (const auto& value : strings) {
        std::smatch match;
        if (std::regex_search(value, match, pattern)) {
            return match.str(0);
        }
    }
    return {};
}

int boardNameScore(const std::string& value) {
    static const std::regex chipsetPattern(R"(\b[ZBHX][0-9]{3}[A-Z0-9-]*\b)", std::regex::icase);

    const bool hasChipset = std::regex_search(value, chipsetPattern);
    bool hasModelToken = containsCaseInsensitive(value, "MSI");
    for (const auto* token : {"MEG", "MPG", "MAG", "PRO ", "GODLIKE", "ACE", "CARBON", "EDGE", "TOMAHAWK", "UNIFY"}) {
        if (containsCaseInsensitive(value, token)) {
            hasModelToken = true;
            break;
        }
    }
    if (!hasChipset || !hasModelToken) {
        return 0;
    }

    int score = 0;
    score += 8;
    for (const auto* token : {"MEG", "MPG", "MAG", "PRO ", "GODLIKE", "ACE", "CARBON", "EDGE", "TOMAHAWK", "UNIFY"}) {
        if (containsCaseInsensitive(value, token)) {
            score += 4;
        }
    }
    if (containsCaseInsensitive(value, "MSI")) {
        score += 2;
    }
    if (containsCaseInsensitive(value, "WIFI") || containsCaseInsensitive(value, "DDR")) {
        score += 2;
    }
    if (containsCaseInsensitive(value, " II") || containsCaseInsensitive(value, " 2")) {
        score += 1;
    }
    if (containsCaseInsensitive(value, "Copyright") || containsCaseInsensitive(value, "American Megatrends")) {
        score -= 8;
    }
    return score;
}

} // namespace

std::vector<std::string> extractTextStrings(std::span<const std::uint8_t> bytes) {
    std::vector<std::string> strings;

    std::string current;
    for (const auto byte : bytes) {
        if (isTextByte(byte)) {
            current.push_back(static_cast<char>(byte));
        } else {
            pushString(strings, current);
            current.clear();
        }
    }
    pushString(strings, current);

    current.clear();
    for (std::size_t i = 0; i + 1 < bytes.size(); i += 2) {
        if (bytes[i + 1] == 0 && isTextByte(bytes[i])) {
            current.push_back(static_cast<char>(bytes[i]));
        } else {
            pushString(strings, current);
            current.clear();
        }
    }
    pushString(strings, current);

    return strings;
}

bool containsCaseInsensitive(const std::string& text, const std::string& needle) {
    return toLower(text).find(toLower(needle)) != std::string::npos;
}

std::string chooseBoardName(const std::vector<std::string>& strings) {
    std::string best;
    int bestScore = 0;
    for (const auto& value : strings) {
        const auto score = boardNameScore(value);
        if (score > bestScore || (score == bestScore && score > 0 && value.size() < best.size())) {
            best = value;
            bestScore = score;
        }
    }
    return best.empty() ? boardNameFromBiosVersion(firstBiosVersion(strings)) : best;
}

std::string chooseProfileName(const std::vector<std::string>& strings) {
    for (const auto& value : strings) {
        if ((containsCaseInsensitive(value, "profile") || containsCaseInsensitive(value, "overclock"))
            && !containsCaseInsensitive(value, "copyright")) {
            return value;
        }
    }
    return {};
}

std::string firstBiosVersion(const std::vector<std::string>& strings) {
    return firstRegexMatch(strings, std::regex(R"(\bE[0-9A-Z]{4,6}IMS\.[0-9A-Z]{2,4}\b)", std::regex::icase));
}

std::string boardNameFromBiosVersion(const std::string& biosVersion) {
    if (biosVersion.size() < 6) {
        return {};
    }

    const auto id = toLower(biosVersion.substr(0, 6));
    if (id == "e7d89i") {
        return "MSI MPG Z790 CARBON WIFI II";
    }
    return {};
}

} // namespace ocb::core::detail
