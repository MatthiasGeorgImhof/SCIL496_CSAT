#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "ImageBuffer.hpp"

#include "imagebuffer/DirectMemoryAccess.hpp"
#include "imagebuffer/LinuxMockHALFlashAccess.hpp"
#include "mock_hal.h"
#include "Checksum.hpp"

#include <vector>
#include <iostream>

// Mock Accessor for testing
struct MockAccessor {
    std::vector<uint8_t> data;
    size_t size;

    MockAccessor(size_t size) : data(size, 0), size(size) {}

    int32_t write(size_t address, const uint8_t* buffer, size_t num_bytes) {
        if (address + num_bytes > size) {
            std::cerr << "MockAccessor::write: out of bounds write. address=" << address << ", num_bytes=" << num_bytes << ", size=" << size << std::endl;
            return -1; // Simulate write error
        }
        std::memcpy(data.data() + address, buffer, num_bytes);
        return 0; // Simulate successful write
    }

    int32_t read(size_t address, uint8_t* buffer, size_t num_bytes) {
        if (address + num_bytes > size) {
             std::cerr << "MockAccessor::read: out of bounds read. address=" << address << ", num_bytes=" << num_bytes << ", size=" << size << std::endl;
            return -1; // Simulate read error
        }
        std::memcpy(buffer, data.data() + address, num_bytes);
        return 0; // Simulate successful read
    }

    void reset() {
        std::fill(data.begin(), data.end(), 0);
    }
};

TEST_CASE("ImageBuffer Initialization") {
    MockAccessor accessor(1024);
    ImageBuffer<MockAccessor> buffer(accessor, 0, 1024);

    REQUIRE(buffer.is_empty() == true);
    REQUIRE(buffer.size() == 0);
    REQUIRE(buffer.count() == 0);
    REQUIRE(buffer.available() == 1024);
    REQUIRE(buffer.capacity() == 1024);
    REQUIRE(buffer.get_head() == 0);
    REQUIRE(buffer.get_tail() == 0);
}

TEST_CASE("ImageBuffer Add and Get Image") {
    MockAccessor accessor(2048);
    ImageBuffer<MockAccessor> buffer(accessor, 1000, 1024);

    ImageMetadata metadata;
    metadata.timestamp = 12345;
    metadata.image_size = 256;
    metadata.latitude = 37.7749;
    metadata.longitude = -122.4194;
    metadata.camera_index = 1;


    std::vector<uint8_t> image_data(metadata.image_size);
    for (size_t i = 0; i < metadata.image_size; ++i) {
        image_data[i] = static_cast<uint8_t>(i % 256);
    }

    SUBCASE("Add Image Success") {
        REQUIRE(buffer.add_image(metadata) == ImageBufferError::NO_ERROR);
        for(size_t i = 0; i < image_data.size(); i++) {
            REQUIRE(buffer.add_data_chunk(&image_data[i],1) == ImageBufferError::NO_ERROR);
        }
        REQUIRE(buffer.push_image() == ImageBufferError::NO_ERROR);


        REQUIRE(buffer.is_empty() == false);
        REQUIRE(buffer.size() > 0);
        REQUIRE(buffer.count() == 1);
        REQUIRE(buffer.available() < 1024);
    }

    SUBCASE("Get Image Success") {
        REQUIRE(buffer.add_image(metadata) == ImageBufferError::NO_ERROR);
        for(size_t i = 0; i < image_data.size(); i++) {
            REQUIRE(buffer.add_data_chunk(&image_data[i],1) == ImageBufferError::NO_ERROR);
        }
        REQUIRE(buffer.push_image() == ImageBufferError::NO_ERROR);

        ImageMetadata retrieved_metadata;
        REQUIRE(buffer.get_image(retrieved_metadata) == ImageBufferError::NO_ERROR);

        REQUIRE(retrieved_metadata.magic == IMAGE_MAGIC);
        REQUIRE(retrieved_metadata.timestamp == metadata.timestamp);
        REQUIRE(retrieved_metadata.image_size == metadata.image_size);
        REQUIRE(retrieved_metadata.latitude == metadata.latitude);
        REQUIRE(retrieved_metadata.longitude == metadata.longitude);
        REQUIRE(retrieved_metadata.camera_index == metadata.camera_index);


        std::vector<uint8_t> retrieved_data(metadata.image_size);
        for(size_t i = 0; i < image_data.size(); i++) {
            REQUIRE(buffer.get_data_chunk(&retrieved_data[i], 1) == ImageBufferError::NO_ERROR);
        }


        for (size_t i = 0; i < metadata.image_size; ++i) {
            REQUIRE(retrieved_data[i] == image_data[i]);
        }
    }

     SUBCASE("Add image exceeding capacity") {
        MockAccessor accessor(100); // Small buffer
        ImageBuffer<MockAccessor> buffer(accessor, 0, 100);

        ImageMetadata large_metadata;
        large_metadata.timestamp = 12345;
        large_metadata.image_size = 256;
        large_metadata.latitude = 37.7749;
        large_metadata.longitude = -122.4194;
        large_metadata.camera_index = 1;

        REQUIRE(buffer.add_image(large_metadata) == ImageBufferError::FULL_BUFFER);
    }


    SUBCASE("Pop Image Success") {
        REQUIRE(buffer.add_image(metadata) == ImageBufferError::NO_ERROR);
        for(size_t i = 0; i < image_data.size(); i++) {
            REQUIRE(buffer.add_data_chunk(&image_data[i],1) == ImageBufferError::NO_ERROR);
        }
        REQUIRE(buffer.push_image() == ImageBufferError::NO_ERROR);


        ImageMetadata retrieved_metadata;
        REQUIRE(buffer.get_image(retrieved_metadata) == ImageBufferError::NO_ERROR);

        std::vector<uint8_t> retrieved_data(metadata.image_size);
        for(size_t i = 0; i < image_data.size(); i++) {
            REQUIRE(buffer.get_data_chunk(&retrieved_data[i], 1) == ImageBufferError::NO_ERROR);
        }

        REQUIRE(buffer.pop_image() == ImageBufferError::NO_ERROR);
        REQUIRE(buffer.is_empty() == true);
        REQUIRE(buffer.size() == 0);
        REQUIRE(buffer.count() == 0);

    }

    SUBCASE("Get Image From Empty Buffer") {
        ImageMetadata retrieved_metadata;
        REQUIRE(buffer.get_image(retrieved_metadata) == ImageBufferError::EMPTY_BUFFER);
    }
}

