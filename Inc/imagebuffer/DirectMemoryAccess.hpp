// DirectMemoryAccess.hpp
#ifndef DIRECT_MEMORY_ACCESS_H
#define DIRECT_MEMORY_ACCESS_H

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <cstring>
#include <vector>

class DirectMemoryAccess
{
public:
    DirectMemoryAccess(uint32_t flash_start, uint32_t total_size)
        : FLASH_START_ADDRESS(flash_start), TOTAL_BUFFER_SIZE(total_size)
    {
        flash_memory.resize(TOTAL_BUFFER_SIZE, 0); // Allocate and zero-initialize the buffer
    }

    int32_t write(uint32_t address, const uint8_t *data, size_t size);
    int32_t read(uint32_t address, uint8_t *data, size_t size);
    int32_t erase(uint32_t address);
    std::vector<uint8_t>& getFlashMemory() { return flash_memory; }

private:
    int32_t checkBounds(uint32_t address, size_t size);

private:
    const uint32_t FLASH_START_ADDRESS; // Make it a class member
    const uint32_t TOTAL_BUFFER_SIZE;   // Make it a class member
    std::vector<uint8_t> flash_memory; // Use a vector for dynamic allocation
};

int32_t DirectMemoryAccess::write(uint32_t address, const uint8_t *data, size_t size)
{
    // Check if the access is within bounds using base class method
    if (checkBounds(address, size) != 0)
    {
        return -1; // Return error if out of bounds
    }

    size_t offset = address - FLASH_START_ADDRESS;

    // Perform the memory copy
    std::memcpy(flash_memory.data() + offset, data, size);
    return 0; // Return success
}

int32_t DirectMemoryAccess::read(uint32_t address, uint8_t *data, size_t size)
{
    // Check if the access is within bounds using base class method
    if (checkBounds(address, size) != 0)
    {
        return -1; // Return error if out of bounds
    }

    size_t offset = address - FLASH_START_ADDRESS;
    // Perform the memory copy
    std::memcpy(data, flash_memory.data() + offset, size);
    return 0; // Return success
}

int32_t DirectMemoryAccess::erase(uint32_t /*address*/)
{
    // Simulate erasing a sector (e.g., by setting all bytes in the sector to 0xFF)
    // Implement sector size and erase logic here
    std::fill(flash_memory.begin(), flash_memory.end(), 0xFF); // Simulate erasing by filling with 0xFF
    return 0; // Success
}

int32_t DirectMemoryAccess::checkBounds(uint32_t address, size_t size)
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