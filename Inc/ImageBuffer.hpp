#ifndef IMAGE_BUFFER_HPP
#define IMAGE_BUFFER_HPP

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <vector>

#include "imagebuffer/AlignmentPolicy.hpp" // AlignmentPolicy, NoAlignmentPolicy, PageAlignmentPolicy
#include "imagebuffer/accessor.hpp"        // AccessorError
#include "imagebuffer/image.hpp"           // ImageMetadata, crc_t, METADATA_SIZE_WO_CRC
#include "imagebuffer/storageheader.hpp"   // StorageHeader, STORAGE_MAGIC, STORAGE_HEADER_VERSION
#include "imagebuffer/imagebuffer.hpp"     // BufferState, ImageBufferError
#include "Checksum.hpp"

// -----------------------------------------------------------------------------
// ImageBuffer (canonical ring implementation)
// -----------------------------------------------------------------------------
template <
    typename Accessor,
    typename AlignmentPolicy = NoAlignmentPolicy,
    typename ChecksumPolicy = DefaultChecksumPolicy>
class ImageBuffer
{
public:
    ImageBuffer(Accessor &accessor)
        : buffer_state_(0,
                        0,
                        0,
                        accessor.getFlashStartAddress(),
                        accessor.getFlashMemorySize()),
          accessor_(accessor),
          checksum_(),
          next_sequence_id_(0),
          write_state_{0, 0, 0, 0},
          read_state_{0, 0, 0, 0}
    {
    }

    // Write path
    ImageBufferError add_image(const ImageMetadata &metadata);
    ImageBufferError add_data_chunk(const uint8_t *data, size_t size);
    ImageBufferError push_image();

    // Read path
    ImageBufferError get_image(ImageMetadata &metadata);
    ImageBufferError get_data_chunk(uint8_t *data, size_t &size);
    ImageBufferError pop_image();

    // Boot-time reconstruction from flash
    ImageBufferError initialize_from_flash();

    // State queries
    inline bool is_empty() const { return buffer_state_.is_empty(); }
    inline size_t size() const { return buffer_state_.size(); }
    inline size_t count() const { return buffer_state_.count(); }
    inline size_t available() const { return buffer_state_.available(); }
    inline size_t capacity() const { return buffer_state_.capacity(); }
    inline size_t get_head() const { return buffer_state_.get_head(); }
    inline size_t get_tail() const { return buffer_state_.get_tail(); }

private:
    // ---------------------------------------------------------
    // Small state structs
    // ---------------------------------------------------------
    struct EntryState
    {
        size_t offset;       // current ring offset
        size_t entry_size;   // total entry size (header + meta + payload + crc)
        size_t consumed;     // bytes consumed/written so far within this entry
        size_t payload_size; // payload size in bytes (payload_size)
    };

    // ---------------------------------------------------------
    // Basic helpers
    // ---------------------------------------------------------
    bool has_enough_space(size_t bytes) const
    {
        return buffer_state_.available() >= bytes;
    }

    void wrap(size_t &addr)
    {
        if (addr >= buffer_state_.TOTAL_BUFFER_CAPACITY_)
            addr -= buffer_state_.TOTAL_BUFFER_CAPACITY_;
    }

    size_t compute_entry_size(const ImageMetadata &metadata) const
    {
        return sizeof(StorageHeader) +
               sizeof(ImageMetadata) +
               metadata.payload_size +
               sizeof(crc_t);
    }

    // ---------------------------------------------------------
    // Ring I/O (wrap-aware)
    // ---------------------------------------------------------
    ImageBufferError write_ring(size_t address, const uint8_t *data, size_t size)
    {
        const size_t capacity = buffer_state_.TOTAL_BUFFER_CAPACITY_;
        // fprintf(stderr, "write_ring: address=%zu, data=%p, size=%zu\n", address, data, size);

        if (size == 0)
            return ImageBufferError::NO_ERROR;

        if (size > capacity)
            return ImageBufferError::OUT_OF_BOUNDS;

        size_t addr = address;
        size_t remaining = size;
        const uint8_t *ptr = data;

        while (remaining > 0)
        {
            const size_t to_end = capacity - addr;
            const size_t chunk = (remaining <= to_end) ? remaining : to_end;

            auto status = static_cast<ImageBufferError>(
                accessor_.write(addr + buffer_state_.FLASH_START_ADDRESS_,
                                ptr,
                                chunk));
            if (status != ImageBufferError::NO_ERROR)
                return status;

            addr = (addr + chunk == capacity) ? 0U : addr + chunk;
            ptr += chunk;
            remaining -= chunk;
        }

        return ImageBufferError::NO_ERROR;
    }

