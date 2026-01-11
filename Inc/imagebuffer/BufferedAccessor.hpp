#ifndef BUFFERED_ACCESSOR_H
#define BUFFERED_ACCESSOR_H

#include "imagebuffer/accessor.hpp"
#include <algorithm>
#include <cstring>

// BufferedAccessor: block-caching wrapper around a BaseAccessor.
// All block math is done in logical (0-based) coordinates within the
// base accessor's flash region.
template <typename BaseAccessor, size_t BLOCK_SIZE>
class BufferedAccessor
{
public:
    BufferedAccessor(BaseAccessor &base_access)
        : base_access_(base_access),
          cache_dirty_(false),
          cache_valid_(false),
          cache_logical_addr_(0)
    {
    }

    ~BufferedAccessor()
    {
        if (cache_dirty_)
            flush_cache();
    }

    AccessorError write(size_t address, const uint8_t *data, size_t size);
    AccessorError read(size_t address, uint8_t *data, size_t size);
    AccessorError erase(size_t address);
    void format();

    size_t getAlignment() const { return 1; }
    size_t getFlashMemorySize() const { return base_access_.getFlashMemorySize(); }
    size_t getFlashStartAddress() const { return base_access_.getFlashStartAddress(); }
    size_t getEraseBlockSize() const { return base_access_.getEraseBlockSize(); }

    AccessorError flush_cache();

private:
    // absolute_block_start is the absolute address of the block start
    AccessorError fill_cache(size_t absolute_block_start);

private:
    BaseAccessor &base_access_;

    uint8_t cache_[BLOCK_SIZE];

    bool   cache_dirty_;
    bool   cache_valid_;
    // 0-based logical block start within the flash region
    size_t cache_logical_addr_;
};

// Flush current cached block (if any) to base accessor.
template <typename BaseAccessor, size_t BLOCK_SIZE>
AccessorError BufferedAccessor<BaseAccessor, BLOCK_SIZE>::flush_cache()
{
    if (!cache_dirty_ || !cache_valid_)
        return AccessorError::NO_ERROR;

    const size_t flash_start   = base_access_.getFlashStartAddress();
    const size_t abs_block_pos = flash_start + cache_logical_addr_;

    AccessorError result = base_access_.write(abs_block_pos, cache_, BLOCK_SIZE);
    if (result != AccessorError::NO_ERROR)
        return result;

    cache_dirty_ = false;
    return AccessorError::NO_ERROR;
}

// Load a full block into cache, given the absolute start address of the block.
template <typename BaseAccessor, size_t BLOCK_SIZE>
AccessorError BufferedAccessor<BaseAccessor, BLOCK_SIZE>::fill_cache(size_t absolute_block_start)
{
    const size_t flash_start = base_access_.getFlashStartAddress();
    const size_t flash_size  = base_access_.getFlashMemorySize();

    if (absolute_block_start < flash_start ||
        absolute_block_start + BLOCK_SIZE > flash_start + flash_size)
    {
        return AccessorError::OUT_OF_BOUNDS;
    }

    const size_t logical_block = absolute_block_start - flash_start;

    AccessorError result = flush_cache();
    if (result != AccessorError::NO_ERROR)
        return result;

    result = base_access_.read(absolute_block_start, cache_, BLOCK_SIZE);
    if (result != AccessorError::NO_ERROR)
        return result;

    cache_logical_addr_ = logical_block;
    cache_valid_        = true;
    return AccessorError::NO_ERROR;
}

// Write [address, address+size) in absolute coordinates.
template <typename BaseAccessor, size_t BLOCK_SIZE>
AccessorError BufferedAccessor<BaseAccessor, BLOCK_SIZE>::write(size_t address,
                                                                const uint8_t *data,
                                                                size_t size)
{
    const size_t flash_start = base_access_.getFlashStartAddress();
    const size_t flash_size  = base_access_.getFlashMemorySize();

    if (size == 0)
        return AccessorError::NO_ERROR;

    if (address < flash_start || (address - flash_start) + size > flash_size)
        return AccessorError::OUT_OF_BOUNDS;

    size_t logical     = address - flash_start; // 0-based within flash
    size_t data_offset = 0;

    while (size > 0)
    {
        const size_t block_offset        = logical % BLOCK_SIZE;
        const size_t block_start_logical = logical - block_offset;
        const size_t block_end_logical   = block_start_logical + BLOCK_SIZE;
        const size_t block_remaining     = block_end_logical - logical;
        const size_t done                = std::min(size, block_remaining);

        if (!cache_valid_ || block_start_logical != cache_logical_addr_)
        {
            const size_t abs_block_start = flash_start + block_start_logical;
            AccessorError fill_result    = fill_cache(abs_block_start);
            if (fill_result != AccessorError::NO_ERROR)
                return fill_result;
        }

        std::memcpy(cache_ + block_offset, data + data_offset, done);
        cache_dirty_ = true;

        data_offset += done;
        logical     += done;
        size        -= done;
    }

    return AccessorError::NO_ERROR;
}

