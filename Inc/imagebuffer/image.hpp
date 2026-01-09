#ifndef __IMAGE_HPP__
#define __IMAGE_HPP__

#include <cstdint>
#include <cstddef>

typedef uint32_t crc_t;
typedef uint32_t image_magic_t;
constexpr static image_magic_t IMAGE_MAGIC = ('I' << 24) | ('M' << 16) | ('T' << 8) | 'A';

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
#pragma pack(push, 1)
struct ImageMetadata
{
    const image_magic_t magic;   // 4 bytes
    uint32_t timestamp;          // 4 bytes
    uint32_t image_size;         // 4 bytes
    float latitude;              // 4 bytes
    float longitude;             // 4 bytes
    uint8_t camera_index;        // 1 byte
    crc_t checksum;              // 4 bytes

    ImageMetadata() : magic(IMAGE_MAGIC) {}

    ImageMetadata(const ImageMetadata& other)
        : magic(IMAGE_MAGIC)
        , timestamp(other.timestamp)
        , image_size(other.image_size)
        , latitude(other.latitude)
        , longitude(other.longitude)
        , camera_index(other.camera_index)
        , checksum(other.checksum)
    {}

    ImageMetadata& operator=(const ImageMetadata& other)
    {
        if (this != &other)
        {
            timestamp = other.timestamp;
            image_size = other.image_size;
            latitude = other.latitude;
            longitude = other.longitude;
            camera_index = other.camera_index;
            checksum = other.checksum;
        }
        return *this;
    }
};
#pragma pack(pop)

constexpr size_t METADATA_SIZE_WO_CHECKSUM = offsetof(ImageMetadata, checksum);
constexpr size_t METADATA_SIZE = sizeof(ImageMetadata);

#endif // __IMAGE_HPP__
