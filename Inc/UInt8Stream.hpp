#ifndef INC_UINT8_STREAM_HPP_
#define INC_UINT8_STREAM_HPP_

#include <cstdint>
#include <cstddef>
#include <string>
#include <array>
#include <memory>
#include <concepts>

#include "imagebuffer/image.hpp"
#include "imagebuffer/imagebuffer.hpp"

//
//
//

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

//
//
//

template <typename Stream>
concept InputStreamConcept = requires(Stream& s) {
    { s.is_empty() } -> std::convertible_to<bool>;
    { s.initialize(std::declval<uint8_t*>(), std::declval<size_t&>()) } -> std::convertible_to<void>;
    { s.size() } -> std::convertible_to<size_t>;
    { s.name() } -> std::convertible_to<std::array<char, NAME_LENGTH>>;
    { s.finalize() } -> std::convertible_to<void>;
    { s.getChunk(std::declval<uint8_t*>(), std::declval<size_t&>()) } -> std::convertible_to<void>;
};
      
template <typename Buffer>
concept ImageBufferConcept = requires(Buffer& b, ImageMetadata& metadata, uint8_t* data, size_t& size) {
    { b.is_empty() } -> std::convertible_to<bool>;
    { b.get_image(metadata) } -> std::convertible_to<ImageBufferError>;
    { b.get_data_chunk(data, size) } -> std::convertible_to<ImageBufferError>;
    { b.pop_image() } -> std::convertible_to<ImageBufferError>;
};

template <ImageBufferConcept Buffer>
class ImageInputStream {
public:
    ImageInputStream(Buffer& buffer) : buffer_(buffer) {}

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

//
//
//

#include <array>

template <typename T>
concept OutputStreamConcept = requires(T& stream, uint8_t* data, size_t size, const std::array<char, NAME_LENGTH>& name) {
    { stream.initialize(name) } -> std::convertible_to<void>;
    { stream.finalize() } -> std::convertible_to<void>;
    { stream.output(data, size) } -> std::convertible_to<void>;
};

class TrivialOuputStream {
public:
    TrivialOuputStream() {}
    void initialize(const std::array<char, NAME_LENGTH>& /* name */) {}
    void finalize() {}
    void output(uint8_t */*data*/, size_t /*size*/) {}
};

static_assert(OutputStreamConcept<TrivialOuputStream>,
              "TrivialOuputStream does not satisfy OutputStreamConcept");

class OuputStreamToFile {
public:
    OuputStreamToFile() : file_(nullptr) {}
    void initialize(const std::array<char, NAME_LENGTH>& name)
    {
        file_ = fopen(name.data(), "wb");
    }
    void finalize()
    {
        fclose(file_);
        file_ = nullptr;
    }
    void output(uint8_t *data, size_t size)
        {
            fwrite(data, sizeof(uint8_t), size, file_);
        }
    private:
    FILE *file_;
};

static_assert(OutputStreamConcept<OuputStreamToFile>,
              "OuputStreamToFile does not satisfy OutputStreamConcept");


#endif // INC_UINT8_STREAM_HPP_