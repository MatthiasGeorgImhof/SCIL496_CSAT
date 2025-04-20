#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "ImageBuffer.hpp"
#include "imagebuffer/DirectMemoryAccess.hpp"
#include "imagebuffer/LinuxMockHALFlashAccess.hpp"
// #include "imagebuffer/STM32I2CFlashAccess.hpp"
#include "mock_hal.h"
#include <vector>
#include <iostream>
#include <cstring>

TEST_CASE("ImageBuffer - Add and Read Image - Static Access") {
    // Define the memory configuration for this test
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024 * 100;

    // Direct accessing of data in flash
    DirectMemoryAccess mockAccess(flash_start, total_size);

    // Pass access interface to template class, instead of injecting during runtime
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    // Create a sample image
    std::vector<uint8_t> image_data = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    size_t image_size = image_data.size();

    // Create sample metadata
    ImageMetadata metadata;
    metadata.timestamp = 1678886400;
    metadata.camera_index = 0;
    metadata.latitude = 37.7749;
    metadata.longitude = -122.4194;
    metadata.image_size = image_size;

    // Add the image to the buffer
    REQUIRE(buffer.add_image(image_data.data(), image_size, metadata) == 0);
    REQUIRE_FALSE(buffer.is_empty());

    // Read the image from the buffer
    ImageMetadata read_metadata;
    std::vector<uint8_t> read_image = buffer.read_next_image(read_metadata);

    //Assert that the vectors are equal
    REQUIRE(read_image == image_data);

    REQUIRE_EQ(read_metadata.timestamp, metadata.timestamp);
    REQUIRE_EQ(read_metadata.camera_index, metadata.camera_index);
    REQUIRE_EQ(read_metadata.latitude, metadata.latitude);
    REQUIRE_EQ(read_metadata.longitude, metadata.longitude);
    REQUIRE_EQ(read_metadata.image_size, image_size);

    REQUIRE(buffer.is_empty());

    std::cout << "head = " << buffer.getHead() << " Tail = " << buffer.getTail() << std::endl;
}

TEST_CASE("ImageBuffer - Add and Read Image - Emulated HAL I2C Access") {
    // Define the memory configuration for this test
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024 * 100;

    //Emulates the flash via mock I2C based on HAL
    LinuxMockHALFlashAccess mockAccess(flash_start, total_size);
    //Pass access interface to template class, instead of injecting during runtime
    ImageBuffer<LinuxMockHALFlashAccess> buffer(mockAccess, flash_start, total_size);

    // Create a sample image
    std::vector<uint8_t> image_data = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    size_t image_size = image_data.size();

    // Create sample metadata
    ImageMetadata metadata;
    metadata.timestamp = 1678886400;
    metadata.camera_index = 0;
    metadata.latitude = 37.7749;
    metadata.longitude = -122.4194;
    metadata.image_size = image_size;

    // Add the image to the buffer
    REQUIRE(buffer.add_image(image_data.data(), image_size, metadata) == 0);
    REQUIRE_FALSE(buffer.is_empty());

    // Read the image from the buffer
    ImageMetadata read_metadata;
    std::vector<uint8_t> read_image = buffer.read_next_image(read_metadata);

    REQUIRE(read_image == image_data);

    REQUIRE_EQ(read_metadata.timestamp, metadata.timestamp);
    REQUIRE_EQ(read_metadata.camera_index, metadata.camera_index);
    REQUIRE_EQ(read_metadata.latitude, metadata.latitude);
    REQUIRE_EQ(read_metadata.longitude, metadata.longitude);
    REQUIRE_EQ(read_metadata.image_size, image_size);

    REQUIRE(buffer.is_empty());

    std::cout << "head = " << buffer.getHead() << " Tail = " << buffer.getTail() << std::endl;
}

/*
I am commenting this code out because in order to test this code, we need to create stub code
*/
//
TEST_CASE("ImageBuffer - Add and Read Image - STM32 I2C Flash Access") {
    // Define the memory configuration for this test (if relevant)
    // uint32_t flash_start = 0x0; // Adjust as needed
    // uint32_t total_size = 65536; // Adjust as needed

  // This is a sign of bad code! You should not be writing to the static memory address here!
    // Direct accessing of data in flash
    //uint8_t* flash_memory = DirectMemoryAccess::static_flash_memory;

}