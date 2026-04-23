#include "ocb/core/BiosAnalysisService.hpp"
#include "ocb/core/IfrFieldMapper.hpp"
#include "core/src/MetadataDetection.hpp"
#include "ocb/tools/ifr/NativeIfrExtractor.hpp"
#include "ocb/tools/uefi/UefiExtractor.hpp"

#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

std::vector<std::uint8_t> readAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open BIOS image: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void logStep(const char* label, const Clock::time_point start) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
    std::cerr << label << " at " << elapsed << " ms" << std::endl;
}

bool containsCaseInsensitive(std::string text, std::string needle) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    for (char& ch : needle) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text.find(needle) != std::string::npos;
}

std::uint32_t readHiiLength(const std::vector<std::uint8_t>& body, std::size_t offset) {
    return static_cast<std::uint32_t>(body[offset])
        | (static_cast<std::uint32_t>(body[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(body[offset + 2]) << 16);
}

bool containsIfrFormSetPackage(const std::vector<std::uint8_t>& body) {
    if (body.size() < 8) {
        return false;
    }

    for (std::size_t index = 0; index + 8 < body.size(); ++index) {
        if ((body[index] == 0 && body[index + 1] == 0 && body[index + 2] == 0)
            || body[index + 3] != 0x02
            || body[index + 4] != 0x0E) {
            continue;
        }

        const auto length = readHiiLength(body, index);
        if (length < 8 || index + length > body.size()) {
            continue;
        }
        if (body[index + length - 2] == 0x29 && body[index + length - 1] == 0x02) {
            return true;
        }
    }

    return false;
}

void dumpCandidates(const ocb::tools::uefi::FirmwareNode& node, const std::string& path) {
    const auto currentPath = path.empty() ? node.name : path + "/" + node.name;
    const bool isExecutable = containsCaseInsensitive(node.type, "pe32") || containsCaseInsensitive(node.type, "te");
    const bool hasIfr = containsIfrFormSetPackage(node.body);
    if (isExecutable || hasIfr) {
        std::cerr
            << "candidate path=" << currentPath
            << " type=" << node.type
            << " size=" << node.body.size()
            << " executable=" << (isExecutable ? "yes" : "no")
            << " ifr=" << (hasIfr ? "yes" : "no")
            << std::endl;
    }
    for (const auto& child : node.children) {
        dumpCandidates(child, currentPath);
    }
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const std::filesystem::path biosPath = argc > 1 ? argv[1] : std::filesystem::path{"D:/14900K/E7D89IMS.A91"};
        const auto started = Clock::now();

        std::cerr << "bios_path=" << biosPath.string() << std::endl;
        auto biosImage = readAll(biosPath);
        logStep("read bios", started);

        const ocb::tools::uefi::NativeUefiExtractor uefiExtractor;
        const ocb::tools::ifr::NativeIfrExtractor ifrExtractor;

        auto root = uefiExtractor.parseImage(biosImage);
        logStep("parseImage done", started);
        auto setup = uefiExtractor.findBestSetupModule(root);
        if (!setup.has_value()) {
            throw std::runtime_error("setup module not found");
        }
        std::cerr << "setup_path=" << setup->pathHint << std::endl;
        std::cerr << "setup_size=" << setup->pe32Body.size() << std::endl;
        logStep("findBestSetupModule done", started);

        auto questions = ifrExtractor.extractQuestions(setup->pe32Body);
        std::cerr << "questions=" << questions.size() << std::endl;
        logStep("extractQuestions done", started);

        auto fields = ocb::core::IfrFieldMapper{}.mapQuestions(questions);
        std::cerr << "mapped_fields=" << fields.size() << std::endl;
        logStep("mapQuestions done", started);

        const ocb::core::BiosAnalysisService service(uefiExtractor, ifrExtractor);
        auto result = service.analyze(biosImage);
        std::cerr << "fields=" << result.fields.size() << std::endl;
        std::cerr << "metadata_board=" << result.metadata.boardName << std::endl;
        std::cerr << "metadata_bios_version=" << result.metadata.biosVersion << std::endl;
        logStep("analyze done", started);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << std::endl;
        return 1;
    }
}
