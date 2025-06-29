#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "ImageBuffer.hpp"
#include "CachedImageBuffer.hpp"
#include "UInt8Stream.hpp"

#include "imagebuffer/accessor.hpp"
#include "imagebuffer/DirectMemoryAccessor.hpp"
#include "imagebuffer/LinuxMockI2CFlashAccessor.hpp"
#include "mock_hal.h"
#include "Checksum.hpp"

#include <vector>
#include <iostream>

// Mock Accessor for testing
struct MockAccessor
{
    size_t start;
    size_t size;
    std::vector<uint8_t> data;

    MockAccessor(size_t start, size_t size) : start(start), size(size), data(size, 0) {}

    std::vector<uint8_t> &getFlashMemory() { return data; }
    size_t getFlashMemorySize() const { return size; };
    size_t getFlashStartAddress() const { return start; };
    size_t getAlignment() const { return 1; }

    AccessorError write(size_t address, const uint8_t *buffer, size_t num_bytes)
    {
        if (address + num_bytes - start > size)
        {
            std::cerr << "MockAccessor::write: out of bounds write. address=" << address << ", num_bytes=" << num_bytes << ", size=" << size << std::endl;
            return AccessorError::WRITE_ERROR; // Simulate write error
        }
        std::memcpy(data.data() + address - start, buffer, num_bytes);
        return AccessorError::NO_ERROR; // Simulate successful write
    }

    AccessorError read(size_t address, uint8_t *buffer, size_t num_bytes)
    {
        if (address + num_bytes - start > size)
        {
            std::cerr << "MockAccessor::read: out of bounds read. address=" << address << ", num_bytes=" << num_bytes << ", size=" << size << std::endl;
            return AccessorError::READ_ERROR; // Simulate read error
        }
        std::memcpy(buffer, data.data() + address - start, num_bytes);
        return AccessorError::NO_ERROR; // Simulate successful read
    }

    AccessorError erase(uint32_t /*address*/) {
        // Mock implementation - just return success
        return AccessorError::NO_ERROR;
    }

    void reset()
    {
        std::fill(data.begin(), data.end(), 0);
    }
};

static_assert(Accessor<MockAccessor>, "MockAccessor does not satisfy the Accessor concept!");

TEST_CASE("UInt8Stream with ImageBuffer") {
    MockAccessor accessor(0, 2048); // Larger buffer for this test
    ImageBuffer<MockAccessor> image_buffer(accessor);
    UInt8Stream<ImageBuffer<MockAccessor>> stream(image_buffer);

    ImageMetadata metadata;
    metadata.timestamp = 0x12345678;
    metadata.image_size = 256;
    metadata.latitude = 37.7749;
    metadata.longitude = -122.4194;
    metadata.camera_index = 0xAB;

    std::vector<uint8_t> image_data(metadata.image_size);
    for (size_t i = 0; i < metadata.image_size; ++i) {
        image_data[i] = static_cast<uint8_t>(i % 256);
    }

    REQUIRE(image_buffer.add_image(metadata) == ImageBufferError::NO_ERROR);
    REQUIRE(image_buffer.add_data_chunk(image_data.data(), metadata.image_size) == ImageBufferError::NO_ERROR);
    REQUIRE(image_buffer.push_image() == ImageBufferError::NO_ERROR);

    SUBCASE("Test initialize") {
        size_t size = 2*sizeof(ImageMetadata);
        uint8_t meta[2*sizeof(ImageMetadata)];
        stream.initialize(meta, size);
        REQUIRE(size == sizeof(ImageMetadata));
        ImageMetadata *metadata_ = reinterpret_cast<ImageMetadata*>(meta);
        CHECK(metadata_->timestamp == metadata.timestamp);
        CHECK(metadata_->camera_index == metadata.camera_index);
    }

    SUBCASE("Test size") {
        REQUIRE(stream.size() == metadata.image_size + sizeof(ImageMetadata));
    }

    SUBCASE("Test name") {
        std::array<char, NAME_LENGTH> expected_name = formatValues(metadata.timestamp, metadata.camera_index);
        
        CHECK(memcmp(stream.name().data(), expected_name.data(), NAME_LENGTH) == 0);
    }

   SUBCASE("Test getChunk") {
        size_t count = 0;
        size_t size = 2*sizeof(ImageMetadata);
        uint8_t chunk[2*sizeof(ImageMetadata)];
        stream.initialize(chunk, size);
        size = 10; // Request a chunk of 10 bytes

        stream.getChunk(chunk, size);
        REQUIRE(size <= 10); // The actual size should be no more than 10
        REQUIRE(size != 0);

        // Verify the chunk data (first 'size' bytes)
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(chunk[i] == image_data[i]);
        }
        count += size;

        // Subsequent calls to getChunk
        stream.getChunk(chunk, size);
        REQUIRE(size <= 10); // The actual size should be no more than 10
        REQUIRE(size != 0);
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(chunk[i] == image_data[i+count]);
        }
        count += size;
    }

    SUBCASE("Test is_empty after adding and then popping image") {
        constexpr size_t chunk_size = 10;
        REQUIRE(stream.is_empty() == false);
        size_t size = 2*sizeof(ImageMetadata);
        uint8_t chunk[2*sizeof(ImageMetadata)];
        stream.initialize(chunk, size);
        size_t stream_size = stream.size() - sizeof(ImageMetadata);
        while(stream_size > 0){
            size = std::min(chunk_size, stream_size);
            stream.getChunk(chunk, size);
            stream_size -= size;
        }
        CHECK(size==6);
        REQUIRE(stream.is_empty() == false);

        size = std::min(chunk_size, stream_size);
        stream.getChunk(chunk, size);
        CHECK(size==0);
        REQUIRE(stream.is_empty() == true);

    }

}

