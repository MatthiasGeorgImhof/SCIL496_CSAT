#ifndef __ALIGNMENT_POLICY_HPP__
#define __ALIGNMENT_POLICY_HPP__

#include <concepts>
#include <cstdint>
#include <cstddef>

#include "accessor.hpp"

template<typename Policy, typename Accessor>
concept AlignmentPolicy =
    requires(size_t tail,
             Accessor& acc,
             size_t entry_size,
             size_t capacity)
{
    { Policy::align(tail, acc, entry_size, capacity) } -> std::same_as<bool>;
};

// -----------------------------------------------------------------------------
// No-alignment policy (RAM, file-backed, etc.)
// -----------------------------------------------------------------------------
struct NoAlignmentPolicy
{
    template<typename Accessor>
    static bool align(size_t& /*tail*/,
                      Accessor&,
                      size_t /*entry_size*/,
                      size_t /*capacity*/)
    {
        return true; // no padding
    }
};

// -----------------------------------------------------------------------------
// Page-alignment policy (NAND)
// -----------------------------------------------------------------------------
struct PageAlignmentPolicy
{
    template<typename Accessor>
    static bool align(size_t& tail,
                      Accessor& accessor,
                      size_t entry_size,
                      size_t capacity)
    {
        const size_t align = accessor.getAlignment();
        const size_t misalignment = tail % align;

        if (misalignment == 0)
            return true;

        const size_t padding = align - misalignment;

        if (padding + entry_size > capacity)
            return false;

        std::vector<uint8_t> pad(padding, 0xFF);
        accessor.write(tail + accessor.getFlashStartAddress(), pad.data(), padding);

        tail += padding;
        if (tail >= capacity)
            tail -= capacity;

        return true;
    }
};

struct DummyAccessor
{
    size_t getAlignment() const { return 256; }
    size_t getFlashStartAddress() const { return 0; }
};

static_assert(AlignmentPolicy<NoAlignmentPolicy, DummyAccessor>,
              "NoAlignmentPolicy does not satisfy AlignmentPolicy concept");

static_assert(AlignmentPolicy<PageAlignmentPolicy, DummyAccessor>,
              "PageAlignmentPolicy does not satisfy AlignmentPolicy concept");

#endif // __ALIGNMENT_POLICY_HPP__
