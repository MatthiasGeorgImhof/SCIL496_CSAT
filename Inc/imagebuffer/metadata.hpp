#ifndef __METADATA_HPP__
#define __METADATA_HPP__

#include <cstdint>
#include <cstddef>

typedef uint32_t crc_t;

// -----------------------------------------------------------------------------
// Semantic source of the image (producer identity)
// -----------------------------------------------------------------------------
enum class SOURCE : uint8_t
{
    CAMERA_1 = 0,
    CAMERA_2 = 1,
    CAMERA_3 = 2,
    THERMAL  = 3,

    // Future sources can be added here
};

// -----------------------------------------------------------------------------
// Semantic Image Metadata (versioned, packed, CRC-protected)
// This is NOT the storage header. This is what producers/consumers care about.
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct ImageMetadata
{
    uint16_t version;         // metadata version
    uint16_t metadata_size;   // sizeof(ImageMetadata) at creation time

    uint32_t timestamp;       // seconds or ms since epoch
    uint32_t image_size;      // payload size in bytes

    float    latitude;        // degrees
    float    longitude;       // degrees

    SOURCE   source;          // 1 byte (enum class SOURCE : uint8_t)

    uint8_t  reserved[8];     // reserved for future expansion

    crc_t    meta_crc;        // CRC over all previous fields
};
#pragma pack(pop)

// -----------------------------------------------------------------------------
// Explicit size check (future-proof, readable)
// -----------------------------------------------------------------------------
static_assert(
    sizeof(ImageMetadata) ==
        sizeof(uint16_t) +   // version
        sizeof(uint16_t) +   // metadata_size
        sizeof(uint32_t) +   // timestamp
        sizeof(uint32_t) +   // image_size
        sizeof(float)   +    // latitude
        sizeof(float)   +    // longitude
        sizeof(SOURCE)  +    // source
        sizeof(uint8_t) * 8 +// reserved
        sizeof(crc_t),       // meta_crc
    "Unexpected ImageMetadata size"
);

// Convenience constants
constexpr size_t METADATA_SIZE = sizeof(ImageMetadata);
constexpr size_t METADATA_SIZE_WO_CRC = offsetof(ImageMetadata, meta_crc);

#endif // __METADATA_HPP__
