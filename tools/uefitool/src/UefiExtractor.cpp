#include "ocb/tools/uefi/UefiExtractor.hpp"

#include "ffs.h"
#include "ffsparser.h"
#include "treeitem.h"
#include "treemodel.h"
#include "types.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ocb::tools::uefi {
namespace {

std::string toStdString(const UString& value) {
    return value.toLocal8Bit() == nullptr ? std::string{} : std::string(value.toLocal8Bit());
}

std::vector<std::uint8_t> toBytes(const UByteArray& bytes) {
    const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.constData());
    return data == nullptr ? std::vector<std::uint8_t>{} : std::vector<std::uint8_t>(data, data + bytes.size());
}

UByteArray toUByteArray(std::span<const std::uint8_t> bytes) {
    return UByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<int32_t>(bytes.size()));
}

std::string itemTypeName(const TreeModel& model, const UModelIndex& index) {
    const auto type = model.type(index);
    const auto subtype = model.subtype(index);

    if (type == Types::Section) {
        return toStdString(sectionTypeToUString(subtype));
    }
    if (type == Types::File) {
        return toStdString(fileTypeToUString(subtype));
    }
    return toStdString(model.text(index));
}

FirmwareNode buildNode(const TreeModel& model, const UModelIndex& index) {
    FirmwareNode node;
    node.name = toStdString(model.name(index));
    if (node.name.empty()) {
        node.name = toStdString(model.text(index));
    }
    node.type = itemTypeName(model, index);
    node.body = toBytes(model.body(index));

    const auto children = model.rowCount(index);
    node.children.reserve(static_cast<std::size_t>(children));
    for (int row = 0; row < children; ++row) {
        node.children.push_back(buildNode(model, model.index(row, 0, index)));
    }

    return node;
}

bool containsCaseInsensitive(std::string text, std::string needle) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
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

void collectSetupModules(const FirmwareNode& node, const std::string& path, std::vector<SetupModule>& modules) {
    const auto currentPath = path.empty() ? node.name : path + "/" + node.name;
    const auto pathHasSetup = containsCaseInsensitive(currentPath, "setup");
    const auto isPe32 = containsCaseInsensitive(node.type, "pe32") || containsCaseInsensitive(node.name, "pe32");

    if (isPe32 && !node.body.empty() && (pathHasSetup || containsIfrFormSetPackage(node.body))) {
        modules.push_back({currentPath, node.body});
    }

    for (const auto& child : node.children) {
        collectSetupModules(child, currentPath, modules);
    }
}

} // namespace

FirmwareNode UefiToolExtractor::parseImage(std::span<const std::uint8_t> image) const {
    TreeModel model;
    FfsParser parser(&model);

    const auto status = parser.parse(toUByteArray(image));
    if (status != U_SUCCESS) {
        throw std::runtime_error("Парсер UEFITool завершился со статусом " + std::to_string(status));
    }

    FirmwareNode root;
    root.name = "корень";
    root.type = "Корень";

    const auto rootRows = model.rowCount();
    root.children.reserve(static_cast<std::size_t>(rootRows));
    for (int row = 0; row < rootRows; ++row) {
        root.children.push_back(buildNode(model, model.index(row, 0)));
    }

    return root;
}

std::optional<SetupModule> UefiToolExtractor::findBestSetupModule(const FirmwareNode& root) const {
    std::vector<SetupModule> modules;
    collectSetupModules(root, {}, modules);
    if (modules.empty()) {
        return std::nullopt;
    }

    std::sort(modules.begin(), modules.end(), [](const SetupModule& lhs, const SetupModule& rhs) {
        const auto lhsExact = containsCaseInsensitive(lhs.pathHint, "/setup/");
        const auto rhsExact = containsCaseInsensitive(rhs.pathHint, "/setup/");
        if (lhsExact != rhsExact) {
            return lhsExact > rhsExact;
        }
        return lhs.pe32Body.size() > rhs.pe32Body.size();
    });

    return modules.front();
}

} // namespace ocb::tools::uefi
