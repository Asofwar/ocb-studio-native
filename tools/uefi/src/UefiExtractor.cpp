#include "ocb/tools/uefi/UefiExtractor.hpp"

extern "C" {
#include "LzmaDec.h"
}

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ocb::tools::uefi {
namespace {

constexpr std::uint8_t kSectionTypeCompression = 0x01;
constexpr std::uint8_t kSectionTypeGuidDefined = 0x02;
constexpr std::uint8_t kSectionTypePe32 = 0x10;
constexpr std::uint8_t kSectionTypeTe = 0x12;
constexpr std::uint8_t kFfsLargeFile = 0x01;
constexpr std::uint8_t kCompressionNone = 0x00;
constexpr std::uint8_t kCompressionStandard = 0x01;

constexpr std::array<std::uint8_t, 16> kLzmaGuid{
    0x98, 0x58, 0x4E, 0xEE, 0x14, 0x39, 0x59, 0x42,
    0x9D, 0x6E, 0xDC, 0x7B, 0xD7, 0x94, 0x03, 0xCF};
constexpr std::array<std::uint8_t, 16> kLzmaHpGuid{
    0x23, 0x5E, 0xD8, 0x0E, 0x53, 0xF2, 0x3F, 0x41,
    0xA0, 0x3C, 0x90, 0x19, 0x87, 0xB0, 0x43, 0x97};
constexpr std::array<std::uint8_t, 16> kLzmaMsGuid{
    0xEA, 0x21, 0x99, 0xBD, 0x91, 0xED, 0x4A, 0x40,
    0x8B, 0x2F, 0xB4, 0xD7, 0x24, 0x74, 0x7C, 0x8C};
constexpr std::array<std::uint8_t, 16> kLzmaF86Guid{
    0xBD, 0xE6, 0x2A, 0xD4, 0x52, 0x13, 0xFB, 0x4B,
    0x90, 0x9A, 0xCA, 0x72, 0xA6, 0xEA, 0xE8, 0x89};

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

[[nodiscard]] std::size_t alignUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1U) & ~(alignment - 1U);
}

[[nodiscard]] std::vector<std::uint8_t> copyBytes(ByteView view, std::size_t begin, std::size_t end) {
    if (begin >= end || end > view.bytes.size()) {
        return {};
    }
    return {view.bytes.begin() + static_cast<std::ptrdiff_t>(begin),
            view.bytes.begin() + static_cast<std::ptrdiff_t>(end)};
}

