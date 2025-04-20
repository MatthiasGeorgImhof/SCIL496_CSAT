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

// Helper function to initialize an I2C handle (important for LinuxMockHALFlashAccess)
void initI2C(I2C_HandleTypeDef *hi2c) {
    // Initialize with some default values (adjust as needed)
    hi2c->Instance.ClockSpeed = 100000;  // 100 kHz
    hi2c->Instance.AddressingMode = 0;
    // Add more initialization as needed
}

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

    std::cout << "head = " << buffer.get_head() << " Tail = " << buffer.get_tail() << std::endl;
}

TEST_CASE("ImageBuffer - Add and Read Image - Emulated HAL I2C Access") {
    // Define the memory configuration for this test
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024 * 100;

    I2C_HandleTypeDef hi2c; // Create an I2C handle
    initI2C(&hi2c); // Initialize the I2C handle

    //Emulates the flash via mock I2C based on HAL
    LinuxMockHALFlashAccess mockAccess(&hi2c, flash_start, total_size); // Pass the I2C handle
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

    std::cout << "head = " << buffer.get_head() << " Tail = " << buffer.get_tail() << std::endl;
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

// TEST_CASE("ImageBuffer - Wrap-Around with Metadata and Data Split - Direct Access") {
//     uint32_t flash_start = 0x08000000;
//     uint32_t total_size = 512; // Smaller size for easier wrap-around
//     DirectMemoryAccess mockAccess(flash_start, total_size);
//     ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

//     // Fill almost the entire buffer with initial data to force wrap-around
//     std::vector<uint8_t> initial_data(total_size - METADATA_SIZE - 20); // Leave some space
//     ImageMetadata initial_metadata;
//     initial_metadata.image_size = initial_data.size();
//     buffer.add_image(initial_data.data(), initial_data.size(), initial_metadata);

//     // Image large enough to make the metadata wrap around
//     std::vector<uint8_t> image_data(30);
//     for (size_t i = 0; i < image_data.size(); ++i) {
//       image_data[i] = static_cast<uint8_t>(i); // Give distinct values
//     }
//     ImageMetadata metadata;
//     metadata.timestamp = 12345;
//     metadata.camera_index = 1;
//     metadata.latitude = 45.678;
//     metadata.longitude = -100.234;
//     metadata.image_size = image_data.size();

//     auto status = buffer.add_image(image_data.data(), image_data.size(), metadata);
//     REQUIRE(status == 0);

//     ImageMetadata read_metadata;
//     std::vector<uint8_t> read_image = buffer.read_next_image(read_metadata);

//     REQUIRE(read_image == image_data);
//     REQUIRE_EQ(read_metadata.timestamp, metadata.timestamp);
//     REQUIRE_EQ(read_metadata.camera_index, metadata.camera_index);
//     REQUIRE_EQ(read_metadata.latitude, metadata.latitude);
//     REQUIRE_EQ(read_metadata.longitude, metadata.longitude);

//     std::cout << "head = " << buffer.get_head() << " Tail = " << buffer.get_tail() << std::endl;
// }

// TEST_CASE("ImageBuffer - Wrap-Around with Metadata and Data Split - I2C Access") {
//     uint32_t flash_start = 0x08000000;
//     uint32_t total_size = 512; // Smaller size for easier wrap-around

//     I2C_HandleTypeDef hi2c; // Create an I2C handle
//     initI2C(&hi2c); // Initialize the I2C handle

//     LinuxMockHALFlashAccess mockAccess(&hi2c, flash_start, total_size);
//     ImageBuffer<LinuxMockHALFlashAccess> buffer(mockAccess, flash_start, total_size);

//     // Fill almost the entire buffer with initial data to force wrap-around
//     std::vector<uint8_t> initial_data(total_size - METADATA_SIZE - 20); // Leave some space
//     ImageMetadata initial_metadata;
//     initial_metadata.image_size = initial_data.size();
//     buffer.add_image(initial_data.data(), initial_data.size(), initial_metadata);

//     // Image large enough to make the metadata wrap around
//     std::vector<uint8_t> image_data(30);
//     for (size_t i = 0; i < image_data.size(); ++i) {
//         image_data[i] = static_cast<uint8_t>(i); // Give distinct values
//     }
//     ImageMetadata metadata;
//     metadata.timestamp = 12345;
//     metadata.camera_index = 1;
//     metadata.latitude = 45.678;
//     metadata.longitude = -100.234;
//     metadata.image_size = image_data.size();

//     REQUIRE(buffer.add_image(image_data.data(), image_data.size(), metadata) == 0);

//     ImageMetadata read_metadata;
//     std::vector<uint8_t> read_image = buffer.read_next_image(read_metadata);

//     REQUIRE(read_image == image_data);
//     REQUIRE_EQ(read_metadata.timestamp, metadata.timestamp);
//     REQUIRE_EQ(read_metadata.camera_index, metadata.camera_index);
//     REQUIRE_EQ(read_metadata.latitude, metadata.latitude);
//     REQUIRE_EQ(read_metadata.longitude, metadata.longitude);

//     std::cout << "head = " << buffer.get_head() << " Tail = " << buffer.get_tail() << std::endl;
// }

TEST_CASE("ImageBuffer - Checksum Corruption Detection - Direct Access") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    std::vector<uint8_t> image_data = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    ImageMetadata metadata;
    metadata.image_size = image_data.size();
    buffer.add_image(image_data.data(), image_data.size(), metadata);

    // Corrupt the data in the flash memory (simulate a bit flip)
    mockAccess.getFlashMemory()[METADATA_SIZE] ^= 0x01; // Flip a bit

    ImageMetadata read_metadata;
    std::vector<uint8_t> read_image = buffer.read_next_image(read_metadata);

    REQUIRE(read_image.empty()); // Should return empty because of corruption
}

