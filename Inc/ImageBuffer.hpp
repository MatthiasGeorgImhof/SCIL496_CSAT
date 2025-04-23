#ifndef IMAGE_BUFFER_H
#define IMAGE_BUFFER_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring>
#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>
#include "Checksum.hpp"

// constexpr static uint32_t IMAGE_MAGIC = ('I' << 24) | ('M' << 16) | ('T' << 8) | 'A';
constexpr static uint32_t IMAGE_MAGIC = 0xfeebcafe;
typedef uint32_t image_magic_t;

void print(const uint8_t *data, size_t size)
{
    printf("           ");
    for (size_t i = 0; i < size; ++i)
    {
        printf("%2x ", data[i]);
    }
    printf("\r\n");
}

// Data Structures (same as before)
struct ImageMetadata
{
    image_magic_t magic = IMAGE_MAGIC;
    uint32_t timestamp;
    size_t image_size;
    float latitude;
    float longitude;
    uint8_t camera_index;
    mutable crc_t checksum;
};
constexpr size_t METADATA_SIZE_WO_CHECKSUM = offsetof(ImageMetadata, checksum);
constexpr size_t METADATA_SIZE = METADATA_SIZE_WO_CHECKSUM + sizeof(crc_t);

struct BufferState
{
    size_t head_;
    size_t tail_;
    size_t size_;
    const size_t FLASH_START_ADDRESS_;
    const size_t TOTAL_BUFFER_CAPACITY_;
    uint32_t checksum;

    bool is_empty() const;
    size_t size() const;
    size_t available() const;
    size_t capacity() const;

    size_t get_head() const;
    size_t get_tail() const;

    BufferState(size_t head, size_t tail, size_t size, size_t flash_start, size_t total_capacity) : head_(head),
                                                                                                    tail_(tail),
                                                                                                    size_(size),
                                                                                                    FLASH_START_ADDRESS_(flash_start),
                                                                                                    TOTAL_BUFFER_CAPACITY_(total_capacity) {}
};
constexpr size_t BUFFERSTATE_SIZE = sizeof(BufferState);
bool BufferState::is_empty() const { return size_ == 0; }
size_t BufferState::size() const { return size_; };
size_t BufferState::available() const { return TOTAL_BUFFER_CAPACITY_ - size_; };
size_t BufferState::capacity() const { return TOTAL_BUFFER_CAPACITY_; };

// Getter methods for test purposes
size_t BufferState::get_head() const { return head_; }
size_t BufferState::get_tail() const { return tail_; }

template <typename Accessor>
class ImageBuffer
{
public:
    ImageBuffer(Accessor &access, size_t flash_start, size_t total_size)
        : buffer_state_(0, 0, 0, flash_start, total_size), access_(access) {}

    int add_image(const uint8_t *image_data, const ImageMetadata &metadata);
    std::vector<uint8_t> read_next_image(ImageMetadata &metadata);
    bool drop_image();

    inline bool is_empty() const { return buffer_state_.is_empty(); };
    inline size_t size() const { return buffer_state_.size(); };
    inline size_t available() const { return buffer_state_.available(); };
    inline size_t capacity() const { return buffer_state_.capacity(); };

    // Getter methods for test purposes
    inline size_t get_head() const { return buffer_state_.get_head(); }
    inline size_t get_tail() const { return buffer_state_.get_tail(); }

private:
    bool verify(const size_t ptr);
    bool has_enough_space(size_t data_size) const { return (buffer_state_.available() >= data_size); };
    void wrap_around(size_t &address);
    size_t calculate_initial_checksum();
    int32_t write(size_t address, const uint8_t *data, size_t size);
    int32_t read(size_t address, uint8_t *data, size_t size);

private:
    BufferState buffer_state_;
    Accessor &access_;
    ChecksumCalculator checksum_calculator_;
};

