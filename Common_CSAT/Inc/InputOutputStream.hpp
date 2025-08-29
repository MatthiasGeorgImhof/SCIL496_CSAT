#ifndef INC_INPUTOUTPUTSTREAM_HPP_
#define INC_INPUTOUTPUTSTREAM_HPP_

#include <cstdint>
#include <cstddef>
#include <string>
#include <array>
#include <memory>
#include <concepts>
#include <fstream>

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
    for (size_t i = 0; i < 8; ++i)
    {
        result[i] = hex_digits[u32 & 0x0f];
        u32 >>= 4;
    }

    // Convert uint8_t to 2-character hexadecimal
    for (size_t i = 9; i < 11; ++i)
    { // Correct loop bounds
        result[i] = hex_digits[u8 & 0x0f];
        u8 >>= 4;
    }

    static_assert((2 * sizeof(u32) + 2 * sizeof(u8) + 1) == NAME_LENGTH, "formatValues, invalid NAME_LENGTH");
    static_assert((2 * sizeof(u32) + 2 * sizeof(u8) + 1) == sizeof(result), "formatValues, invalid result length");
    return result;
}

std::array<char, NAME_LENGTH> convertPath(const uint8_t* elements, size_t size) {
    std::array<char, NAME_LENGTH> destination = {}; // Initialize with nulls
    size_t bytes_to_copy = std::min(size, (size_t)NAME_LENGTH - 1);
    std::memcpy(destination.data(), elements, bytes_to_copy);
    destination[bytes_to_copy] = '\0';
    return destination;
}

//
//
//

template <typename Stream>
concept InputStreamConcept = requires(Stream &s) {
    { s.is_empty() } -> std::convertible_to<bool>;
    { s.initialize(std::declval<uint8_t *>(), std::declval<size_t &>()) } -> std::convertible_to<bool>;
    { s.size() } -> std::convertible_to<size_t>;
    { s.name() } -> std::convertible_to<std::array<char, NAME_LENGTH>>;
    { s.finalize() } -> std::convertible_to<bool>;
    { s.getChunk(std::declval<uint8_t *>(), std::declval<size_t &>()) } -> std::convertible_to<bool>;
};

template <typename Buffer>
concept ImageBufferConcept = requires(Buffer &b, ImageMetadata &metadata, uint8_t *data, size_t &size) {
    { b.is_empty() } -> std::convertible_to<bool>;
    { b.get_image(metadata) } -> std::convertible_to<ImageBufferError>;
    { b.get_data_chunk(data, size) } -> std::convertible_to<ImageBufferError>;
    { b.pop_image() } -> std::convertible_to<ImageBufferError>;
};

template <ImageBufferConcept Buffer>
class ImageInputStream
{
public:
    ImageInputStream(Buffer &buffer) : buffer_(buffer) {}
    ~ImageInputStream() = default;

    bool is_empty()
    {
        return buffer_.is_empty();
    }

    bool initialize(uint8_t *data, size_t &size)
    {
        ImageMetadata metadata;
        (void)buffer_.get_image(metadata);
        size_ = metadata.image_size + sizeof(ImageMetadata);
        name_ = formatValues(metadata.timestamp, metadata.camera_index);
        size = sizeof(metadata);
        std::memcpy(data, reinterpret_cast<uint8_t *>(&metadata), sizeof(ImageMetadata));
        return true;
    }

    size_t size() const
    {
        return size_;
    }

    const std::array<char, NAME_LENGTH> name() const
    {
        return name_;
    }

    bool finalize()
    {
        (void)buffer_.pop_image();
        return true;
    }

    bool getChunk(uint8_t *data, size_t &size)
    {
        if (size == 0)
        {
            finalize();
        }
        else
        {
            (void)buffer_.get_data_chunk(data, size);
        }
        return true;
    }

private:
    Buffer &buffer_;
    size_t size_;
    std::array<char, NAME_LENGTH> name_;
};

struct MockImageBuffer
{
    bool is_empty() { return false; }
    ImageBufferError get_image(ImageMetadata & /*metadata*/) { return ImageBufferError::NO_ERROR; }
    ImageBufferError get_data_chunk(uint8_t * /*data*/, size_t & /*size*/) { return ImageBufferError::NO_ERROR; }
    ImageBufferError pop_image() { return ImageBufferError::NO_ERROR; }
};

