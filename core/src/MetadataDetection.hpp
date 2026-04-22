#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ocb::core::detail {

[[nodiscard]] std::vector<std::string> extractTextStrings(std::span<const std::uint8_t> bytes);
[[nodiscard]] bool containsCaseInsensitive(const std::string& text, const std::string& needle);
[[nodiscard]] std::string chooseBoardName(const std::vector<std::string>& strings);
[[nodiscard]] std::string chooseProfileName(const std::vector<std::string>& strings);
[[nodiscard]] std::string firstBiosVersion(const std::vector<std::string>& strings);
[[nodiscard]] std::string boardNameFromBiosVersion(const std::string& biosVersion);

} // namespace ocb::core::detail
