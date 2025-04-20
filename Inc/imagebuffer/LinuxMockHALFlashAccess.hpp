#ifndef LINUX_MOCK_HAL_FLASH_ACCESS_H
#define LINUX_MOCK_HAL_FLASH_ACCESS_H

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <cstring>
#include "mock_hal.h"

class LinuxMockHALFlashAccess
{
public:
    LinuxMockHALFlashAccess() = delete;
    
    LinuxMockHALFlashAccess(I2C_HandleTypeDef *hi2c, uint32_t flash_start, uint32_t total_size)
        : hi2c_(hi2c), FLASH_START_ADDRESS(flash_start), TOTAL_BUFFER_SIZE(total_size)
    {
        flash_memory.resize(TOTAL_BUFFER_SIZE, 0xff);
    }

    int32_t write(uint32_t address, const uint8_t *data, size_t size);
    int32_t read(uint32_t address, uint8_t *data, size_t size);
    int32_t erase(uint32_t address);
    std::vector<uint8_t>& getFlashMemory() { return flash_memory; }

private:
    int32_t checkBounds(uint32_t address, size_t size);

private:
    I2C_HandleTypeDef *hi2c_;
    const uint32_t FLASH_START_ADDRESS;
    const uint32_t TOTAL_BUFFER_SIZE;
    std::vector<uint8_t> flash_memory;
};

int32_t LinuxMockHALFlashAccess::write(uint32_t address, const uint8_t *data, size_t size)
{
    // Check bounds
    if (checkBounds(address, size) != 0)
    {
        return -1; // Return error if out of bounds
    }

    size_t offset = address - FLASH_START_ADDRESS;
    // Now, call the mocked HAL_I2C_Mem_Write function to simulate the I2C write
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(hi2c_, 0xA0, offset, 2, (uint8_t *)data, static_cast<uint16_t>(size), 100);
    if (status != HAL_OK)
    {
        std::cerr << "I2C Write Failed (Mock HAL)" << std::endl;
        return -1;
    }

    // Perform the memory copy to the simulated flash
    std::memcpy(flash_memory.data() + offset, data, size);

    return 0;
}

int32_t LinuxMockHALFlashAccess::read(uint32_t address, uint8_t *data, size_t size)
{
    // Check bounds
    if (checkBounds(address, size) != 0)
    {
        return -1; // Return error if out of bounds
    }

    size_t offset = address - FLASH_START_ADDRESS;
    // Now, call the mocked HAL_I2C_Mem_Read function to simulate the I2C read
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2c_, 0xA0, offset, 2, (uint8_t *)data, static_cast<uint16_t>(size), 100);
    if (status != HAL_OK)
    {
        std::cerr << "I2C Read Failed (Mock HAL)" << std::endl;
        return -1;
    }

    // Perform the memory copy from the simulated flash
    std::memcpy(data, flash_memory.data() + offset, size);

    return 0;
}

int32_t LinuxMockHALFlashAccess::erase(uint32_t /*address*/)
{
    // Implement the flash erase sequence using the HAL mocks
    // For now, just return success.  In a real implementation, you would need
    // to simulate the flash erase operation, possibly by writing 0xFF to the
    // affected memory region.
    std::fill(flash_memory.begin(), flash_memory.end(), 0xFF); // Simulate erasing by filling with 0xFF
    return 0;                                                  // Success
}

int32_t LinuxMockHALFlashAccess::checkBounds(uint32_t address, size_t size)
{
    if (address < FLASH_START_ADDRESS || address + size > FLASH_START_ADDRESS + TOTAL_BUFFER_SIZE)
    {
        std::cerr << "Error: Access out of bounds. Address: 0x" << std::hex << address
                  << ", Size: " << size << std::dec << std::endl; // Added address and size for debugging
        return -1;                                                // Return error
    }
    return 0; // Return success
}

#endif