// Read [address, address+size) in absolute coordinates.
template <typename BaseAccessor, size_t BLOCK_SIZE>
AccessorError BufferedAccessor<BaseAccessor, BLOCK_SIZE>::read(size_t address,
                                                               uint8_t *data,
                                                               size_t size)
{
    const size_t flash_start = base_access_.getFlashStartAddress();
    const size_t flash_size  = base_access_.getFlashMemorySize();

    if (size == 0)
        return AccessorError::NO_ERROR;

    if (address < flash_start || (address - flash_start) + size > flash_size)
        return AccessorError::OUT_OF_BOUNDS;

    size_t logical     = address - flash_start; // 0-based within flash
    size_t data_offset = 0;

    while (size > 0)
    {
        const size_t block_offset        = logical % BLOCK_SIZE;
        const size_t block_start_logical = logical - block_offset;
        const size_t block_end_logical   = block_start_logical + BLOCK_SIZE;
        const size_t block_remaining     = block_end_logical - logical;
        const size_t done                = std::min(size, block_remaining);

        if (!cache_valid_ || block_start_logical != cache_logical_addr_)
        {
            const size_t abs_block_start = flash_start + block_start_logical;
            AccessorError fill_result    = fill_cache(abs_block_start);
            if (fill_result != AccessorError::NO_ERROR)
                return fill_result;
        }

        std::memcpy(data + data_offset, cache_ + block_offset, done);

        data_offset += done;
        logical     += done;
        size        -= done;
    }

    return AccessorError::NO_ERROR;
}

// Erase via base accessor, ensuring cache state is coherent.
template <typename BaseAccessor, size_t BLOCK_SIZE>
AccessorError BufferedAccessor<BaseAccessor, BLOCK_SIZE>::erase(size_t address)
{
    // Flush pending writes, then invalidate cache since flash changes underneath.
    AccessorError flush_result = flush_cache();
    if (flush_result != AccessorError::NO_ERROR)
        return flush_result;

    cache_valid_ = false;
    cache_dirty_ = false;

    return base_access_.erase(address);
}

template <typename BaseAccessor, size_t BLOCK_SIZE>
void BufferedAccessor<BaseAccessor, BLOCK_SIZE>::format()
{
    flush_cache();
    cache_valid_ = false;
    cache_dirty_ = false;

    if constexpr (requires(BaseAccessor b) { b.format(); })
        base_access_.format();
}

// Concept sanity-check
namespace BUFFERED_ACCESSOR_H
{
    class __MockAccessor__
    {
    public:
        __MockAccessor__() {}

        AccessorError write(size_t /*address*/, const uint8_t* /*data*/, size_t /*size*/)
        {
            return AccessorError::NO_ERROR;
        }
        AccessorError read(size_t /*address*/, uint8_t* /*data*/, size_t /*size*/)
        {
            return AccessorError::NO_ERROR;
        }
        AccessorError erase(size_t /*address*/) { return AccessorError::NO_ERROR; }

        size_t getAlignment() const { return 0; }
        size_t getFlashMemorySize() const { return 0; }
        size_t getFlashStartAddress() const { return 0; }
        size_t getEraseBlockSize() const { return 1; }
    };

    static_assert(Accessor<__MockAccessor__>,
                  "__MockAccessor__ does not satisfy the Accessor concept!");
    static_assert(Accessor<BufferedAccessor<__MockAccessor__, 512>>,
                  "BufferedAccessor does not satisfy the Accessor concept!");
}

#endif // BUFFERED_ACCESSOR_H