    ImageBufferError read_ring(size_t address, uint8_t *data, size_t size)
    {
        const size_t capacity = buffer_state_.TOTAL_BUFFER_CAPACITY_;
        // fprintf(stderr, "read_ring: address=%zu, data=%p, size=%zu\n", address, data, size);

        if (size == 0)
            return ImageBufferError::NO_ERROR;

        if (size > capacity)
            return ImageBufferError::OUT_OF_BOUNDS;

        size_t addr = address;
        size_t remaining = size;
        uint8_t *ptr = data;

        while (remaining > 0)
        {
            const size_t to_end = capacity - addr;
            const size_t chunk = (remaining <= to_end) ? remaining : to_end;

            auto status = static_cast<ImageBufferError>(
                accessor_.read(addr + buffer_state_.FLASH_START_ADDRESS_,
                               ptr,
                               chunk));
            if (status != ImageBufferError::NO_ERROR)
                return status;

            addr = (addr + chunk == capacity) ? 0U : addr + chunk;
            ptr += chunk;
            remaining -= chunk;
        }

        return ImageBufferError::NO_ERROR;
    }

    // ---------------------------------------------------------
    // Alignment helper
    // ---------------------------------------------------------
    ImageBufferError align_tail_for_entry(size_t entry_size)
    {
        size_t tail = buffer_state_.tail_;
        if (!AlignmentPolicy::align(tail,
                                    accessor_,
                                    entry_size,
                                    buffer_state_.TOTAL_BUFFER_CAPACITY_))
        {
            return ImageBufferError::FULL_BUFFER;
        }

        buffer_state_.tail_ = tail;
        return ImageBufferError::NO_ERROR;
    }

    // ---------------------------------------------------------
    // StorageHeader helpers
    // ---------------------------------------------------------
    void compute_header_crc(StorageHeader &hdr)
    {
        checksum_.reset();
        checksum_.update(reinterpret_cast<const uint8_t *>(&hdr),
                         offsetof(StorageHeader, header_crc));
        hdr.header_crc = checksum_.get();
    }

    bool validate_header_crc(const StorageHeader &hdr)
    {
        checksum_.reset();
        checksum_.update(reinterpret_cast<const uint8_t *>(&hdr),
                         offsetof(StorageHeader, header_crc));
        return checksum_.get() == hdr.header_crc;
    }

    bool is_erased_header(const StorageHeader &hdr) const
    {
        // Adjust if your erased pattern differs.
        return hdr.magic == 0xFFFFFFFFu;
    }

    ImageBufferError write_header(const ImageMetadata &metadata_in, EntryState &ws)
    {
        StorageHeader hdr{};
        hdr.magic = STORAGE_MAGIC;
        hdr.version = STORAGE_HEADER_VERSION;
        hdr.header_size = static_cast<uint16_t>(sizeof(StorageHeader));
        hdr.sequence_id = next_sequence_id_++;
        hdr.total_size = static_cast<uint32_t>(sizeof(ImageMetadata) + metadata_in.payload_size + sizeof(crc_t));
        hdr.flags = 0;
        std::memset(hdr.reserved, 0, sizeof(hdr.reserved));

        compute_header_crc(hdr);

        ImageBufferError status =
            write_ring(ws.offset,
                       reinterpret_cast<const uint8_t *>(&hdr),
                       sizeof(StorageHeader));
        if (status != ImageBufferError::NO_ERROR)
            return status;

        ws.offset += sizeof(StorageHeader);
        ws.consumed += sizeof(StorageHeader);
        wrap(ws.offset);

        // entry_size was set by caller from compute_entry_size()
        return ImageBufferError::NO_ERROR;
    }

    ImageBufferError read_header(StorageHeader &hdr, EntryState &rs)
    {
        ImageBufferError status =
            read_ring(rs.offset,
                      reinterpret_cast<uint8_t *>(&hdr),
                      sizeof(StorageHeader));
        if (status != ImageBufferError::NO_ERROR)
            return status;

        if (hdr.magic != STORAGE_MAGIC)
            return ImageBufferError::CHECKSUM_ERROR;

        if (!validate_header_crc(hdr))
            return ImageBufferError::CHECKSUM_ERROR;

        rs.offset += sizeof(StorageHeader);
        rs.consumed += sizeof(StorageHeader);
        wrap(rs.offset);

        rs.entry_size = sizeof(StorageHeader) + hdr.total_size;

        return ImageBufferError::NO_ERROR;
    }

