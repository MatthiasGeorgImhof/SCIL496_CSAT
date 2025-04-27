#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "imagebuffer/access.hpp"
#include "imagebuffer/DirectMemoryAccess.hpp"
#include "imagebuffer/BufferedAccessor.hpp"
#include "ImageBuffer.hpp"

#include "imagebuffer/access.hpp"
#include "mock_hal.h"
#include "Checksum.hpp"

#include <vector>
#include <iostream>

// // Mock Accessor for testing
// struct MockAccessor
// {
//     size_t start;
//     size_t size;
//     std::vector<uint8_t> data;
//     size_t alignment;

//     MockAccessor(size_t start, size_t size, size_t alignment = 1) : start(start), size(size), data(size, 0), alignment(alignment) {}

//     std::vector<uint8_t> &getFlashMemory() { return data; }
//     size_t getFlashMemorySize() const { return size; };
//     size_t getFlashStartAddress() const { return start; };
//     size_t getAlignment() const { return alignment; }

//     AccessError write(uint32_t address, const uint8_t *buffer, size_t num_bytes)
//     {
//         if (address - start + num_bytes > size)
//         {
//             std::cerr << "MockAccessor::write: out of bounds write. address=" << address << ", num_bytes=" << num_bytes << ", size=" << size << std::endl;
//             return AccessError::WRITE_ERROR; // Simulate write error
//         }
//         std::memcpy(data.data() + address - start, buffer, num_bytes);
//         return AccessError::NO_ERROR; // Simulate successful write
//     }

//     AccessError read(uint32_t address, uint8_t *buffer, size_t num_bytes)
//     {
//         if (address - start + num_bytes > size)
//         {
//             std::cerr << "MockAccessor::read: out of bounds read. address=" << address << ", num_bytes=" << num_bytes << ", size=" << size << std::endl;
//             return AccessError::READ_ERROR; // Simulate read error
//         }
//         std::memcpy(buffer, data.data() + address - start, num_bytes);
//         return AccessError::NO_ERROR; // Simulate successful read
//     }

//     AccessError erase(uint32_t address) {
//         std::cerr << "MockAccessor::erase: address=" << address << ", start=" << start << ", size=" << size << std::endl;

//         // Mock implementation - just set the bytes to 0
//         if (address < start || address >= start + size) {
//             std::cerr << "MockAccessor::erase: out of bounds erase. address=" << address << ", start=" << start << ", size=" << size << std::endl;
//             return AccessError::OUT_OF_BOUNDS;
//         }

//         // Mock implementation: Set all data to 0.  This makes reasoning about the code a lot easier.
//         std::fill(data.begin(), data.end(), 0);
//         return AccessError::NO_ERROR;
//     }


//     void reset()
//     {
//         std::fill(data.begin(), data.end(), 0);
//     }
// };

// static_assert(Accessor<MockAccessor>, "MockAccessor does not satisfy the Accessor concept!");

// Test case for ImageBuffer with BufferedAccessor