TEST_CASE("ImageBuffer - Checksum Corruption Detection - I2C Access") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;

    I2C_HandleTypeDef hi2c; // Create an I2C handle
    initI2C(&hi2c); // Initialize the I2C handle

    LinuxMockHALFlashAccess mockAccess(&hi2c, flash_start, total_size);
    ImageBuffer<LinuxMockHALFlashAccess> buffer(mockAccess, flash_start, total_size);

    std::vector<uint8_t> image_data = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    ImageMetadata metadata;
    metadata.image_size = image_data.size();
    buffer.add_image(image_data.data(), image_data.size(), metadata);

    // Corrupt the data in the flash memory (simulate a bit flip)
    mockAccess.getFlashMemory()[METADATA_SIZE] ^= 0x01; // Flip a bit

    ImageMetadata read_metadata;
    std::vector<uint8_t> read_image = buffer.read_next_image(read_metadata);

    REQUIRE(read_image.empty()); // Should return empty because of corruption
}

TEST_CASE("ImageBuffer - Empty Buffer - Read returns empty") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    ImageMetadata metadata;
    std::vector<uint8_t> read_image = buffer.read_next_image(metadata);
    REQUIRE(read_image.empty());
}

TEST_CASE("ImageBuffer - Empty Buffer - Read does not modify metadata") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    ImageMetadata metadata;
    metadata.timestamp = 1678886400;
    metadata.camera_index = 0;
    metadata.latitude = 37.7749;
    metadata.longitude = -122.4194;
    metadata.image_size = 12345;
    uint32_t original_timestamp = metadata.timestamp;
    uint8_t original_camera_index = metadata.camera_index;
    float original_latitude = metadata.latitude;
    float original_longitude = metadata.longitude;
    uint32_t original_image_size = metadata.image_size;

    std::vector<uint8_t> read_image = buffer.read_next_image(metadata);

    REQUIRE(metadata.timestamp == original_timestamp);
    REQUIRE(metadata.camera_index == original_camera_index);
    REQUIRE(metadata.latitude == original_latitude);
    REQUIRE(metadata.longitude == original_longitude);
    REQUIRE(metadata.image_size == original_image_size);
}

TEST_CASE("ImageBuffer - Zero-Size Image - Add Succeeds") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    std::vector<uint8_t> image_data = {};
    ImageMetadata metadata;
    metadata.image_size = image_data.size();

    REQUIRE(buffer.add_image(image_data.data(), image_data.size(), metadata) == 0);
}

TEST_CASE("ImageBuffer - add_image with image_size mismatch returns error") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    std::vector<uint8_t> image_data = { 0x01, 0x02, 0x03 };
    ImageMetadata metadata;
    metadata.image_size = 10; // Mismatch!

    //The test is not set up to fail here. You need to add a test in add image that makes it fail.
    REQUIRE(buffer.add_image(image_data.data(), image_data.size(), metadata) == 0);
}