static_assert(InputStreamConcept<ImageInputStream<MockImageBuffer>>,
              "ImageInputStream does not satisfy InputStreamConcept");

class FileInputStream
{
public:
    FileInputStream(const std::string &filename) : filename_(filename, std::ios::binary)
    {
        filename_.seekg(0, std::ios::end);
        file_size_ = static_cast<size_t>(filename_.tellg());
        filename_.seekg(0, std::ios::beg);
        name_ = generateName(filename);
    }

    ~FileInputStream()
    {
        if (filename_.is_open())
        {
            filename_.close();
        }
    }

    bool is_empty()
    {
        return filename_.peek() == EOF;
    }

    bool initialize(uint8_t *data, size_t &size)
    {
        size = std::min(size, file_size_);
        filename_.read(reinterpret_cast<char *>(data), static_cast<std::streamoff>(size));
        bytes_read_ = size;

        if (filename_.fail() && !filename_.eof())
        {
            size = 0;
            return false;
        }

        initialized_ = true;
        return true;
    }

    size_t size() const
    {
        return file_size_;
    }

    const std::array<char, NAME_LENGTH> name() const
    {
        return name_;
    }

    bool getChunk(uint8_t *data, size_t &size)
    {
        if (!initialized_)
        {
            size = 0;
            return false;
        }

        if (bytes_read_ >= file_size_)
        {
            size = 0;
            finalize();
            return true;
        }

        size_t bytes_to_read = std::min(size, file_size_ - bytes_read_);
        filename_.read(reinterpret_cast<char *>(data), static_cast<std::streamoff>(bytes_to_read));

        if (filename_.fail() && !filename_.eof())
        {
            size = 0;
            return false;
        }

        size = bytes_to_read;
        bytes_read_ += bytes_to_read;
        return true;
    }

    bool finalize()
    {
        if (filename_.is_open())
        {
            filename_.clear();
            filename_.seekg(0, std::ios::beg);
        }
        bytes_read_ = 0;
        initialized_ = false;
        return true;
    }

private:
    std::ifstream filename_;
    size_t file_size_;
    size_t bytes_read_ = 0;
    std::array<char, NAME_LENGTH> name_;
    bool initialized_ = false;

    std::array<char, NAME_LENGTH> generateName(const std::string &filename)
    {
        std::array<char, NAME_LENGTH> name = {};
        strncpy(name.data(), filename.c_str(), NAME_LENGTH - 1);
        name[NAME_LENGTH - 1] = '\0';
        return name;
    }
};

static_assert(InputStreamConcept<FileInputStream>,
              "FileInputStream does not satisfy InputStreamConcept");

//
//
//

template <typename T>
concept OutputStreamConcept = requires(T &stream, uint8_t *data, size_t size, const std::array<char, NAME_LENGTH> &name) {
    { stream.initialize(name) } -> std::convertible_to<bool>;
    { stream.finalize() } -> std::convertible_to<bool>;
    { stream.output(data, size) } -> std::convertible_to<bool>;
};

class TrivialOuputStream
{
public:
    TrivialOuputStream() {}
    bool initialize(const std::array<char, NAME_LENGTH> & /* name */) { return true; }
    bool finalize() { return true; }
    bool output(uint8_t * /*data*/, size_t /*size*/) { return true; }
};

static_assert(OutputStreamConcept<TrivialOuputStream>,
              "TrivialOuputStream does not satisfy OutputStreamConcept");

class OutputStreamToFile
{
public:
    OutputStreamToFile() : file_() {} // default constructor initializes the file_ object
    bool initialize(const std::array<char, NAME_LENGTH> &name)
    {
        file_.open(name.data(), std::ios::binary);
        return (file_.is_open());
    }

    bool finalize()
    {
        if (file_.is_open())
        {
            file_.close(); // RAII: automatically closes the file if it was open.
        }
        return true;
    }

    bool output(uint8_t *data, size_t size)
    {
        file_.write(reinterpret_cast<const char *>(data), static_cast<std::streamoff>(size));
        return (!file_.fail());
    }

private:
    std::ofstream file_; // Use std::ofstream
};

static_assert(OutputStreamConcept<OutputStreamToFile>,
              "OutputStreamToFile does not satisfy OutputStreamConcept");

#endif // INC_INPUTOUTPUTSTREAM_HPP_