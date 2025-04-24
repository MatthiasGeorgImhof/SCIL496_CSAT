#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "imagebuffer/DirectMemoryAccess.hpp"
#include "imagebuffer/LinuxMockHALFlashAccess.hpp"
#include "imagebuffer/access.hpp"
#include "mock_hal.h"
#include <iostream>
#include <vector> // Include vector header

// Helper function to compare memory regions
bool compareMemory(const uint8_t *mem1, const uint8_t *mem2, size_t size)
{
    return std::memcmp(mem1, mem2, size) == 0;
}

// Test case for DirectMemoryAccess
TEST_CASE("DirectMemoryAccess")
{
    const uint32_t FLASH_START = 0x08000000;
    const uint32_t FLASH_SIZE = 1024;
    DirectMemoryAccess dma(FLASH_START, FLASH_SIZE);

    SUBCASE("Write and Read within bounds")
    {
        uint32_t address = FLASH_START + 10;
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        size_t size = sizeof(data);
        std::vector<uint8_t> read_data(size); // Use std::vector

        REQUIRE(dma.write(address, data, size) == AccessError::NO_ERROR);
        REQUIRE(dma.read(address, read_data.data(), size) == AccessError::NO_ERROR); // Pass the data pointer
        REQUIRE(compareMemory(data, read_data.data(), size));    // Pass the data pointer
    }

    SUBCASE("Write out of bounds")
    {
        uint32_t address = FLASH_START + FLASH_SIZE;
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        size_t size = sizeof(data);

        REQUIRE(dma.write(address, data, size) == AccessError::OUT_OF_BOUNDS);
    }

    SUBCASE("Read out of bounds")
    {
        uint32_t address = FLASH_START + FLASH_SIZE;
        std::vector<uint8_t> data(4); // Fixed size
        size_t size = data.size();

        REQUIRE(dma.read(address, data.data(), size) == AccessError::OUT_OF_BOUNDS);
    }

    SUBCASE("Erase (simulated)")
    {
        uint32_t address = FLASH_START + 10;
        REQUIRE(dma.erase(address) == AccessError::NO_ERROR); // Simply checks that the function runs without error
    }
}

// Test case for LinuxMockHALFlashAccess
TEST_CASE("LinuxMockHALFlashAccess")
{
    const uint32_t FLASH_START = 0x08000000;
    const uint32_t FLASH_SIZE = 1024;
    I2C_HandleTypeDef hi2c; 
    LinuxMockHALFlashAccess hal(&hi2c, FLASH_START, FLASH_SIZE);

    SUBCASE("Write and Read within bounds")
    {
        uint32_t address = FLASH_START + 10;
        uint8_t data[] = {0x05, 0x06, 0x07, 0x08};
        size_t size = sizeof(data);
        std::vector<uint8_t> read_data(size); // Use std::vector
        auto result = hal.write(address, data, size);
        REQUIRE(result == AccessError::NO_ERROR);
        result = hal.read(address, read_data.data(), size);
        REQUIRE(result == AccessError::NO_ERROR);
        bool result_bool = compareMemory(data, read_data.data(), size);
        REQUIRE(result_bool == true);
    }

    SUBCASE("Write out of bounds")
    {
        uint32_t address = FLASH_START + FLASH_SIZE;
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        size_t size = sizeof(data);

        REQUIRE(hal.write(address, data, size) == AccessError::OUT_OF_BOUNDS);
    }

    SUBCASE("Read out of bounds")
    {
        uint32_t address = FLASH_START + FLASH_SIZE;
        std::vector<uint8_t> data(4); // Fixed size
        size_t size = data.size();

        REQUIRE(hal.read(address, data.data(), size) == AccessError::OUT_OF_BOUNDS);
    }

    SUBCASE("Erase (simulated)")
    {
        uint32_t address = FLASH_START + 10;
        REQUIRE(hal.erase(address) == AccessError::NO_ERROR); // Simply checks that the function runs without error
    }
}

TEST_CASE("DirectMemoryAccess and LinuxMockHALFlashAccess API consistency")
{
    const uint32_t FLASH_START = 0x08000000;
    const uint32_t FLASH_SIZE = 1024;
    I2C_HandleTypeDef hi2c; 

    DirectMemoryAccess dma(FLASH_START, FLASH_SIZE);
    LinuxMockHALFlashAccess hal(&hi2c, FLASH_START, FLASH_SIZE);

    uint32_t address = FLASH_START + 10;
    uint8_t data[] = {0x09, 0x0A, 0x0B, 0x0C};
    size_t size = sizeof(data);
    std::vector<uint8_t> read_data_dma(size); // Use std::vector
    std::vector<uint8_t> read_data_hal(size); // Use std::vector

    // Write using both APIs
    REQUIRE(dma.write(address, data, size) == AccessError::NO_ERROR);
    REQUIRE(hal.write(address, data, size) == AccessError::NO_ERROR);

    // Read using both APIs
    REQUIRE(dma.read(address, read_data_dma.data(), size) == AccessError::NO_ERROR); // Pass the data pointer
    REQUIRE(hal.read(address, read_data_hal.data(), size) == AccessError::NO_ERROR); // Pass the data pointer

    // Compare the read data
    REQUIRE(compareMemory(read_data_dma.data(), read_data_hal.data(), size)); // Pass the data pointer

    // Erase using both APIs
    REQUIRE(dma.erase(address) == AccessError::NO_ERROR);
    REQUIRE(hal.erase(address) == AccessError::NO_ERROR);
}