template <typename Accessor>
int32_t ImageBuffer<Accessor>::write(size_t address, const uint8_t *data, size_t size)
{
    if (address + buffer_state_.size_ <= buffer_state_.TOTAL_BUFFER_CAPACITY_)
    {
        int32_t status = access_.write(address + buffer_state_.FLASH_START_ADDRESS_, data, size);
        return status;
    }

    size_t first_part = buffer_state_.TOTAL_BUFFER_CAPACITY_ - address;
    size_t second_part = size - first_part;
    int32_t status = access_.write(address + buffer_state_.FLASH_START_ADDRESS_, data, first_part);
    if (status != 0)
    {
        std::cerr << "Write failure" << std::endl;
        return -1;
    }
    status = access_.write(buffer_state_.FLASH_START_ADDRESS_, data + first_part, second_part);
    if (status != 0)
    {
        std::cerr << "Write failure" << std::endl;
        return -1;
    }
    return 0;
}

template <typename Accessor>
int32_t ImageBuffer<Accessor>::read(size_t address, uint8_t *data, size_t size)
{
    if (address + size <= buffer_state_.TOTAL_BUFFER_CAPACITY_)
    {
        int32_t status = access_.read(address + buffer_state_.FLASH_START_ADDRESS_, data, size);
        return status;
    }

    size_t first_part = buffer_state_.TOTAL_BUFFER_CAPACITY_ - address;
    size_t second_part = size - first_part;
    int32_t status = access_.read(address + buffer_state_.FLASH_START_ADDRESS_, data, first_part);
    if (status != 0)
    {
        std::cerr << "Read failure" << std::endl;
        return -1;
    }
    status = access_.read(buffer_state_.FLASH_START_ADDRESS_, data + first_part, second_part);
    if (status != 0)
    {
        std::cerr << "Read failure" << std::endl;
        return -1;
    }
    return 0;
}

template <typename Accessor>
void ImageBuffer<Accessor>::wrap_around(size_t &address)
{
    if (address >= buffer_state_.TOTAL_BUFFER_CAPACITY_)
    {
        address -= buffer_state_.TOTAL_BUFFER_CAPACITY_;
    }
}

template <typename Accessor>
int ImageBuffer<Accessor>::add_image(const uint8_t *image_data, const ImageMetadata &metadata)
{
    size_t total_size = METADATA_SIZE + metadata.image_size + sizeof(crc_t);

    if (!has_enough_space(total_size))
    {
        std::cerr << "Write Error: Not enough space in buffer." << std::endl;
        return -1;
    }

    // Write metadata, but store image metadata, with checksum
    checksum_calculator_.reset();
    checksum_calculator_.update(reinterpret_cast<const uint8_t *>(&metadata), METADATA_SIZE_WO_CHECKSUM);

    print(reinterpret_cast<const uint8_t *>(&metadata), METADATA_SIZE_WO_CHECKSUM);
    metadata.checksum = checksum_calculator_.get_checksum();
    print(reinterpret_cast<const uint8_t *>(&metadata), METADATA_SIZE);

    if (write(buffer_state_.tail_, reinterpret_cast<const uint8_t *>(&metadata), METADATA_SIZE) != 0)
    {
        std::cerr << "Write failure" << std::endl;
    }

    // Simultaneously write the data and calculate the checksum
    checksum_calculator_.reset();
    checksum_calculator_.update(image_data, metadata.image_size);
    crc_t data_checksum = checksum_calculator_.get_checksum();

    if (write(buffer_state_.tail_ + METADATA_SIZE, image_data, metadata.image_size) != 0)
    {
        std::cerr << "Write failure" << std::endl;
    }

    if (write(buffer_state_.tail_ + METADATA_SIZE + metadata.image_size, reinterpret_cast<const uint8_t *>(&data_checksum), sizeof(crc_t)) != 0)
    {
        std::cerr << "Write failure" << std::endl;
    }

    verify(buffer_state_.tail_);

    buffer_state_.size_ += total_size;
    buffer_state_.tail_ += total_size;
    wrap_around(buffer_state_.tail_);

    return 0;
}