[[nodiscard]] bool equalsAscii(ByteView view, std::size_t offset, std::string_view value) {
    if (!view.has(offset, value.size())) {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (view.u8(offset + index) != static_cast<std::uint8_t>(value[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string hexByte(std::uint8_t value) {
    constexpr std::array<char, 16> kHex{'0', '1', '2', '3', '4', '5', '6', '7',
                                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string result(2, '0');
    result[0] = kHex[value >> 4U];
    result[1] = kHex[value & 0x0FU];
    return result;
}

[[nodiscard]] std::string guidAt(ByteView view, std::size_t offset) {
    if (!view.has(offset, 16)) {
        return {};
    }

    auto result = std::string{};
    auto appendHex = [&result](std::uint64_t value, int width) {
        auto buffer = std::array<char, 16>{};
        auto [ptr, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 16);
        if (error != std::errc{}) {
            return;
        }
        result.append(static_cast<std::size_t>(width - (ptr - buffer.data())), '0');
        result.append(buffer.data(), ptr);
    };

    appendHex(view.u32(offset), 8);
    result.push_back('-');
    appendHex(view.u16(offset + 4), 4);
    result.push_back('-');
    appendHex(view.u16(offset + 6), 4);
    result.push_back('-');
    result += hexByte(view.u8(offset + 8));
    result += hexByte(view.u8(offset + 9));
    result.push_back('-');
    for (std::size_t index = 10; index < 16; ++index) {
        result += hexByte(view.u8(offset + index));
    }
    return result;
}

[[nodiscard]] bool guidEquals(ByteView view, std::size_t offset, const std::array<std::uint8_t, 16>& guid) {
    if (!view.has(offset, guid.size())) {
        return false;
    }
    for (std::size_t index = 0; index < guid.size(); ++index) {
        if (view.u8(offset + index) != guid[index]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool containsCaseInsensitive(std::string text, std::string needle) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text.find(needle) != std::string::npos;
}

[[nodiscard]] std::string sectionTypeName(std::uint8_t type) {
    switch (type) {
    case kSectionTypeCompression:
        return "Compression section";
    case kSectionTypeGuidDefined:
        return "GUID-defined section";
    case kSectionTypePe32:
        return "PE32 section";
    case kSectionTypeTe:
        return "TE section";
    case 0x19:
        return "RAW section";
    default:
        return "Section 0x" + hexByte(type);
    }
}

[[nodiscard]] std::uint32_t readHiiLength(const std::vector<std::uint8_t>& body, std::size_t offset) {
    return static_cast<std::uint32_t>(body[offset])
        | (static_cast<std::uint32_t>(body[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(body[offset + 2]) << 16);
}

[[nodiscard]] bool containsIfrFormSetPackage(const std::vector<std::uint8_t>& body) {
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

[[nodiscard]] bool isPadding(ByteView view, std::size_t offset, std::size_t end) {
    const auto scanEnd = std::min(offset + 24U, end);
    for (auto index = offset; index < scanEnd; ++index) {
        if (view.u8(index) != 0xFF && view.u8(index) != 0x00) {
            return false;
        }
    }
    return true;
}

class EfiStandardDecompressor {
public:
    enum class Variant {
        Efi11,
        Tiano,
    };

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> decompress(ByteView source, Variant variant) {
        if (!source.has(0, 8)) {
            return std::nullopt;
        }

        const auto compressedSize = source.u32(0);
        const auto originalSize = source.u32(4);
        if (compressedSize > source.bytes.size() - 8U) {
            return std::nullopt;
        }

        compressed_ = source.bytes.subspan(8, compressedSize);
        output_.assign(originalSize, 0);
        inputOffset_ = 0;
        outputOffset_ = 0;
        bitCount_ = 0;
        bitBuffer_ = 0;
        subBitBuffer_ = 0;
        blockSize_ = 0;
        badTable_ = false;
        pBit_ = variant == Variant::Tiano ? 5 : 4;

        left_.fill(0);
        right_.fill(0);
        cLen_.fill(0);
        ptLen_.fill(0);
        cTable_.fill(0);
        ptTable_.fill(0);

        fillBuffer(kBitBufferSize);
        decode();
        if (badTable_ || outputOffset_ != output_.size()) {
            return std::nullopt;
        }
        return output_;
    }

private:
    static constexpr std::uint16_t kBitBufferSize = 32;
    static constexpr std::uint16_t kMaxMatch = 256;
    static constexpr std::uint16_t kThreshold = 3;
    static constexpr std::uint16_t kCodeBits = 16;
    static constexpr std::uint16_t kNC = 0xFF + kMaxMatch + 2 - kThreshold;
    static constexpr std::uint16_t kCBits = 9;
    static constexpr std::uint16_t kMaxPBits = 5;
    static constexpr std::uint16_t kTBits = 5;
    static constexpr std::uint16_t kMaxNP = (1U << kMaxPBits) - 1U;
    static constexpr std::uint16_t kNT = kCodeBits + 3;
    static constexpr std::uint16_t kNPT = kNT > kMaxNP ? kNT : kMaxNP;

    std::span<const std::uint8_t> compressed_;
    std::vector<std::uint8_t> output_;
    std::size_t inputOffset_{};
    std::size_t outputOffset_{};
    std::uint16_t bitCount_{};
    std::uint32_t bitBuffer_{};
    std::uint32_t subBitBuffer_{};
    std::uint16_t blockSize_{};
    std::uint8_t pBit_{};
    bool badTable_{};
    std::array<std::uint16_t, 2 * kNC - 1> left_{};
    std::array<std::uint16_t, 2 * kNC - 1> right_{};
    std::array<std::uint8_t, kNC> cLen_{};
    std::array<std::uint8_t, kNPT> ptLen_{};
    std::array<std::uint16_t, 4096> cTable_{};
    std::array<std::uint16_t, 256> ptTable_{};

    void fillBuffer(std::uint16_t bits) {
        bitBuffer_ <<= bits;

        while (bits > bitCount_) {
            bits = static_cast<std::uint16_t>(bits - bitCount_);
            bitBuffer_ |= subBitBuffer_ << bits;

            if (inputOffset_ < compressed_.size()) {
                subBitBuffer_ = compressed_[inputOffset_++];
                bitCount_ = 8;
            } else {
                subBitBuffer_ = 0;
                bitCount_ = 8;
            }
        }

        bitCount_ = static_cast<std::uint16_t>(bitCount_ - bits);
        bitBuffer_ |= subBitBuffer_ >> bitCount_;
    }

    [[nodiscard]] std::uint32_t getBits(std::uint16_t bits) {
        const auto value = bitBuffer_ >> (kBitBufferSize - bits);
        fillBuffer(bits);
        return value;
    }

    [[nodiscard]] bool makeTable(
        std::uint16_t numChars,
        const std::uint8_t* lengths,
        std::uint16_t tableBits,
        std::uint16_t* table,
        std::size_t tableSize) {
        if (tableBits >= 17) {
            return false;
        }

        std::array<std::uint16_t, 17> count{};
        std::array<std::uint16_t, 17> weight{};
        std::array<std::uint16_t, 18> start{};

        for (std::uint16_t index = 0; index < numChars; ++index) {
            if (lengths[index] > 16) {
                return false;
            }
            ++count[lengths[index]];
        }

        for (std::uint16_t index = 1; index <= 16; ++index) {
            start[index + 1] = static_cast<std::uint16_t>(start[index] + (count[index] << (16 - index)));
        }
        if (start[17] != 0) {
            return false;
        }

        const auto shift = static_cast<std::uint16_t>(16 - tableBits);
        for (std::uint16_t index = 1; index <= tableBits; ++index) {
            start[index] >>= shift;
            weight[index] = static_cast<std::uint16_t>(1U << (tableBits - index));
        }
        for (std::uint16_t index = static_cast<std::uint16_t>(tableBits + 1); index <= 16; ++index) {
            weight[index] = static_cast<std::uint16_t>(1U << (16 - index));
        }

        const auto tableEntries = static_cast<std::uint16_t>(1U << tableBits);
        if (tableEntries > tableSize) {
            return false;
        }
        const auto clearFrom = static_cast<std::uint16_t>(start[tableBits + 1] >> shift);
        if (clearFrom < tableEntries) {
            std::fill(table + clearFrom, table + tableEntries, std::uint16_t{0});
        }

        auto available = numChars;
        const auto mask = static_cast<std::uint16_t>(1U << (15 - tableBits));
        for (std::uint16_t symbol = 0; symbol < numChars; ++symbol) {
            const auto length = lengths[symbol];
            if (length == 0 || length >= 17) {
                continue;
            }

            const auto nextCode = static_cast<std::uint16_t>(start[length] + weight[length]);
            if (length <= tableBits) {
                if (nextCode > tableEntries) {
                    return false;
                }
                std::fill(table + start[length], table + nextCode, symbol);
            } else {
                auto code = start[length];
                auto* branch = &table[code >> shift];
                auto depth = static_cast<std::uint16_t>(length - tableBits);
                while (depth != 0) {
                    if (*branch == 0) {
                        if (available >= left_.size()) {
                            return false;
                        }
                        left_[available] = 0;
                        right_[available] = 0;
                        *branch = available++;
                    }

                    if (*branch >= left_.size()) {
                        return false;
                    }
                    branch = (code & mask) != 0 ? &right_[*branch] : &left_[*branch];
                    code <<= 1U;
                    --depth;
                }
                *branch = symbol;
            }
            start[length] = nextCode;
        }

        return true;
    }

    [[nodiscard]] std::optional<std::uint16_t> readPTLengths(
        std::uint16_t symbolCount,
        std::uint16_t bitCount,
        std::optional<std::uint16_t> specialIndex) {
        const auto number = static_cast<std::uint16_t>(getBits(bitCount));
        if (number > ptLen_.size() || symbolCount > ptLen_.size()) {
            return std::nullopt;
        }

        if (number == 0) {
            const auto symbol = static_cast<std::uint16_t>(getBits(bitCount));
            std::fill(ptTable_.begin(), ptTable_.end(), symbol);
            std::fill(ptLen_.begin(), ptLen_.begin() + symbolCount, std::uint8_t{0});
            return std::uint16_t{0};
        }

        std::uint16_t index = 0;
        while (index < number && index < kNPT) {
            auto length = static_cast<std::uint16_t>(bitBuffer_ >> (kBitBufferSize - 3));
            if (length == 7) {
                auto mask = 1U << (kBitBufferSize - 1U - 3U);
                while ((mask & bitBuffer_) != 0) {
                    mask >>= 1U;
                    ++length;
                }
            }

            fillBuffer(static_cast<std::uint16_t>(length < 7 ? 3 : length - 3));
            ptLen_[index++] = static_cast<std::uint8_t>(length);

            if (specialIndex.has_value() && index == *specialIndex) {
                auto zeros = static_cast<std::int32_t>(getBits(2));
                while (--zeros >= 0 && index < kNPT) {
                    ptLen_[index++] = std::uint8_t{0};
                }
            }
        }

        while (index < symbolCount && index < kNPT) {
            ptLen_[index++] = std::uint8_t{0};
        }

        return makeTable(symbolCount, ptLen_.data(), 8, ptTable_.data(), ptTable_.size())
            ? std::optional<std::uint16_t>{std::uint16_t{0}}
            : std::nullopt;
    }

    [[nodiscard]] std::optional<std::uint16_t> readCLengths() {
        const auto number = static_cast<std::uint16_t>(getBits(kCBits));
        if (number == 0) {
            const auto symbol = static_cast<std::uint16_t>(getBits(kCBits));
            std::fill(cLen_.begin(), cLen_.end(), std::uint8_t{0});
            std::fill(cTable_.begin(), cTable_.end(), symbol);
            return std::uint16_t{0};
        }

        std::uint16_t index = 0;
        while (index < number && index < kNC) {
            auto symbol = ptTable_[bitBuffer_ >> (kBitBufferSize - 8)];
            if (symbol >= kNT) {
                auto mask = 1U << (kBitBufferSize - 1U - 8U);
                do {
                    if (symbol >= left_.size()) {
                        return std::nullopt;
                    }
                    symbol = (mask & bitBuffer_) != 0 ? right_[symbol] : left_[symbol];
                    mask >>= 1U;
                } while (symbol >= kNT);
            }

            fillBuffer(ptLen_[symbol]);

            if (symbol <= 2) {
                if (symbol == 0) {
                    symbol = 1;
                } else if (symbol == 1) {
                    symbol = static_cast<std::uint16_t>(getBits(4) + 3);
                } else {
                    symbol = static_cast<std::uint16_t>(getBits(kCBits) + 20);
                }

                auto zeros = static_cast<std::int32_t>(symbol);
                while (--zeros >= 0 && index < kNC) {
                    cLen_[index++] = std::uint8_t{0};
                }
            } else {
                cLen_[index++] = static_cast<std::uint8_t>(symbol - 2);
            }
        }

        std::fill(cLen_.begin() + index, cLen_.end(), std::uint8_t{0});
        return makeTable(kNC, cLen_.data(), 12, cTable_.data(), cTable_.size())
            ? std::optional<std::uint16_t>{std::uint16_t{0}}
            : std::nullopt;
    }

    [[nodiscard]] std::optional<std::uint16_t> decodeC() {
        if (blockSize_ == 0) {
            blockSize_ = static_cast<std::uint16_t>(getBits(16));
            if (!readPTLengths(kNT, kTBits, std::uint16_t{3}).has_value()) {
                return std::nullopt;
            }
            if (!readCLengths().has_value()) {
                return std::nullopt;
            }
            if (!readPTLengths(kMaxNP, pBit_, std::nullopt).has_value()) {
                return std::nullopt;
            }
        }

        --blockSize_;
        auto symbol = cTable_[bitBuffer_ >> (kBitBufferSize - 12)];
        if (symbol >= kNC) {
            auto mask = 1U << (kBitBufferSize - 1U - 12U);
            do {
                if (symbol >= left_.size()) {
                    return std::nullopt;
                }
                symbol = (mask & bitBuffer_) != 0 ? right_[symbol] : left_[symbol];
                mask >>= 1U;
            } while (symbol >= kNC);
        }

        fillBuffer(cLen_[symbol]);
        return symbol;
    }

    [[nodiscard]] std::optional<std::uint32_t> decodeP() {
        auto symbol = ptTable_[bitBuffer_ >> (kBitBufferSize - 8)];
        if (symbol >= kMaxNP) {
            auto mask = 1U << (kBitBufferSize - 1U - 8U);
            do {
                if (symbol >= left_.size()) {
                    return std::nullopt;
                }
                symbol = (mask & bitBuffer_) != 0 ? right_[symbol] : left_[symbol];
                mask >>= 1U;
            } while (symbol >= kMaxNP);
        }

        fillBuffer(ptLen_[symbol]);
        if (symbol <= 1) {
            return symbol;
        }
        return (1U << (symbol - 1U)) + getBits(static_cast<std::uint16_t>(symbol - 1U));
    }

    void decode() {
        while (outputOffset_ < output_.size()) {
            const auto maybeSymbol = decodeC();
            if (!maybeSymbol.has_value()) {
                badTable_ = true;
                return;
            }
            auto symbol = *maybeSymbol;

            if (symbol < 256) {
                output_[outputOffset_++] = static_cast<std::uint8_t>(symbol);
                continue;
            }

            symbol = static_cast<std::uint16_t>(symbol - (0x100U - kThreshold));
            const auto maybePosition = decodeP();
            if (!maybePosition.has_value() || *maybePosition >= outputOffset_) {
                badTable_ = true;
                return;
            }

            auto sourceOffset = outputOffset_ - *maybePosition - 1U;
            for (std::uint16_t remaining = symbol; remaining > 0; --remaining) {
                if (sourceOffset >= outputOffset_ || outputOffset_ >= output_.size()) {
                    badTable_ = true;
                    return;
                }
                output_[outputOffset_++] = output_[sourceOffset++];
            }
        }
    }
};

void* lzmaAlloc(ISzAllocPtr, std::size_t size) {
    return std::malloc(size);
}

void lzmaFree(ISzAllocPtr, void* address) {
    std::free(address);
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> decodeLzma(ByteView source) {
    constexpr std::size_t kLzmaHeaderSize = LZMA_PROPS_SIZE + 8U;
    if (!source.has(0, kLzmaHeaderSize)) {
        return std::nullopt;
    }

    std::uint64_t decodedSize = 0;
    for (std::size_t index = 0; index < 8; ++index) {
        decodedSize |= static_cast<std::uint64_t>(source.u8(LZMA_PROPS_SIZE + index)) << (index * 8U);
    }
    if (decodedSize == 0 || decodedSize > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
        return std::nullopt;
    }

    auto output = std::vector<std::uint8_t>(static_cast<std::size_t>(decodedSize));
    auto outputSize = static_cast<SizeT>(output.size());
    auto inputSize = static_cast<SizeT>(source.bytes.size() - kLzmaHeaderSize);
    auto status = ELzmaStatus{};
    auto allocator = ISzAlloc{lzmaAlloc, lzmaFree};
    const auto result = LzmaDecode(
        output.data(),
        &outputSize,
        source.bytes.data() + kLzmaHeaderSize,
        &inputSize,
        source.bytes.data(),
        LZMA_PROPS_SIZE,
        LZMA_FINISH_END,
        &status,
        &allocator);

    if (result != SZ_OK || outputSize != output.size()) {
        return std::nullopt;
    }
    return output;
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> decodeLzmaSection(ByteView source) {
    if (auto decoded = decodeLzma(source); decoded.has_value()) {
        return decoded;
    }
    if (source.has(4, 1)) {
        return decodeLzma(ByteView{source.bytes.subspan(4)});
    }
    return std::nullopt;
}

void parseSections(ByteView view, std::size_t begin, std::size_t end, FirmwareNode& parent);

void parseGuidDefinedSection(
    ByteView view,
    std::size_t sectionBegin,
    std::size_t bodyBegin,
    std::size_t end,
    FirmwareNode& node) {
    const auto begin = bodyBegin;
    if (!view.has(begin, 20)) {
        return;
    }

    node.guid = guidAt(view, begin);
    const auto dataOffset = view.u16(begin + 16);
    if (dataOffset < bodyBegin - sectionBegin || sectionBegin + dataOffset >= end) {
        return;
    }

    const auto dataBegin = sectionBegin + dataOffset;
    if (guidEquals(view, begin, kLzmaGuid)
        || guidEquals(view, begin, kLzmaHpGuid)
        || guidEquals(view, begin, kLzmaMsGuid)
        || guidEquals(view, begin, kLzmaF86Guid)) {
        auto decoded = decodeLzmaSection(ByteView{view.bytes.subspan(dataBegin, end - dataBegin)});
        if (decoded.has_value()) {
            parseSections(ByteView{std::span<const std::uint8_t>(*decoded)}, 0, decoded->size(), node);
        }
        return;
    }

    parseSections(view, dataBegin, end, node);
}

void parseCompressionSection(ByteView view, std::size_t begin, std::size_t end, FirmwareNode& node) {
    if (!view.has(begin, 5)) {
        return;
    }

    const auto compressionType = view.u8(begin + 4);
    const auto uncompressedSize = view.u32(begin);
    node.name += compressionType == kCompressionNone ? " (uncompressed)" : " (compressed)";
    if (compressionType == kCompressionNone && begin + 5 < end) {
        parseSections(view, begin + 5, end, node);
        return;
    }

    if (compressionType != kCompressionStandard || begin + 5 >= end) {
        return;
    }

    const auto compressed = ByteView{view.bytes.subspan(begin + 5, end - begin - 5)};
    EfiStandardDecompressor decompressor;
    auto decompressed = decompressor.decompress(compressed, EfiStandardDecompressor::Variant::Tiano);
    if (!decompressed.has_value() || decompressed->size() != uncompressedSize) {
        decompressed = decompressor.decompress(compressed, EfiStandardDecompressor::Variant::Efi11);
    }
    if (!decompressed.has_value() || decompressed->size() != uncompressedSize) {
        return;
    }

    auto childView = ByteView{std::span<const std::uint8_t>(*decompressed)};
    const auto childCount = node.children.size();
    parseSections(childView, 0, decompressed->size(), node);
    if (node.children.size() == childCount) {
        const auto fallback = decompressor.decompress(compressed, EfiStandardDecompressor::Variant::Efi11);
        if (fallback.has_value() && fallback->size() == uncompressedSize) {
            FirmwareNode retry;
            retry.name = node.name;
            retry.type = node.type;
            parseSections(ByteView{std::span<const std::uint8_t>(*fallback)}, 0, fallback->size(), retry);
            if (!retry.children.empty()) {
                node.children = std::move(retry.children);
            }
        }
    }
}

void parseSections(ByteView view, std::size_t begin, std::size_t end, FirmwareNode& parent) {
    for (auto offset = begin; offset + 4 <= end;) {
        auto length = static_cast<std::uint64_t>(view.u24(offset));
        auto headerSize = std::size_t{4};
        const auto type = view.u8(offset + 3);

        if (length == 0xFFFFFFU) {
            if (!view.has(offset, 8)) {
                break;
            }
            length = view.u32(offset + 4);
            headerSize = 8;
        }

        if (length < headerSize || length > end - offset) {
            break;
        }

        const auto dataBegin = offset + headerSize;
        const auto dataEnd = offset + static_cast<std::size_t>(length);

        FirmwareNode node;
        node.name = sectionTypeName(type);
        node.type = node.name;
        node.body = copyBytes(view, dataBegin, dataEnd);
        if (type == kSectionTypeGuidDefined) {
            parseGuidDefinedSection(view, offset, dataBegin, dataEnd, node);
        } else if (type == kSectionTypeCompression) {
            parseCompressionSection(view, dataBegin, dataEnd, node);
        }

        parent.children.push_back(std::move(node));
        offset = alignUp(dataEnd, 4);
    }
}

void parseFirmwareVolume(ByteView view, std::size_t begin, std::size_t end, FirmwareNode& root) {
    if (!view.has(begin, 0x38) || !equalsAscii(view, begin + 0x28, "_FVH")) {
        return;
    }

    const auto volumeLength = view.u64(begin + 0x20);
    const auto headerLength = view.u16(begin + 0x30);
    if (volumeLength < headerLength || volumeLength > end - begin || headerLength < 0x38) {
        return;
    }

    const auto volumeEnd = begin + static_cast<std::size_t>(volumeLength);
    FirmwareNode volume;
    volume.name = "Firmware volume";
    volume.guid = guidAt(view, begin + 0x10);
    volume.type = "Firmware volume";

    for (auto offset = alignUp(begin + headerLength, 8); offset + 24 <= volumeEnd;) {
        if (isPadding(view, offset, volumeEnd)) {
            offset = alignUp(offset + 8, 8);
            continue;
        }

        const auto attributes = view.u8(offset + 19);
        auto fileSize = static_cast<std::uint64_t>(view.u24(offset + 20));
        auto headerSize = std::size_t{24};
        if ((attributes & kFfsLargeFile) != 0) {
            if (!view.has(offset, 32)) {
                break;
            }
            fileSize = view.u64(offset + 24);
            headerSize = 32;
        }

        if (fileSize < headerSize || fileSize > volumeEnd - offset) {
            offset = alignUp(offset + 8, 8);
            continue;
        }

        FirmwareNode file;
        file.guid = guidAt(view, offset);
        file.name = "FFS " + file.guid;
        file.type = "FFS file 0x" + hexByte(view.u8(offset + 18));
        parseSections(view, offset + headerSize, offset + static_cast<std::size_t>(fileSize), file);

        volume.children.push_back(std::move(file));
        offset = alignUp(offset + static_cast<std::size_t>(fileSize), 8);
    }

    root.children.push_back(std::move(volume));
}

[[nodiscard]] bool hasExecutablePayload(const FirmwareNode& node) {
    if (containsCaseInsensitive(node.type, "pe32") || containsCaseInsensitive(node.type, "te")) {
        return true;
    }
    return std::any_of(node.children.begin(), node.children.end(), [](const FirmwareNode& child) {
        return hasExecutablePayload(child);
    });
}

[[nodiscard]] std::vector<std::uint8_t> largestBody(const FirmwareNode& node) {
    auto best = node.body;
    for (const auto& child : node.children) {
        auto childBest = largestBody(child);
        if (childBest.size() > best.size()) {
            best = std::move(childBest);
        }
    }
    return best;
}

void collectSetupModules(const FirmwareNode& node, const std::string& path, std::vector<SetupModule>& modules) {
    const auto currentPath = path.empty() ? node.name : path + "/" + node.name;
    const auto pathHasSetup = containsCaseInsensitive(currentPath, "setup");
    const auto isPe = containsCaseInsensitive(node.type, "pe32") || containsCaseInsensitive(node.type, "te");

    if (isPe && !node.body.empty() && (pathHasSetup || containsIfrFormSetPackage(node.body))) {
        modules.push_back({currentPath, node.body});
    }
    if (!node.body.empty() && containsIfrFormSetPackage(node.body)) {
        modules.push_back({currentPath, node.body});
    }
    if (containsCaseInsensitive(node.type, "ffs")
        && !pathHasSetup
        && hasExecutablePayload(node)) {
        auto body = largestBody(node);
        if (!body.empty() && containsIfrFormSetPackage(body)) {
            modules.push_back({currentPath, std::move(body)});
        }
    }

    for (const auto& child : node.children) {
        collectSetupModules(child, currentPath, modules);
    }
}

} // namespace

FirmwareNode NativeUefiExtractor::parseImage(std::span<const std::uint8_t> image) const {
    FirmwareNode root;
    root.name = "root";
    root.type = "raw firmware image";
    root.body = {image.begin(), image.end()};

    const auto view = ByteView{image};
    for (std::size_t offset = 0; offset + 0x38 <= image.size(); ++offset) {
        if (equalsAscii(view, offset + 0x28, "_FVH")) {
            parseFirmwareVolume(view, offset, image.size(), root);
        }
    }

    return root;
}

std::optional<SetupModule> NativeUefiExtractor::findBestSetupModule(const FirmwareNode& root) const {
    std::vector<SetupModule> modules;
    collectSetupModules(root, {}, modules);

    if (modules.empty() && containsIfrFormSetPackage(root.body)) {
        modules.push_back({"root/raw firmware image", root.body});
    }

    if (modules.empty()) {
        return std::nullopt;
    }

    std::sort(modules.begin(), modules.end(), [](const SetupModule& lhs, const SetupModule& rhs) {
        const auto lhsSetup = containsCaseInsensitive(lhs.pathHint, "setup");
        const auto rhsSetup = containsCaseInsensitive(rhs.pathHint, "setup");
        if (lhsSetup != rhsSetup) {
            return lhsSetup > rhsSetup;
        }
        const auto lhsIfr = containsIfrFormSetPackage(lhs.pe32Body);
        const auto rhsIfr = containsIfrFormSetPackage(rhs.pe32Body);
        if (lhsIfr != rhsIfr) {
            return lhsIfr > rhsIfr;
        }
        return lhs.pe32Body.size() > rhs.pe32Body.size();
    });

    return modules.front();
}

} // namespace ocb::tools::uefi