    // Erase any erase-blocks that are touched by the popped entry region.
    // entry_offset: logical start offset of the popped entry (old head_)
    // entry_size:   total size of the popped entry (header + metadata + payload + crc)
    ImageBufferError erase_blocks_for_popped_entry(size_t entry_offset,
                                                   size_t entry_size)
    {
        if (entry_size == 0)
            return ImageBufferError::NO_ERROR;

        const size_t ERASE_BLOCK_SIZE = accessor_.getEraseBlockSize();
        const size_t capacity = buffer_state_.TOTAL_BUFFER_CAPACITY_;

        // Normalize entry region into at most two linear segments
        // Segment 1: [entry_offset, seg1_end)
        const size_t seg1_len =
            std::min(entry_size, capacity - entry_offset);
        const size_t seg1_start = entry_offset;
        const size_t seg1_end = seg1_start + seg1_len;

        // Segment 2: optional wrap-around: [0, seg2_end)
        const bool has_seg2 = (entry_size > seg1_len);
        const size_t seg2_start = 0;
        const size_t seg2_end = entry_size - seg1_len;

        auto erase_blocks_in_segment =
            [&](size_t seg_start, size_t seg_end) -> ImageBufferError
        {
            if (seg_start >= seg_end)
                return ImageBufferError::NO_ERROR;

            // Start from the block that contains seg_start
            size_t block_start =
                (seg_start / ERASE_BLOCK_SIZE) * ERASE_BLOCK_SIZE;

            while (block_start < seg_end)
            {
                const size_t block_end = block_start + ERASE_BLOCK_SIZE;

                // NEW: erase any block that overlaps the segment at all.
                //
                // Overlap condition:
                //   [block_start, block_end) overlaps [seg_start, seg_end)
                //   iff block_start < seg_end && block_end > seg_start
                if (block_start < seg_end && block_end > seg_start)
                {
                    const size_t phys_addr =
                        buffer_state_.FLASH_START_ADDRESS_ + block_start;

                    auto err = accessor_.erase(static_cast<uint32_t>(phys_addr));
                    if (err != AccessorError::NO_ERROR)
                        return ImageBufferError::WRITE_ERROR;
                }

                block_start += ERASE_BLOCK_SIZE;
            }

            return ImageBufferError::NO_ERROR;
        };

        // Segment 1
        auto err = erase_blocks_in_segment(seg1_start, seg1_end);
        if (err != ImageBufferError::NO_ERROR)
            return err;

        // Segment 2 (wrap-around)
        if (has_seg2)
            err = erase_blocks_in_segment(seg2_start, seg2_end);

        return err;
    }

    // ---------------------------------------------------------
    // ImageMetadata helpers
    // ---------------------------------------------------------
    void compute_metadata_crc(ImageMetadata &meta)
    {
        checksum_.reset();
        checksum_.update(reinterpret_cast<const uint8_t *>(&meta),
                         METADATA_SIZE_WO_CRC);
        meta.meta_crc = checksum_.get();
    }

    bool validate_metadata_crc(const ImageMetadata &meta)
    {
        checksum_.reset();
        checksum_.update(reinterpret_cast<const uint8_t *>(&meta),
                         METADATA_SIZE_WO_CRC);
        return checksum_.get() == meta.meta_crc;
    }

    ImageBufferError write_metadata(const ImageMetadata &metadata_in, EntryState &ws)
    {
        ImageMetadata meta = metadata_in;
        meta.version = 1;
        meta.metadata_size = sizeof(ImageMetadata);

        compute_metadata_crc(meta);

        ImageBufferError status =
            write_ring(ws.offset,
                       reinterpret_cast<const uint8_t *>(&meta),
                       sizeof(ImageMetadata));
        if (status != ImageBufferError::NO_ERROR)
            return status;

        ws.offset += sizeof(ImageMetadata);
        ws.consumed += sizeof(ImageMetadata);
        wrap(ws.offset);

        // payload_size is managed by caller
        return ImageBufferError::NO_ERROR;
    }

