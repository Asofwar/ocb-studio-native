#include "ocb/tools/ifr/IfrTextParser.hpp"

#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace ocb::tools::ifr {
namespace {

std::uint32_t parseU32(const std::string& text) {
    const auto base = text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0 ? 16 : 10;
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, base));
}

std::uint64_t parseU64(const std::string& text) {
    const auto base = text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0 ? 16 : 10;
    return static_cast<std::uint64_t>(std::stoull(text, nullptr, base));
}

std::string varStoreNameForId(std::uint32_t id) {
    switch (id) {
    case 0xF101:
        return "Setup";
    case 0x0002:
        return "CpuSetup";
    case 0x0005:
        return "SaSetup";
    case 0x0007:
        return "PchSetup";
    default:
        return {};
    }
}

std::string trimLeft(std::string value) {
    const auto pos = value.find_first_not_of(" \t");
    if (pos == std::string::npos) {
        return {};
    }
    value.erase(0, pos);
    return value;
}

} // namespace

std::vector<IfrQuestion> IfrTextParser::parse(std::istream& input) const {
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parse(buffer.str());
}

std::vector<IfrQuestion> IfrTextParser::parse(std::string_view text) const {
    static const std::regex questionRegex(
        R"ifr(^(OneOf|Numeric) Prompt: "([^"]*)".*?QuestionId: (0x[0-9A-Fa-f]+), VarStoreId: (0x[0-9A-Fa-f]+), VarOffset: (0x[0-9A-Fa-f]+).*?Size: ([0-9]+))ifr");
    static const std::regex optionRegex(
        R"ifr(^OneOfOption Option: "([^"]*)".*?Value: (0x[0-9A-Fa-f]+|[0-9]+))ifr");

    std::vector<std::string> lines;
    {
        std::istringstream stream(std::string{text});
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(trimLeft(std::move(line)));
        }
    }

    std::vector<IfrQuestion> questions;
    std::set<std::string> seen;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::smatch match;
        if (!std::regex_search(lines[i], match, questionRegex)) {
            continue;
        }

        const auto varStoreId = parseU32(match[4].str());
        auto varStoreName = varStoreNameForId(varStoreId);
        if (varStoreName.empty()) {
            continue;
        }

        IfrQuestion question;
        question.kind = match[1].str() == "OneOf" ? IfrQuestionKind::OneOf : IfrQuestionKind::Numeric;
        question.prompt = match[2].str();
        question.questionId = parseU32(match[3].str());
        question.varStoreId = varStoreId;
        question.varStoreName = std::move(varStoreName);
        question.varOffset = parseU32(match[5].str());
        question.sizeBits = parseU32(match[6].str());
        question.sourceLine = static_cast<std::uint32_t>(i + 1);

        if (question.sizeBits != 8 && question.sizeBits != 16 && question.sizeBits != 32) {
            continue;
        }

        if (question.kind == IfrQuestionKind::OneOf) {
            for (std::size_t j = i + 1; j < lines.size(); ++j) {
                if (lines[j] == "End") {
                    break;
                }
                std::smatch optionMatch;
                if (std::regex_search(lines[j], optionMatch, optionRegex)) {
                    question.options.push_back({parseU64(optionMatch[2].str()), optionMatch[1].str()});
                }
            }
        }

        const auto key = std::to_string(static_cast<int>(question.kind)) + ":" + question.prompt + ":"
            + question.varStoreName + ":" + std::to_string(question.varOffset) + ":"
            + std::to_string(question.sizeBits);
        if (seen.insert(key).second) {
            questions.push_back(std::move(question));
        }
    }

    return questions;
}

} // namespace ocb::tools::ifr