TEST_CASE("ImageBuffer Wrap-Around") {
    MockAccessor accessor(512);
    ImageBuffer<MockAccessor> buffer(accessor, 0, 512);

    ImageMetadata metadata;
    metadata.timestamp = 12345;
    metadata.image_size = 256;
    metadata.latitude = 37.7749;
    metadata.longitude = -122.4194;
    metadata.camera_index = 1;

    std::vector<uint8_t> image_data(metadata.image_size);
    for (size_t i = 0; i < metadata.image_size; ++i) {
        image_data[i] = static_cast<uint8_t>(i % 256);
    }

    // Fill most of the buffer
    REQUIRE(buffer.add_image(metadata) == ImageBufferError::NO_ERROR);
    for(size_t i = 0; i < image_data.size(); i++) {
        REQUIRE(buffer.add_data_chunk(&image_data[i],1) == ImageBufferError::NO_ERROR);
    }
    REQUIRE(buffer.push_image() == ImageBufferError::NO_ERROR);

    // Add another image that will cause wrap-around
    ImageMetadata metadata2;
    metadata2.timestamp = 67890;
    metadata2.image_size = 128;
    metadata2.latitude = 34.0522;
    metadata2.longitude = -118.2437;
    metadata2.camera_index = 2;

    std::vector<uint8_t> image_data2(metadata2.image_size);
    for (size_t i = 0; i < metadata2.image_size; ++i) {
        image_data2[i] = static_cast<uint8_t>((i + 100) % 256);
    }

    auto result = buffer.add_image(metadata2);
    REQUIRE(result == ImageBufferError::NO_ERROR);
    for(size_t i = 0; i < image_data2.size(); i++) {
        REQUIRE(buffer.add_data_chunk(&image_data2[i],1) == ImageBufferError::NO_ERROR);
    }
    REQUIRE(buffer.push_image() == ImageBufferError::NO_ERROR);


    REQUIRE(buffer.count() == 2);

    // Retrieve the images to check for correct data after wrap-around
    ImageMetadata retrieved_metadata;
    REQUIRE(buffer.get_image(retrieved_metadata) == ImageBufferError::NO_ERROR);
    std::vector<uint8_t> retrieved_data(metadata.image_size);
        for(size_t i = 0; i < image_data.size(); i++) {
            REQUIRE(buffer.get_data_chunk(&retrieved_data[i], 1) == ImageBufferError::NO_ERROR);
        }


    for (size_t i = 0; i < metadata.image_size; ++i) {
        REQUIRE(retrieved_data[i] == image_data[i]);
    }

    REQUIRE(buffer.pop_image() == ImageBufferError::NO_ERROR);

    ImageMetadata retrieved_metadata2;
    REQUIRE(buffer.get_image(retrieved_metadata2) == ImageBufferError::NO_ERROR);

    std::vector<uint8_t> retrieved_data2(metadata2.image_size);
    for(size_t i = 0; i < image_data2.size(); i++) {
            REQUIRE(buffer.get_data_chunk(&retrieved_data2[i], 1) == ImageBufferError::NO_ERROR);
        }

    for (size_t i = 0; i < metadata2.image_size; ++i) {
        REQUIRE(retrieved_data2[i] == image_data2[i]);
    }
}