    ImageBufferError read_metadata(ImageMetadata &metadata, EntryState &rs)
    {
        ImageBufferError status =
            read_ring(rs.offset,
                      reinterpret_cast<uint8_t *>(&metadata),
                      sizeof(ImageMetadata));
        if (status != ImageBufferError::NO_ERROR)
            return status;

        if (!validate_metadata_crc(metadata))
            return ImageBufferError::CHECKSUM_ERROR;

        rs.offset += sizeof(ImageMetadata);
        rs.consumed += sizeof(ImageMetadata);
        wrap(rs.offset);

        rs.payload_size = metadata.payload_size;

        // prepare checksum for payload
        checksum_.reset();

        return ImageBufferError::NO_ERROR;
    }

    // ---------------------------------------------------------
    // Payload helpers
    // ---------------------------------------------------------
    ImageBufferError write_payload_chunk(const uint8_t *data, size_t size, EntryState &ws)
    {
        if (size == 0)
            return ImageBufferError::NO_ERROR;

        checksum_.update(data, size);

        ImageBufferError status = write_ring(ws.offset, data, size);
        if (status != ImageBufferError::NO_ERROR)
            return status;

        ws.offset += size;
        ws.consumed += size;
        wrap(ws.offset);

        return ImageBufferError::NO_ERROR;
    }

    ImageBufferError write_payload_crc(EntryState &ws)
    {
        const crc_t data_crc = checksum_.get();

        ImageBufferError status =
            write_ring(ws.offset,
                       reinterpret_cast<const uint8_t *>(&data_crc),
                       sizeof(crc_t));
        if (status != ImageBufferError::NO_ERROR)
            return status;

        ws.offset += sizeof(crc_t);
        ws.consumed += sizeof(crc_t);
        wrap(ws.offset);

        if (ws.consumed != ws.entry_size)
            return ImageBufferError::WRITE_ERROR;

        return ImageBufferError::NO_ERROR;
    }

    ImageBufferError read_payload_chunk(uint8_t *data, size_t &size, EntryState &rs)
    {
        const size_t header_and_meta =
            sizeof(StorageHeader) + sizeof(ImageMetadata);

        if (rs.consumed < header_and_meta)
            return ImageBufferError::READ_ERROR; // logic bug

        const size_t payload_consumed =
            rs.consumed - header_and_meta;

        if (payload_consumed >= rs.payload_size)
        {
            size = 0; // no payload left; caller should now call pop_image()
            return ImageBufferError::NO_ERROR;
        }

        const size_t remaining_payload = rs.payload_size - payload_consumed;

        if (size > remaining_payload)
            size = remaining_payload;

        if (size == 0)
            return ImageBufferError::NO_ERROR;

        ImageBufferError status = read_ring(rs.offset, data, size);
        if (status != ImageBufferError::NO_ERROR)
            return status;

        checksum_.update(data, size);

        rs.offset += size;
        rs.consumed += size;
        wrap(rs.offset);

        return ImageBufferError::NO_ERROR;
    }

    ImageBufferError validate_payload_crc(EntryState &rs)
    {
        crc_t stored_crc = 0;
        ImageBufferError status =
            read_ring(rs.offset,
                      reinterpret_cast<uint8_t *>(&stored_crc),
                      sizeof(crc_t));
        if (status != ImageBufferError::NO_ERROR)
            return status;

        if (stored_crc != checksum_.get())
            return ImageBufferError::CHECKSUM_ERROR;

        rs.offset += sizeof(crc_t);
        rs.consumed += sizeof(crc_t);
        wrap(rs.offset);

        if (rs.consumed != rs.entry_size)
            return ImageBufferError::READ_ERROR;

        return ImageBufferError::NO_ERROR;
    }

    // ---------------------------------------------------------
    // BufferState advancement
    // ---------------------------------------------------------
void align_head_after_pop()
{
    const size_t align = accessor_.getAlignment();
    if (align == 0)
        return;

    const size_t capacity = buffer_state_.TOTAL_BUFFER_CAPACITY_;
    size_t head = buffer_state_.head_;

    const size_t misalignment = head % align;
    if (misalignment == 0)
        return;

    const size_t padding = align - misalignment;
    head += padding;
    if (head >= capacity)
        head -= capacity;

    buffer_state_.head_ = head;
    if (buffer_state_.size_ >= padding)
        buffer_state_.size_ -= padding;
    else
        buffer_state_.size_ = 0; // defensive
}

