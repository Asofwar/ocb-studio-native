#pragma once

#include "ocb/core/OcbField.hpp"

#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ocb::core {

class FieldCatalog final {
public:
    FieldCatalog();
    explicit FieldCatalog(std::vector<OcbField> fields);

    void resetToBuiltin();
    void merge(std::span<const OcbField> fields);

    [[nodiscard]] const std::vector<OcbField>& fields() const noexcept;
    [[nodiscard]] std::vector<OcbField> builtinOnly() const;
    [[nodiscard]] std::vector<OcbField> search(std::string_view query) const;
    [[nodiscard]] const OcbField* findById(std::string_view id) const;
    [[nodiscard]] const OcbField* findByPrompt(std::string_view prompt) const;

private:
    std::vector<OcbField> fields_;
    std::unordered_map<std::string, std::size_t> indexById_;

    void rebuildIndex();
};

} // namespace ocb::core