template <typename Accessor>
std::vector<uint8_t> ImageBuffer<Accessor>::read_next_image(ImageMetadata &metadata)
{
    if (is_empty())
    {
        return {};
    }
    verify(buffer_state_.head_);

    if (read(buffer_state_.head_, reinterpret_cast<uint8_t *>(&metadata), METADATA_SIZE) != 0)
    {
        std::cerr << "Read metadata failure" << std::endl;
        return {};
    }
    checksum_calculator_.reset();
    checksum_calculator_.update(reinterpret_cast<const uint8_t *>(&metadata), METADATA_SIZE_WO_CHECKSUM);
    crc_t checksum = checksum_calculator_.get_checksum();
    if (metadata.checksum != checksum)
    {
        std::cerr << "Read Error: Checksum metadata mismatch, data corrupted!" << std::endl;
        return {}; // Data corrupted
    }

    std::vector<uint8_t> image_data(metadata.image_size);
    if (read(buffer_state_.head_ + METADATA_SIZE, image_data.data(), metadata.image_size) != 0)
    {
        std::cerr << "Read data failure" << std::endl;
        return {};
    }
    checksum_calculator_.reset();
    checksum_calculator_.update(image_data.data(), metadata.image_size);
    checksum = checksum_calculator_.get_checksum();
    
    crc_t data_checksum = 0;
    if (read(buffer_state_.head_ + METADATA_SIZE + metadata.image_size, reinterpret_cast<uint8_t *>(&data_checksum), sizeof(crc_t)) != 0)
    {
        std::cerr << "Read crc failure" << std::endl;
        return {};
    }

    if (checksum != data_checksum)
    {
        std::cerr << "Read Error: Checksum data mismatch, data corrupted!" << std::endl;
        return {}; // Data corrupted
    }

    size_t total_size = METADATA_SIZE + metadata.image_size + sizeof(crc_t);
    buffer_state_.size_ -= total_size;
    buffer_state_.head_ += total_size;
    wrap_around(buffer_state_.head_);

    return image_data;
}

template <typename Accessor>
bool ImageBuffer<Accessor>::verify(const size_t ptr)
{
    ImageMetadata metadata;
    if (is_empty())
    {
        return false;
    }

    if (read(ptr, reinterpret_cast<uint8_t *>(&metadata), METADATA_SIZE) != 0)
    {
        std::cerr << "Verify metadata failure" << std::endl;
        return false;
    }
    checksum_calculator_.reset();
    checksum_calculator_.update(reinterpret_cast<const uint8_t *>(&metadata), METADATA_SIZE_WO_CHECKSUM);
    crc_t checksum = checksum_calculator_.get_checksum();
    if (metadata.checksum != checksum)
    {
        std::cerr << "Verify Error: Checksum metadata mismatch, data corrupted!" << std::endl;
        return false; // Data corrupted
    }

    std::vector<uint8_t> image_data(metadata.image_size);
    if (read(ptr + METADATA_SIZE, image_data.data(), metadata.image_size) != 0)
    {
        std::cerr << "Verify Read data failure" << std::endl;
        return false;
    }
    checksum_calculator_.reset();
    checksum_calculator_.update(image_data.data(), metadata.image_size);
    checksum = checksum_calculator_.get_checksum();
    
    crc_t data_checksum = 0;
    if (read(ptr + METADATA_SIZE + metadata.image_size, reinterpret_cast<uint8_t *>(&data_checksum), sizeof(crc_t)) != 0)
    {
        std::cerr << "Verify Read crc failure" << std::endl;
        return false;
    }

    if (checksum != data_checksum)
    {
        std::cerr << "Verify Error: Checksum data mismatch, data corrupted!" << std::endl;
        return false; // Data corrupted
    }

    return true;
}

template <typename Accessor>
bool ImageBuffer<Accessor>::drop_image()
{   

    ImageMetadata metadata;
    if (is_empty())
    {
        return {};
    }
    verify(buffer_state_.head_);

    if (read(buffer_state_.head_, reinterpret_cast<uint8_t *>(&metadata), METADATA_SIZE) != 0)
    {
        std::cerr << "Drop Read metadata failure" << std::endl;
        return {};
    }
    checksum_calculator_.reset();
    checksum_calculator_.update(reinterpret_cast<const uint8_t *>(&metadata), METADATA_SIZE_WO_CHECKSUM);
    crc_t checksum = checksum_calculator_.get_checksum();
    if (metadata.checksum != checksum)
    {
        std::cerr << "Drop Error: Checksum metadata mismatch, data corrupted!" << std::endl;
        return {}; // Data corrupted
    }

    size_t total_size = METADATA_SIZE + metadata.image_size + sizeof(crc_t);
    buffer_state_.size_ -= total_size;
    buffer_state_.head_ += total_size;
    wrap_around(buffer_state_.head_);

    return true;
}

#endif // IMAGE_BUFFER_H