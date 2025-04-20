#ifndef IMAGE_BUFFER_H
#define IMAGE_BUFFER_H

#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>

// Configuration
const uint32_t METADATA_SIZE = 32; // Define it here!

// Data Structures (same as before)
struct ImageMetadata
{
    uint32_t timestamp;
    uint8_t camera_index;
    float latitude;
    float longitude;
    uint32_t image_size;
    uint32_t checksum;
};

static_assert(sizeof(ImageMetadata) <= METADATA_SIZE, "ImageMetadata size mismatch!");

template <typename Accessor>
class ImageBuffer
{
public:
    ImageBuffer(Accessor &access, uint32_t flash_start, uint32_t total_size)
        : head(flash_start), tail(flash_start), access_(access), FLASH_START_ADDRESS(flash_start), TOTAL_BUFFER_SIZE(total_size) {}

    int add_image(const uint8_t *image_data, size_t image_size, const ImageMetadata &metadata);
    std::vector<uint8_t> read_next_image(ImageMetadata &metadata);
    bool is_empty() const { return head == tail; }

    // Getter methods for test purposes
    uint32_t getHead() const { return head; }
    uint32_t getTail() const { return tail; }

private:
    uint32_t head;
    uint32_t tail;

    Accessor &access_; // Reference
    const uint32_t FLASH_START_ADDRESS;
    const uint32_t TOTAL_BUFFER_SIZE;

    bool has_enough_space(size_t data_size) const;
    void wrap_around();
    uint32_t calculate_initial_checksum();
};

template <typename Accessor>
bool ImageBuffer<Accessor>::has_enough_space(size_t data_size) const
{
    uint32_t available_space = (tail >= head) ? (FLASH_START_ADDRESS + TOTAL_BUFFER_SIZE - tail + head - FLASH_START_ADDRESS)
                                              : (head - tail);
    return (data_size <= available_space);
}

template <typename Accessor>
void ImageBuffer<Accessor>::wrap_around()
{
    if (tail >= FLASH_START_ADDRESS + TOTAL_BUFFER_SIZE)
    {
        tail = FLASH_START_ADDRESS;
    }
}

// Very simple example of an XOR checksum
template <typename Accessor>
uint32_t ImageBuffer<Accessor>::calculate_initial_checksum()
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
    uint32_t checksum = calculate_initial_checksum();

    // Write metadata, but store image metadata, with checksum
    ImageMetadata write_metadata = metadata; // Copy metadata to modify checksum without changing original
    write_metadata.image_size = image_size;

    // Simultaneously write the data and calculate the checksum
    for (size_t i = 0; i < image_size; ++i)
    {
        checksum ^= image_data[i];
    }

    write_metadata.checksum = checksum;

    if (access_.write(tail, reinterpret_cast<const uint8_t *>(&write_metadata), METADATA_SIZE) != 0)
    {
        std::cerr << "Write failure" << std::endl;
    }

    if (access_.write(tail + METADATA_SIZE, image_data, image_size) != 0)
    {
        std::cerr << "Write failure" << std::endl;
    }

    tail += total_size;
    wrap_around();

    return 0;
}

template <typename Accessor>
std::vector<uint8_t> ImageBuffer<Accessor>::read_next_image(ImageMetadata &metadata)
{
    if (is_empty())
    {
        return {};
    }

    if (access_.read(head, reinterpret_cast<uint8_t *>(&metadata), METADATA_SIZE) != 0)
    {
        std::cerr << "Read failure" << std::endl;
        return {};
    }

    std::vector<uint8_t> image_data(metadata.image_size);

    if (access_.read(head + METADATA_SIZE, image_data.data(), metadata.image_size) != 0)
    {
        std::cerr << "Read failure" << std::endl;
        return {};
    }

    uint32_t checksum = calculate_initial_checksum();
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

    head += METADATA_SIZE + metadata.image_size;
    wrap_around();

    return image_data;
}

#endif // IMAGE_BUFFER_H