    void advance_tail(const EntryState &ws)
    {
        buffer_state_.size_ += ws.entry_size;
        buffer_state_.tail_ = ws.offset;
        wrap(buffer_state_.tail_);
        buffer_state_.count_++;
    }

void advance_head(const EntryState &rs)
{
    buffer_state_.size_ -= rs.entry_size;
    buffer_state_.head_ = rs.offset;
    wrap(buffer_state_.head_);
    buffer_state_.count_--;
    align_head_after_pop();
}

private:
    BufferState buffer_state_;
    Accessor &accessor_;
    ChecksumPolicy checksum_;
    uint32_t next_sequence_id_;

    EntryState write_state_;
    EntryState read_state_;
};

// -----------------------------------------------------------------------------
// add_image
// -----------------------------------------------------------------------------
template <typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError
ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::add_image(const ImageMetadata &metadata_in)
{
    const size_t entry_size = compute_entry_size(metadata_in);

    if (!has_enough_space(entry_size))
        return ImageBufferError::FULL_BUFFER;

    ImageBufferError status = align_tail_for_entry(entry_size);
    if (status != ImageBufferError::NO_ERROR)
        return status;

    write_state_.offset = buffer_state_.tail_;
    write_state_.entry_size = entry_size;
    write_state_.consumed = 0;
    write_state_.payload_size = metadata_in.payload_size;

    status = write_header(metadata_in, write_state_);
    if (status != ImageBufferError::NO_ERROR)
        return status;

    status = write_metadata(metadata_in, write_state_);
    if (status != ImageBufferError::NO_ERROR)
        return status;

    // Prepare for payload CRC
    checksum_.reset();

    return ImageBufferError::NO_ERROR;
}

// -----------------------------------------------------------------------------
// add_data_chunk
// -----------------------------------------------------------------------------
template <typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError
ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::add_data_chunk(const uint8_t *data,
                                                                       size_t size)
{
    return write_payload_chunk(data, size, write_state_);
}

// -----------------------------------------------------------------------------
// push_image
// -----------------------------------------------------------------------------
template <typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError
ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::push_image()
{
    ImageBufferError status = write_payload_crc(write_state_);
    if (status != ImageBufferError::NO_ERROR)
        return status;

    advance_tail(write_state_);
    return ImageBufferError::NO_ERROR;
}

// -----------------------------------------------------------------------------
// get_image
// -----------------------------------------------------------------------------
template <typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError
ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::get_image(ImageMetadata &metadata)
{
    if (is_empty())
        return ImageBufferError::EMPTY_BUFFER;

    read_state_.offset = buffer_state_.head_;
    read_state_.entry_size = 0;
    read_state_.consumed = 0;
    read_state_.payload_size = 0;

    StorageHeader hdr{};
    ImageBufferError status = read_header(hdr, read_state_);
    if (status != ImageBufferError::NO_ERROR)
        return status;

    status = read_metadata(metadata, read_state_);
    if (status != ImageBufferError::NO_ERROR)
        return status;

    // read_metadata() already set payload_size and reset checksum_ for payload
    return ImageBufferError::NO_ERROR;
}

// -----------------------------------------------------------------------------
// get_data_chunk
// -----------------------------------------------------------------------------
template <typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError
ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::get_data_chunk(uint8_t *data,
                                                                       size_t &size)
{
    return read_payload_chunk(data, size, read_state_);
}

// -----------------------------------------------------------------------------
// pop_image
// -----------------------------------------------------------------------------
template <typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError
ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::pop_image()
{
    if (is_empty())
        return ImageBufferError::EMPTY_BUFFER;

    // Capture the entry’s logical region before we advance head_
    const size_t entry_offset = buffer_state_.head_;
    const size_t entry_size = read_state_.entry_size;

    ImageBufferError status = validate_payload_crc(read_state_);
    if (status != ImageBufferError::NO_ERROR)
        return status;

    // Advance ring head (logical removal)
    advance_head(read_state_);

    // Now that the entry is logically gone,
    // we can safely erase any fully covered erase-blocks it occupied.
    status = erase_blocks_for_popped_entry(entry_offset, entry_size);
    if (status != ImageBufferError::NO_ERROR)
        return status;

    return ImageBufferError::NO_ERROR;
}

