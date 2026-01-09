#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "ImageBuffer.hpp"
#include "imagebuffer/DirectMemoryAccessor.hpp"
#include "imagebuffer/BufferedAccessor.hpp"
#include "imagebuffer/storageheader.hpp"
#include "imagebuffer/image.hpp"
#include "Checksum.hpp"

#include <vector>
#include <cstring>

// Convenience alias
template <typename Accessor>
using TestBuffer = ImageBuffer<Accessor, NoAlignmentPolicy>;

static ImageMetadata make_meta(size_t image_size, uint32_t ts = 1234)
{
    ImageMetadata m{};
    m.timestamp = ts;
    m.image_size = static_cast<uint32_t>(image_size);
    m.latitude = 1.0f;
    m.longitude = 2.0f;
    m.source = SOURCE::CAMERA_1;
    return m;
}

static void erase_flash(DirectMemoryAccessor &acc, size_t flash_start, size_t flash_size)
{
    std::vector<uint8_t> blank(flash_size, 0xFF);
    acc.write(flash_start, blank.data(), flash_size);
}

TEST_CASE("initialize_from_flash: empty flash")
{
    constexpr size_t flash_start = 0x4000;
    constexpr size_t flash_size = 4096;

    DirectMemoryAccessor acc(flash_start, flash_size);
    erase_flash(acc, flash_start, flash_size);
    TestBuffer<DirectMemoryAccessor> buf(acc);

    // Flash is erased by default
    ImageBufferError err = buf.initialize_from_flash();
    REQUIRE(err == ImageBufferError::NO_ERROR);

    REQUIRE(buf.is_empty());
    REQUIRE(buf.count() == 0);
    REQUIRE(buf.size() == 0);
    REQUIRE(buf.get_head() == 0);
    REQUIRE(buf.get_tail() == 0);
}

TEST_CASE("initialize_from_flash: single entry")
{
    constexpr size_t flash_start = 0x4000;
    constexpr size_t flash_size = 4096;

    DirectMemoryAccessor acc(flash_start, flash_size);
    erase_flash(acc, flash_start, flash_size);

    // First buffer writes data
    {
        TestBuffer<DirectMemoryAccessor> buf(acc);
        ImageMetadata meta = make_meta(64, 1000);

        REQUIRE(buf.add_image(meta) == ImageBufferError::NO_ERROR);

        std::vector<uint8_t> payload(meta.image_size);
        for (size_t i = 0; i < payload.size(); i++)
            payload[i] = uint8_t(i);

        REQUIRE(buf.add_data_chunk(payload.data(), payload.size()) == ImageBufferError::NO_ERROR);
        REQUIRE(buf.push_image() == ImageBufferError::NO_ERROR);
    }

    // New buffer reconstructs
    {
        TestBuffer<DirectMemoryAccessor> buf(acc);

        ImageBufferError err = buf.initialize_from_flash();
        REQUIRE(err == ImageBufferError::NO_ERROR);

        REQUIRE(buf.count() == 1);

        ImageMetadata meta{};
        REQUIRE(buf.get_image(meta) == ImageBufferError::NO_ERROR);
        REQUIRE(meta.timestamp == 1000);
        REQUIRE(meta.image_size == 64);

        std::vector<uint8_t> out(meta.image_size);
        size_t chunk = meta.image_size;
        REQUIRE(buf.get_data_chunk(out.data(), chunk) == ImageBufferError::NO_ERROR);
        REQUIRE(chunk == meta.image_size);

        for (size_t i = 0; i < out.size(); i++)
            REQUIRE(out[i] == uint8_t(i));

        REQUIRE(buf.pop_image() == ImageBufferError::NO_ERROR);
        REQUIRE(buf.is_empty());
    }
}

TEST_CASE("initialize_from_flash: multiple entries")
{
    constexpr size_t flash_start = 0x4000;
    constexpr size_t flash_size = 16384;

    DirectMemoryAccessor acc(flash_start, flash_size);
    erase_flash(acc, flash_start, flash_size);

    const size_t N = 5;
    const size_t IMG_SIZE = 128;

    // Write N images
    {
        TestBuffer<DirectMemoryAccessor> buf(acc);

        for (size_t i = 0; i < N; i++)
        {
            ImageMetadata meta = make_meta(IMG_SIZE, static_cast<uint32_t>(2000 + i));
            REQUIRE(buf.add_image(meta) == ImageBufferError::NO_ERROR);

            std::vector<uint8_t> payload(IMG_SIZE);
            for (size_t j = 0; j < IMG_SIZE; j++)
                payload[j] = uint8_t(j + i);

            REQUIRE(buf.add_data_chunk(payload.data(), payload.size()) == ImageBufferError::NO_ERROR);
            REQUIRE(buf.push_image() == ImageBufferError::NO_ERROR);
        }
    }

    // Reconstruct
    {
        TestBuffer<DirectMemoryAccessor> buf(acc);
        ImageBufferError err = buf.initialize_from_flash();
        REQUIRE(err == ImageBufferError::NO_ERROR);
        REQUIRE(buf.count() == N);

        for (size_t i = 0; i < N; i++)
        {
            ImageMetadata meta{};
            REQUIRE(buf.get_image(meta) == ImageBufferError::NO_ERROR);
            REQUIRE(meta.timestamp == 2000 + i);
            REQUIRE(meta.image_size == IMG_SIZE);

            std::vector<uint8_t> out(IMG_SIZE);
            size_t chunk = IMG_SIZE;
            REQUIRE(buf.get_data_chunk(out.data(), chunk) == ImageBufferError::NO_ERROR);

            for (size_t j = 0; j < IMG_SIZE; j++)
                REQUIRE(out[j] == uint8_t(j + i));

            REQUIRE(buf.pop_image() == ImageBufferError::NO_ERROR);
        }

        REQUIRE(buf.is_empty());
    }
}

