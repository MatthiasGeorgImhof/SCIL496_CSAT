#ifndef IMAGE_BUFFER_H
#define IMAGE_BUFFER_H

#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>

// Data Structures (same as before)
struct ImageMetadata
{
    uint32_t timestamp;
    uint8_t camera_index;
    float latitude;
    float longitude;
    size_t image_size;
    uint32_t checksum;
};
constexpr size_t METADATA_SIZE = sizeof(ImageMetadata);

template <typename Accessor>
class ImageBuffer
{
public:
    ImageBuffer(Accessor &access, size_t flash_start, size_t total_size)
        : head_(0), tail_(0), access_(access), FLASH_START_ADDRESS_(flash_start), TOTAL_BUFFER_CAPACITY_(total_size), size_(0) {}

    int add_image(const uint8_t *image_data, size_t image_size, const ImageMetadata &metadata);
    std::vector<uint8_t> read_next_image(ImageMetadata &metadata);
    bool drop_image();

    bool is_empty() const { return size_ == 0; }
    size_t size() const { return size_; };
    size_t available() const { return TOTAL_BUFFER_CAPACITY_ - size_; };
    size_t capacity() const { return TOTAL_BUFFER_CAPACITY_; };

    // Getter methods for test purposes
    size_t get_head() const { return head_; }
    size_t get_tail() const { return tail_; }

private:
    bool has_enough_space(size_t data_size) const { return (available() >= data_size); };
    void wrap_around(size_t &address);
    size_t calculate_initial_checksum();
    int32_t write(size_t address, const uint8_t *data, size_t size);
    int32_t read(size_t address, uint8_t *data, size_t size);

private:
    size_t head_;
    size_t tail_;

    Accessor &access_; // Reference
    const size_t FLASH_START_ADDRESS_;
    const size_t TOTAL_BUFFER_CAPACITY_;
    size_t size_;
};

template <typename Accessor>
int32_t ImageBuffer<Accessor>::write(size_t address, const uint8_t *data, size_t size)
{
    if (address + size <= TOTAL_BUFFER_CAPACITY_)
    {
        int32_t status = access_.write(address + FLASH_START_ADDRESS_, data, size);
        return status;
    }

    size_t first_part = TOTAL_BUFFER_CAPACITY_ - address;
    size_t second_part = size - first_part;
    int32_t status = access_.write(address + FLASH_START_ADDRESS_, data, first_part);
    if (status != 0)
    {
        std::cerr << "Write failure" << std::endl;
        return -1;
    }
    status = access_.write(FLASH_START_ADDRESS_, data + first_part, second_part);
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
    if (address + size <= TOTAL_BUFFER_CAPACITY_)
    {
        int32_t status = access_.read(address + FLASH_START_ADDRESS_, data, size);
        return status;
    }

    size_t first_part = TOTAL_BUFFER_CAPACITY_ - address;
    size_t second_part = size - first_part;
    int32_t status = access_.read(address + FLASH_START_ADDRESS_, data, first_part);
    if (status != 0)
    {
        std::cerr << "Read failure" << std::endl;
        return -1;
    }
    status = access_.read(FLASH_START_ADDRESS_, data + first_part, second_part);
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
    if (address >= TOTAL_BUFFER_CAPACITY_)
    {
        address -= TOTAL_BUFFER_CAPACITY_;
    }
}

// Very simple example of an XOR checksum
template <typename Accessor>
size_t ImageBuffer<Accessor>::calculate_initial_checksum()
{
    return 0; // Start with an initial checksum of 0
}

template <typename Accessor>
int ImageBuffer<Accessor>::add_image(const uint8_t *image_data, size_t image_size, const ImageMetadata &metadata)
{
    size_t total_size = METADATA_SIZE + image_size;

    if (!has_enough_space(total_size))
    {
        std::cerr << "Error: Not enough space in buffer." << std::endl;
        return -1;
    }

    // Set checksum
    size_t checksum = calculate_initial_checksum();

    // Write metadata, but store image metadata, with checksum
    ImageMetadata write_metadata = metadata; // Copy metadata to modify checksum without changing original
    write_metadata.image_size = image_size;

    // Simultaneously write the data and calculate the checksum
    for (size_t i = 0; i < image_size; ++i)
    {
        checksum ^= image_data[i];
    }

    write_metadata.checksum = checksum;

    if (write(tail_, reinterpret_cast<const uint8_t *>(&write_metadata), METADATA_SIZE) != 0)
    {
        std::cerr << "Write failure" << std::endl;
    }

    if (write(tail_ + METADATA_SIZE, image_data, image_size) != 0)
    {
        std::cerr << "Write failure" << std::endl;
    }

    size_ += total_size;
    tail_ += total_size;
    wrap_around(tail_);

    return 0;
}

template <typename Accessor>
std::vector<uint8_t> ImageBuffer<Accessor>::read_next_image(ImageMetadata &metadata)
{
    if (is_empty())
    {
        return {};
    }

    if (read(head_, reinterpret_cast<uint8_t *>(&metadata), METADATA_SIZE) != 0)
    {
        std::cerr << "Read failure" << std::endl;
        return {};
    }

    std::vector<uint8_t> image_data(metadata.image_size);

    if (read(head_ + METADATA_SIZE, image_data.data(), metadata.image_size) != 0)
    {
        std::cerr << "Read failure" << std::endl;
        return {};
    }

    size_t checksum = calculate_initial_checksum();
    for (size_t i = 0; i < image_data.size(); ++i)
    {
        checksum ^= image_data[i];
    }

    // Verify checksum
    if (metadata.checksum != checksum)
    {
        std::cerr << "Error: Checksum mismatch, data corrupted!" << std::endl;
        return {}; // Data corrupted
    }

    size_t total_size = METADATA_SIZE + metadata.image_size;
    size_ -= total_size;
    head_ += total_size;
    wrap_around(head_);

    return image_data;
}

template <typename Accessor>
bool ImageBuffer<Accessor>::drop_image()
{
    if (is_empty())
    {
        return false;
    }

    ImageMetadata metadata;

    if (read(head_, reinterpret_cast<uint8_t *>(&metadata), METADATA_SIZE) != 0)
    {
        std::cerr << "Read failure" << std::endl;
        return false;
    }

    size_t total_size = METADATA_SIZE + metadata.image_size;
    size_ -= total_size;
    head_ += total_size;
    wrap_around(head_);

    return true;
}

#endif // IMAGE_BUFFER_H