TEST_CASE("UInt8Stream with CachedImageBuffer") {
    MockAccessor accessor(0, 2048); // Larger buffer for this test
    CachedImageBuffer<MockAccessor> image_buffer(accessor);
    UInt8Stream<CachedImageBuffer<MockAccessor>> stream(image_buffer);

    ImageMetadata metadata;
    metadata.timestamp = 0x12345678;
    metadata.image_size = 256;
    metadata.latitude = 37.7749;
    metadata.longitude = -122.4194;
    metadata.camera_index = 0xAB;

    std::vector<uint8_t> image_data(metadata.image_size);
    for (size_t i = 0; i < metadata.image_size; ++i) {
        image_data[i] = static_cast<uint8_t>(i % 256);
    }

    REQUIRE(image_buffer.add_image(metadata) == CachedImageBufferError::NO_ERROR);
    REQUIRE(image_buffer.add_data_chunk(image_data.data(), metadata.image_size) == CachedImageBufferError::NO_ERROR);
    REQUIRE(image_buffer.push_image() == CachedImageBufferError::NO_ERROR);

    SUBCASE("Test initialize") {
        size_t size = 2*sizeof(ImageMetadata);
        uint8_t meta[2*sizeof(ImageMetadata)];
        stream.initialize(meta, size);
        REQUIRE(size == sizeof(ImageMetadata));
        ImageMetadata *metadata_ = reinterpret_cast<ImageMetadata*>(meta);
        CHECK(metadata_->timestamp == metadata.timestamp);
        CHECK(metadata_->camera_index == metadata.camera_index);
    }

    SUBCASE("Test size") {
        REQUIRE(stream.size() == metadata.image_size + sizeof(ImageMetadata));
    }

    SUBCASE("Test name") {
        std::array<char, NAME_LENGTH> expected_name = formatValues(metadata.timestamp, metadata.camera_index);
        
        CHECK(memcmp(stream.name().data(), expected_name.data(), NAME_LENGTH) == 0);
    }

   SUBCASE("Test getChunk") {
        size_t count = 0;
        size_t size = 2*sizeof(ImageMetadata);
        uint8_t chunk[2*sizeof(ImageMetadata)];
        stream.initialize(chunk, size);
        size = 10; // Request a chunk of 10 bytes

        stream.getChunk(chunk, size);
        REQUIRE(size <= 10); // The actual size should be no more than 10
        REQUIRE(size != 0);

        // Verify the chunk data (first 'size' bytes)
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(chunk[i] == image_data[i]);
        }
        count += size;

        // Subsequent calls to getChunk
        stream.getChunk(chunk, size);
        REQUIRE(size <= 10); // The actual size should be no more than 10
        REQUIRE(size != 0);
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(chunk[i] == image_data[i+count]);
        }
        count += size;
    }

    SUBCASE("Test is_empty after adding and then popping image") {
        constexpr size_t chunk_size = 10;
        REQUIRE(stream.is_empty() == false);
        size_t size = 2*sizeof(ImageMetadata);
        uint8_t chunk[2*sizeof(ImageMetadata)];
        stream.initialize(chunk, size);
        size_t stream_size = stream.size() - sizeof(ImageMetadata);
        while(stream_size > 0){
            size = std::min(chunk_size, stream_size);
            stream.getChunk(chunk, size);
            stream_size -= size;
        }
        CHECK(size==6);
        REQUIRE(stream.is_empty() == false);

        size = std::min(chunk_size, stream_size);
        stream.getChunk(chunk, size);
        CHECK(size==0);
        REQUIRE(stream.is_empty() == true);

    }

}