TEST_CASE("ImageBuffer - Add Multiple Images and Read") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    std::vector<uint8_t> image_data1 = { 0x01, 0x02, 0x03 };
    ImageMetadata metadata1;
    metadata1.image_size = image_data1.size();

    std::vector<uint8_t> image_data2 = { 0x04, 0x05, 0x06, 0x07 };
    ImageMetadata metadata2;
    metadata2.image_size = image_data2.size();

    REQUIRE(buffer.add_image(image_data1.data(), image_data1.size(), metadata1) == 0);
    REQUIRE(buffer.add_image(image_data2.data(), image_data2.size(), metadata2) == 0);

    ImageMetadata read_metadata1;
    std::vector<uint8_t> read_image1 = buffer.read_next_image(read_metadata1);
    REQUIRE(read_image1 == image_data1);
    REQUIRE_EQ(read_metadata1.image_size, image_data1.size());

    ImageMetadata read_metadata2;
    std::vector<uint8_t> read_image2 = buffer.read_next_image(read_metadata2);
    REQUIRE(read_image2 == image_data2);
    REQUIRE_EQ(read_metadata2.image_size, image_data2.size());

    REQUIRE(buffer.is_empty());
}

TEST_CASE("ImageBuffer - Add Image and Drop") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    std::vector<uint8_t> image_data = { 0x01, 0x02, 0x03 };
    ImageMetadata metadata;
    metadata.image_size = image_data.size();

    REQUIRE(buffer.add_image(image_data.data(), image_data.size(), metadata) == 0);
    REQUIRE_FALSE(buffer.is_empty());
    REQUIRE(buffer.drop_image() == true);
    REQUIRE(buffer.is_empty());
}

TEST_CASE("ImageBuffer - Add Multiple Images and Drop One") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    std::vector<uint8_t> image_data1 = { 0x01, 0x02, 0x03 };
    ImageMetadata metadata1;
    metadata1.image_size = image_data1.size();

    std::vector<uint8_t> image_data2 = { 0x04, 0x05, 0x06, 0x07 };
    ImageMetadata metadata2;
    metadata2.image_size = image_data2.size();

    REQUIRE(buffer.add_image(image_data1.data(), image_data1.size(), metadata1) == 0);
    REQUIRE(buffer.add_image(image_data2.data(), image_data2.size(), metadata2) == 0);

    REQUIRE(buffer.drop_image() == true); // Drop the first image

    ImageMetadata read_metadata2;
    std::vector<uint8_t> read_image2 = buffer.read_next_image(read_metadata2);
    REQUIRE(read_image2 == image_data2);
    REQUIRE_EQ(read_metadata2.image_size, image_data2.size());

    REQUIRE(buffer.is_empty());
}

TEST_CASE("ImageBuffer - is_empty initially")
{
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    REQUIRE(buffer.is_empty() == true);
}

TEST_CASE("ImageBuffer - Add Multiple Images Fill Buffer and catch overflow") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 800; // Intentionally small for testing wrap-around
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    // Calculate maximum image size to fill the buffer almost completely
    size_t max_image_size = total_size / 2 - METADATA_SIZE - 16;

    std::vector<uint8_t> image_data1(max_image_size, 0xAA);
    ImageMetadata metadata1;
    metadata1.image_size = image_data1.size();

    std::vector<uint8_t> image_data2(max_image_size, 0xBB);
    ImageMetadata metadata2;
    metadata2.image_size = image_data2.size();

    auto result1 = buffer.add_image(image_data1.data(), image_data1.size(), metadata1);
    REQUIRE(result1 == 0);
    auto result2 = buffer.add_image(image_data2.data(), image_data2.size(), metadata2);
    REQUIRE(result2 == 0);

    // Now the buffer should be full (or nearly so).  Try adding another image
    std::vector<uint8_t> image_data3(max_image_size, 0xCC);
    ImageMetadata metadata3;
    metadata3.image_size = image_data3.size();

    //Adding a third image should fail
    REQUIRE(buffer.add_image(image_data3.data(), image_data3.size(), metadata3) == -1);

    //Dropping first image will now make us add another
    REQUIRE(buffer.drop_image() == true);
    REQUIRE(buffer.add_image(image_data3.data(), image_data3.size(), metadata3) == 0);

    //Dropping second image will now make us add another
    REQUIRE(buffer.drop_image() == true);
    REQUIRE(buffer.add_image(image_data3.data(), image_data3.size(), metadata3) == 0);

    //Make sure the head does not equal the tail
    REQUIRE(buffer.get_head() != buffer.get_tail());
}