// -----------------------------------------------------------------------------
// initialize_from_flash (robust scan-forward reconstruction)
// -----------------------------------------------------------------------------
template <typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError
ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::initialize_from_flash()
{
    const size_t capacity = buffer_state_.TOTAL_BUFFER_CAPACITY_;

    // Reset state
    buffer_state_.head_  = 0;
    buffer_state_.tail_  = 0;
    buffer_state_.size_  = 0;
    buffer_state_.count_ = 0;
    next_sequence_id_    = 0;

    if (capacity == 0)
        return ImageBufferError::NO_ERROR;

    const size_t align = accessor_.getAlignment();
    const size_t step  = (align == 0 ? 1 : align);

    auto align_up_mod = [capacity, align](size_t x) -> size_t
    {
        if (align <= 1)
            return x % capacity;

        const size_t rem = x % align;
        if (rem == 0)
            return x % capacity;

        x += (align - rem);
        if (x >= capacity)
            x -= capacity;
        return x;
    };

    struct FoundEntry
    {
        size_t   offset;
        size_t   entry_size;
        uint32_t sequence_id;
    };

    std::vector<FoundEntry> entries;
    entries.reserve(16);

    size_t offset              = 0;
    size_t scanned             = 0;
    bool   corruption_detected = false;

    // Expected position of the *next* header if everything is contiguous
    bool   have_expected_next  = false;
    size_t expected_next       = 0;

    while (scanned < capacity)
    {
        StorageHeader hdr{};

        ImageBufferError err =
            read_ring(offset,
                      reinterpret_cast<uint8_t*>(&hdr),
                      sizeof(StorageHeader));
        if (err != ImageBufferError::NO_ERROR)
        {
            offset  = (offset + step) % capacity;
            scanned += step;
            continue;
        }

        if (is_erased_header(hdr))
        {
            offset  = (offset + step) % capacity;
            scanned += step;
            continue;
        }

        if (hdr.magic != STORAGE_MAGIC || !validate_header_crc(hdr))
        {
            offset  = (offset + step) % capacity;
            scanned += step;
            continue;
        }

        const size_t entry_size = sizeof(StorageHeader) + hdr.total_size;
        if (entry_size == 0 || entry_size > capacity)
        {
            // Impossible entry size → treat as corruption
            corruption_detected = true;
            break;
        }

        // If we already saw at least one valid entry, enforce contiguity:
        // the next header must appear exactly where the previous entry
        // says it should end (after accounting for any alignment padding).
        if (have_expected_next)
        {
            if (offset != expected_next)
            {
                corruption_detected = true;
                break;
            }
        }

        // Record this entry
        entries.push_back(FoundEntry{
            .offset      = offset,
            .entry_size  = entry_size,
            .sequence_id = hdr.sequence_id
        });

        // Compute where the *next* header is expected to be:
        // raw_end = offset + entry_size (end of data for this entry)
        // then align that up to the next aligned position.
        size_t raw_end = offset + entry_size;
        expected_next  = align_up_mod(raw_end);
        have_expected_next = true;

        // Advance scan pointer to that aligned position, and
        // account for bytes skipped (entry + padding) in 'scanned'.
        size_t delta;
        if (expected_next >= offset)
            delta = expected_next - offset;
        else
            delta = (capacity - offset) + expected_next;

        offset  = expected_next;
        scanned += delta;
    }

    if (entries.empty())
        return corruption_detected ? ImageBufferError::CHECKSUM_ERROR
                                   : ImageBufferError::NO_ERROR;

    // Sort by sequence ID (the ring can be wrapped)
    std::sort(entries.begin(),
              entries.end(),
              [](const FoundEntry& a, const FoundEntry& b) {
                  return a.sequence_id < b.sequence_id;
              });

    // Head is the earliest sequence entry
    buffer_state_.head_ = entries.front().offset;

    // Tail and used bytes
    size_t tail       = buffer_state_.head_;
    size_t used_bytes = 0;

    for (const auto& e : entries)
    {
        used_bytes += e.entry_size;
        tail += e.entry_size;
        if (tail >= capacity)
            tail -= capacity;
    }

    buffer_state_.tail_  = tail;
    buffer_state_.size_  = used_bytes;
    buffer_state_.count_ = entries.size();
    next_sequence_id_    = entries.back().sequence_id + 1;

    return corruption_detected ? ImageBufferError::CHECKSUM_ERROR
                               : ImageBufferError::NO_ERROR;
}

#endif // IMAGE_BUFFER_HPP