TEST_CASE("ImageBuffer with BufferedAccessor")
{
    constexpr size_t flash_start = 0x4000;
    constexpr size_t flash_size = 4096;
    constexpr size_t block_size = 512;

    DirectMemoryAccess base_accessor(flash_start, flash_size);
    BufferedAccessor<DirectMemoryAccess, block_size> buffered_accessor(base_accessor);
    ImageBuffer<BufferedAccessor<DirectMemoryAccess, block_size>> buffer(buffered_accessor);

    ImageMetadata metadata;
    metadata.timestamp = 12345;
    metadata.image_size = 256;
    metadata.latitude = 37.7749;
    metadata.longitude = -122.4194;
    metadata.camera_index = 1;

    std::vector<uint8_t> image_data(metadata.image_size);
    for (size_t i = 0; i < metadata.image_size; ++i)
    {
        image_data[i] = static_cast<uint8_t>(i % 256);
    }

    SUBCASE("Add and Get Image with BufferedAccessor, chunksize 1")
    {
        constexpr size_t CHUNKSIZE = 1;
        // std::cerr << "ImageBuffer: Calling add_image" << std::endl;
        REQUIRE(buffer.add_image(metadata) == ImageBufferError::NO_ERROR);
        // std::cerr << "ImageBuffer: add_image returned" << std::endl;

        for (size_t i = 0; i < image_data.size(); i++)
        {
            // std::cerr << "ImageBuffer: Calling add_data_chunk, i=" << i << std::endl;
            REQUIRE(buffer.add_data_chunk(&image_data[i], CHUNKSIZE) == ImageBufferError::NO_ERROR);
            // std::cerr << "ImageBuffer: add_data_chunk returned" << std::endl;
        }

        std::cerr << "ImageBuffer: Calling push_image" << std::endl;
        REQUIRE(buffer.push_image() == ImageBufferError::NO_ERROR);
        // std::cerr << "ImageBuffer: push_image returned" << std::endl;

        ImageMetadata retrieved_metadata;

        // std::cerr << "ImageBuffer: Calling get_image" << std::endl;
        REQUIRE(buffer.get_image(retrieved_metadata) == ImageBufferError::NO_ERROR);
        // std::cerr << "ImageBuffer: get_image returned" << std::endl;

        REQUIRE(retrieved_metadata.magic == IMAGE_MAGIC);
        REQUIRE(retrieved_metadata.timestamp == metadata.timestamp);
        REQUIRE(retrieved_metadata.image_size == metadata.image_size);
        REQUIRE(retrieved_metadata.latitude == metadata.latitude);
        REQUIRE(retrieved_metadata.longitude == metadata.longitude);
        REQUIRE(retrieved_metadata.camera_index == metadata.camera_index);

        std::vector<uint8_t> retrieved_data(metadata.image_size);
        for (size_t i = 0; i < image_data.size(); i++)
        {
            size_t size = CHUNKSIZE;
            // std::cerr << "ImageBuffer: Calling get_data_chunk, i=" << i << std::endl;
            REQUIRE(buffer.get_data_chunk(&retrieved_data[i], size) == ImageBufferError::NO_ERROR);
            // std::cerr << "ImageBuffer: get_data_chunk returned, size=" << size << std::endl;
            REQUIRE(size == CHUNKSIZE);
        }

        for (size_t i = 0; i < metadata.image_size; ++i)
        {
            REQUIRE(retrieved_data[i] == image_data[i]);
        }

        buffer.pop_image();
        REQUIRE(buffer.is_empty());
    }

    SUBCASE("Add and Get Image with BufferedAccessor, chunksize 33")
    {
        constexpr size_t CHUNKSIZE = 33;
        // std::cerr << "ImageBuffer: Calling add_image" << std::endl;
        REQUIRE(buffer.add_image(metadata) == ImageBufferError::NO_ERROR);
        // std::cerr << "ImageBuffer: add_image returned" << std::endl;

        for (size_t i = 0; i < image_data.size(); i+=CHUNKSIZE)
        {
            // std::cerr << "ImageBuffer: Calling add_data_chunk, i=" << i << std::endl;
            REQUIRE(buffer.add_data_chunk(&image_data[i], CHUNKSIZE) == ImageBufferError::NO_ERROR);
            // std::cerr << "ImageBuffer: add_data_chunk returned" << std::endl;
        }

        std::cerr << "ImageBuffer: Calling push_image" << std::endl;
        REQUIRE(buffer.push_image() == ImageBufferError::NO_ERROR);
        // std::cerr << "ImageBuffer: push_image returned" << std::endl;

        ImageMetadata retrieved_metadata;

        // std::cerr << "ImageBuffer: Calling get_image" << std::endl;
        REQUIRE(buffer.get_image(retrieved_metadata) == ImageBufferError::NO_ERROR);
        // std::cerr << "ImageBuffer: get_image returned" << std::endl;

        REQUIRE(retrieved_metadata.magic == IMAGE_MAGIC);
        REQUIRE(retrieved_metadata.timestamp == metadata.timestamp);
        REQUIRE(retrieved_metadata.image_size == metadata.image_size);
        REQUIRE(retrieved_metadata.latitude == metadata.latitude);
        REQUIRE(retrieved_metadata.longitude == metadata.longitude);
        REQUIRE(retrieved_metadata.camera_index == metadata.camera_index);

        std::vector<uint8_t> retrieved_data(metadata.image_size);
        // size_t niter = image_data.size()/CHUNKSIZE;
        size_t nrem = image_data.size()%CHUNKSIZE;
        for (size_t i = 0; i < image_data.size(); i+=CHUNKSIZE)
        {
            size_t size = CHUNKSIZE;
            // std::cerr << "ImageBuffer: Calling get_data_chunk, i=" << i << std::endl;
            REQUIRE(buffer.get_data_chunk(&retrieved_data[i], size) == ImageBufferError::NO_ERROR);
            // std::cerr << "ImageBuffer: get_data_chunk returned, size=" << size << std::endl;
            bool result = size == CHUNKSIZE || size == nrem;
            REQUIRE(result);
        }

        for (size_t i = 0; i < metadata.image_size; ++i)
        {
            REQUIRE(retrieved_data[i] == image_data[i]);
        }

        buffer.pop_image();
        REQUIRE(buffer.is_empty());

    }

}