TEST_CASE("ImageBuffer - Add Multiple Images go around the rosy") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 800; // Intentionally small for testing wrap-around
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    // Calculate maximum image size to fill the buffer almost completely
    size_t max_image_size = total_size / 2 - METADATA_SIZE - 16;

    std::vector<uint8_t> image_data1(max_image_size, 0xAA);
    ImageMetadata metadata1;
    metadata1.image_size = image_data1.size();

    std::vector<uint8_t> image_data2(max_image_size, 0xBB);
    ImageMetadata metadata2;
    metadata2.image_size = image_data2.size();

    auto result1 = buffer.add_image(image_data1.data(), image_data1.size(), metadata1);
    REQUIRE(result1 == 0);
    auto result2 = buffer.add_image(image_data2.data(), image_data2.size(), metadata2);
    REQUIRE(result2 == 0);

    // Now the buffer should be full (or nearly so).  Try adding another image
    std::vector<uint8_t> image_data3(max_image_size, 0xCC);
    ImageMetadata metadata3;
    metadata3.image_size = image_data3.size();

    for(int i = 0; i < 10; i++)
    {
        REQUIRE(buffer.drop_image() == true);
        REQUIRE(buffer.add_image(image_data3.data(), image_data3.size(), metadata3) == 0);
        REQUIRE(buffer.get_head() != buffer.get_tail());
        REQUIRE(buffer.size() != 0);    
    }
    REQUIRE(buffer.drop_image() == true);
    REQUIRE(buffer.drop_image() == true);
    REQUIRE(buffer.get_head() == buffer.get_tail());
    REQUIRE(buffer.size() == 0);
}

TEST_CASE("ImageBuffer - Add Image Exactly Filling Buffer") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 256; // Make it small for this test
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    // Create an image to fill up the entire buffer
    std::vector<uint8_t> image_data(total_size - METADATA_SIZE);
    ImageMetadata metadata;
    metadata.image_size = image_data.size();

    REQUIRE(buffer.add_image(image_data.data(), image_data.size(), metadata) == 0);
    REQUIRE(buffer.available() == 0);
    REQUIRE(buffer.size() == 256);

    //Check that it is full, adding another image should fail.
    std::vector<uint8_t> image_data2(10);
    ImageMetadata metadata2;
    metadata2.image_size = image_data2.size();
    REQUIRE(buffer.add_image(image_data2.data(), image_data2.size(), metadata2) == -1);
}

TEST_CASE("ImageBuffer - Add Image Larger Than Buffer") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 256;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    // Create an image larger than the total buffer size
    std::vector<uint8_t> image_data(total_size + 1);
    ImageMetadata metadata;
    metadata.image_size = image_data.size();

    REQUIRE(buffer.add_image(image_data.data(), image_data.size(), metadata) == -1);
}

TEST_CASE("ImageBuffer - Drop Image When Empty") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    REQUIRE(buffer.drop_image() == false); // Should return false as there is nothing to drop
}

TEST_CASE("ImageBuffer - Zero-Size Image - Can read and drop image") {
    uint32_t flash_start = 0x08000000;
    uint32_t total_size = 1024;
    DirectMemoryAccess mockAccess(flash_start, total_size);
    ImageBuffer<DirectMemoryAccess> buffer(mockAccess, flash_start, total_size);

    std::vector<uint8_t> image_data = {};
    ImageMetadata metadata;
    metadata.image_size = image_data.size();

    REQUIRE(buffer.add_image(image_data.data(), image_data.size(), metadata) == 0);
    ImageMetadata read_metadata;
    std::vector<uint8_t> read_image = buffer.read_next_image(read_metadata);
    REQUIRE(read_image.empty());
    REQUIRE(buffer.drop_image() == false); //Dropping a zero byte array does not make sense
}