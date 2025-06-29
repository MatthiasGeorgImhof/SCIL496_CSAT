#ifndef INC_UINT8_STREAM_HPP_
#define INC_UINT8_STREAM_HPP_

#include <cstdint>
#include <cstddef>
#include <string>
#include <array>
#include <memory>

#include "imagebuffer/image.hpp"

constexpr size_t NAME_LENGTH = 11;

std::array<char, NAME_LENGTH> formatValues(uint32_t u32, uint8_t u8)
{
    std::array<char, NAME_LENGTH> result = {'0', '0', '0', '0', '0', '0', '0', '0', '_', '0', '0'};
    static constexpr char hex_digits[] = "0123456789abcdef";

    // Convert uint32_t to 8-character hexadecimal
    for (int i = 7; i >= 0; --i) {
        result[i] = hex_digits[u32 & 0x0f];
        u32 >>= 4;
    }

    // Convert uint8_t to 2-character hexadecimal
    for (int i = 10; i >= 9; --i) {  // Correct loop bounds
        result[i] = hex_digits[u8 & 0x0f];
        u8 >>= 4;
    }

    static_assert((2*sizeof(u32) + 2*sizeof(u8) + 1) == NAME_LENGTH, "formatValues, invalid NAME_LENGTH");
    static_assert((2*sizeof(u32) + 2*sizeof(u8) + 1) == sizeof(result), "formatValues, invalid result length");
    return result;
}



template <typename Buffer>
class UInt8Stream {
public:
    UInt8Stream(Buffer& buffer) : buffer_(buffer) {}

    bool is_empty() {
        return buffer_.is_empty();
    }

    void initialize(uint8_t *data, size_t &size)
    {
        ImageMetadata metadata;
        (void) buffer_.get_image(metadata);
        size_ = metadata.image_size + sizeof(ImageMetadata);
        name_ = formatValues(metadata.timestamp, metadata.camera_index);
        size = sizeof(metadata);
        std::memcpy(data, reinterpret_cast<uint8_t*>(&metadata), sizeof(ImageMetadata));
    }

    size_t size() const {
        return size_;
    }
    
    const std::array<char, NAME_LENGTH> name() const {
        return name_;
    }

    void finalize()
    {
        (void) buffer_.pop_image();
    }

    void getChunk(uint8_t *data, size_t &size)
    {   
        if (size==0)
        {
            finalize();
        }
        else
        {
            (void) buffer_.get_data_chunk(data, size);
        }
    }
    
private:
    Buffer& buffer_;
    size_t size_;
    std::array<char, NAME_LENGTH> name_;
};


#endif // INC_UINT8_STREAM_HPP_