#pragma once

#include <stdexcept>
#include <string>

namespace ocb::core {

class OcbException : public std::runtime_error {
public:
    explicit OcbException(const std::string& message)
        : std::runtime_error(message) {}
};

} // namespace ocb::core
