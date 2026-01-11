#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <array>
#include "imagebuffer/BufferedAccessor.hpp"
#include "imagebuffer/accessor.hpp"

// ------------------------------------------------------------
// Mock BaseAccessor for testing BufferedAccessor
// ------------------------------------------------------------
class MockBaseAccessor
{
public:
    size_t writes = 0;
    size_t last_write_addr = 0;
    size_t last_write_size = 0;

    AccessorError write(size_t addr, const uint8_t *data, size_t size)
    {
        writes++;
        last_write_addr = addr;
        last_write_size = size;
        last_write_data.assign(data, data + size);
        return AccessorError::NO_ERROR;
    }

    AccessorError read(size_t /*addr*/, uint8_t *data, size_t size)
    {
        // Simulate erased NAND: all 0xFF
        std::fill(data, data + size, 0xFF);
        return AccessorError::NO_ERROR;
    }

    AccessorError erase(size_t) { return AccessorError::NO_ERROR; }

    size_t getAlignment() const { return 4096; }
    size_t getFlashMemorySize() const { return 1ULL << 20; }
    size_t getFlashStartAddress() const { return 0; }
    size_t getEraseBlockSize() const { return 1; }

    std::vector<uint8_t> last_write_data;
};

static_assert(Accessor<MockBaseAccessor>,
              "MockBaseAccessor must satisfy Accessor concept");

// ------------------------------------------------------------
// Test suite
// ------------------------------------------------------------
TEST_SUITE("BufferedAccessor (unit test, independent of NAND)")
{
    TEST_CASE("Unaligned small writes coalesce into one full block flush")
    {
        MockBaseAccessor base;
        using Buf = BufferedAccessor<MockBaseAccessor, 4096>;
        Buf buf(base);

        std::array<uint8_t, 16> data{};
        std::fill(data.begin(), data.end(), 0xAA);

        // First small write at offset 10
        CHECK(buf.write(10, data.data(), data.size()) == AccessorError::NO_ERROR);

        // No flush yet
        CHECK(base.writes == 0);

        // Second write in same block
        CHECK(buf.write(100, data.data(), data.size()) == AccessorError::NO_ERROR);

        // Now flush
        CHECK(buf.flush_cache() == AccessorError::NO_ERROR);

        // Exactly one full-block write
        CHECK(base.writes == 1);
        CHECK(base.last_write_size == 4096);
        CHECK(base.last_write_addr == 0); // block-aligned
    }

    TEST_CASE("Cross-block write touches three blocks and flushes the last one")
    {
        MockBaseAccessor base;
        using Buf = BufferedAccessor<MockBaseAccessor, 4096>;
        Buf buf(base);

        std::vector<uint8_t> data(4096 + 100, 0xBB);

        CHECK(buf.write(4096 - 50, data.data(), data.size()) == AccessorError::NO_ERROR);

        // Flush the last dirty block
        CHECK(buf.flush_cache() == AccessorError::NO_ERROR);

        // Three blocks were touched (0, 1, 2)
        CHECK(base.writes == 3);

        // The last flushed block is block 2 (address 8192)
        CHECK(base.last_write_addr == 8192);
        CHECK(base.last_write_size == 4096);
    }
}