TEST_CASE("initialize_from_flash: corrupted header truncates tail")
{
    constexpr size_t flash_start = 0x4000;
    constexpr size_t flash_size = 8192;

    DirectMemoryAccessor acc(flash_start, flash_size);
    erase_flash(acc, flash_start, flash_size);

    // Write 3 images
    {
        TestBuffer<DirectMemoryAccessor> buf(acc);

        for (size_t i = 0; i < 3; i++)
        {
            ImageMetadata meta = make_meta(32, static_cast<uint32_t>(3000 + i));
            REQUIRE(buf.add_image(meta) == ImageBufferError::NO_ERROR);

            std::vector<uint8_t> payload(meta.image_size, uint8_t(i));
            REQUIRE(buf.add_data_chunk(payload.data(), payload.size()) == ImageBufferError::NO_ERROR);
            REQUIRE(buf.push_image() == ImageBufferError::NO_ERROR);
        }
    }

    // Corrupt the second header's magic
    {
        StorageHeader hdr{};
        size_t offset = 0;

        // Read first header to compute its size
        {
            TestBuffer<DirectMemoryAccessor> tmp(acc);
            acc.read(flash_start + 0, reinterpret_cast<uint8_t *>(&hdr), sizeof(StorageHeader));

            offset += sizeof(StorageHeader) + hdr.total_size;
        }

        // Overwrite magic of second header
        uint32_t bad_magic = 0xDEADBEEF;
        acc.write(flash_start + offset, reinterpret_cast<uint8_t *>(&bad_magic), sizeof(bad_magic));
    }

    // Reconstruct
    {
        TestBuffer<DirectMemoryAccessor> buf(acc);
        ImageBufferError err = buf.initialize_from_flash();

        REQUIRE(err == ImageBufferError::CHECKSUM_ERROR);
        REQUIRE(buf.count() == 1); // only first entry survives

        ImageMetadata meta{};
        REQUIRE(buf.get_image(meta) == ImageBufferError::NO_ERROR);
        REQUIRE(meta.timestamp == 3000);

        // Drain payload (we don't care about its contents here)
        std::vector<uint8_t> dummy(meta.image_size);
        size_t chunk = meta.image_size;
        REQUIRE(buf.get_data_chunk(dummy.data(), chunk) == ImageBufferError::NO_ERROR);
        REQUIRE(chunk == meta.image_size);

        // Now CRC comparison is valid
        REQUIRE(buf.pop_image() == ImageBufferError::NO_ERROR);
        REQUIRE(buf.is_empty());
    }
}

TEST_CASE("initialize_from_flash: sequence_id continues correctly")
{
    constexpr size_t flash_start = 0x4000;
    constexpr size_t flash_size = 8192;

    DirectMemoryAccessor acc(flash_start, flash_size);
    erase_flash(acc, flash_start, flash_size);

    // Write 2 images
    {
        TestBuffer<DirectMemoryAccessor> buf(acc);

        for (size_t i = 0; i < 2; i++)
        {
            ImageMetadata meta = make_meta(32, static_cast<uint32_t>(4000 + i));
            REQUIRE(buf.add_image(meta) == ImageBufferError::NO_ERROR);

            std::vector<uint8_t> payload(meta.image_size, uint8_t(i));
            REQUIRE(buf.add_data_chunk(payload.data(), payload.size()) == ImageBufferError::NO_ERROR);
            REQUIRE(buf.push_image() == ImageBufferError::NO_ERROR);
        }
    }

    // Reconstruct
    TestBuffer<DirectMemoryAccessor> buf(acc);
    REQUIRE(buf.initialize_from_flash() == ImageBufferError::NO_ERROR);

    // Add a new image — sequence_id should continue from last
    ImageMetadata meta = make_meta(16, 9999);
    REQUIRE(buf.add_image(meta) == ImageBufferError::NO_ERROR);

    // Read header to check sequence_id
    StorageHeader hdr{};
    acc.read(flash_start + buf.get_tail(), reinterpret_cast<uint8_t *>(&hdr), sizeof(StorageHeader));

    REQUIRE(hdr.sequence_id == 2); // previous were 0 and 1
}
