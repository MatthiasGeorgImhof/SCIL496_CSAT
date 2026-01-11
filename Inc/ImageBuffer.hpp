#ifndef IMAGE_BUFFER_HPP
#define IMAGE_BUFFER_HPP

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <vector>

#include "imagebuffer/accessor.hpp"
#include "imagebuffer/image.hpp"
#include "imagebuffer/storageheader.hpp"
#include "imagebuffer/imagebuffer.hpp"
#include "Checksum.hpp"

template <typename Accessor, typename ChecksumPolicy = DefaultChecksumPolicy>
class ImageBuffer
{
public:
    ImageBuffer(Accessor &accessor) : buffer_state_(0, 0, 0, accessor.getFlashStartAddress(), accessor.getFlashMemorySize()),
                                      accessor_(accessor), next_sequence_id_(0), write_state_{}, read_state_{} {}

    // State queries
    inline bool is_empty() const { return buffer_state_.is_empty(); }
    inline size_t size() const { return buffer_state_.size(); }
    inline size_t count() const { return buffer_state_.count(); }
    inline size_t available() const { return buffer_state_.available(); }
    inline size_t capacity() const { return buffer_state_.capacity(); }
    inline size_t get_head() const { return buffer_state_.get_head(); }
    inline size_t get_tail() const { return buffer_state_.get_tail(); }

    ImageBufferError add_image(const ImageMetadata &meta);
    ImageBufferError add_data_chunk(const uint8_t *data, size_t size) { return ring_io(write_state_, const_cast<uint8_t *>(data), size, true, true); }
    ImageBufferError push_image();
    ImageBufferError get_image(ImageMetadata &meta);
    ImageBufferError get_data_chunk(uint8_t *data, size_t &size);
    ImageBufferError pop_image();
    ImageBufferError initialize_from_flash();

protected:
    void test_set_tail(size_t t) { buffer_state_.tail_ = t; }
    ImageBufferError validate_entry(size_t offset, size_t &entry_size, uint32_t &seq_id, ImageMetadata &meta_out);

private:
    struct EntryState
    {
        size_t offset;
        size_t entry_size;
        size_t consumed;
        size_t payload_size;
    };

    size_t entry_alignment() const
    {
        size_t a = accessor_.getAlignment();
        return a == 0 ? 1 : a;
    }

    size_t align_up(size_t v) const
    {
        size_t a = entry_alignment();
        return (v + a - 1) / a * a;
    }

    ImageBufferError ring_io(EntryState &s, uint8_t *data, size_t size, bool write, bool update_crc)
    {
        if (size == 0)
            return ImageBufferError::NO_ERROR;
        const size_t cap = buffer_state_.TOTAL_BUFFER_CAPACITY_;
        if (size > cap)
            return ImageBufferError::OUT_OF_BOUNDS;

        size_t remaining = size;
        while (remaining > 0)
        {
            size_t chunk = std::min(remaining, cap - s.offset);
            auto phys_addr = s.offset + buffer_state_.FLASH_START_ADDRESS_;
            auto status = write ? accessor_.write(phys_addr, data, chunk) : accessor_.read(phys_addr, data, chunk);

            if (status != AccessorError::NO_ERROR)
                return write ? ImageBufferError::WRITE_ERROR : ImageBufferError::READ_ERROR;
            if (update_crc)
                checksum_.update(data, chunk);

            s.offset = (s.offset + chunk) % cap;
            data += chunk;
            remaining -= chunk;
        }
        s.consumed += size;
        return ImageBufferError::NO_ERROR;
    }

    template <typename T>
    ImageBufferError process_struct(EntryState &s, T &obj, size_t crc_offset, bool write)
    {
        if (write)
        {
            checksum_.reset();
            checksum_.update(reinterpret_cast<uint8_t *>(&obj), crc_offset);
            *reinterpret_cast<crc_t *>(reinterpret_cast<uint8_t *>(&obj) + crc_offset) = checksum_.get();
        }
        auto err = ring_io(s, reinterpret_cast<uint8_t *>(&obj), sizeof(T), write, false);
        if (err != ImageBufferError::NO_ERROR)
            return err;

        if (!write)
        {
            checksum_.reset();
            checksum_.update(reinterpret_cast<uint8_t *>(&obj), crc_offset);
            if (checksum_.get() != *reinterpret_cast<crc_t *>(reinterpret_cast<uint8_t *>(&obj) + crc_offset))
                return ImageBufferError::CHECKSUM_ERROR;
        }
        return ImageBufferError::NO_ERROR;
    }

    ImageBufferError erase_entry_blocks(size_t offset, size_t size)
    {
        const size_t block_sz = accessor_.getEraseBlockSize();
        const size_t cap = buffer_state_.TOTAL_BUFFER_CAPACITY_;
        for (size_t i = 0; i < size; i += block_sz)
        {
            size_t block_addr = ((offset + i) % cap / block_sz) * block_sz;
            if (accessor_.erase(static_cast<uint32_t>(buffer_state_.FLASH_START_ADDRESS_ + block_addr)) != AccessorError::NO_ERROR)
                return ImageBufferError::WRITE_ERROR;
        }
        return ImageBufferError::NO_ERROR;
    }

    void adjust_head(size_t size)
    {
        size_t cap = buffer_state_.TOTAL_BUFFER_CAPACITY_;

        buffer_state_.size_ -= size;
        buffer_state_.head_ = (buffer_state_.head_ + size) % cap;

        // Align head to next valid entry boundary
        size_t aligned = align_up(buffer_state_.head_);
        if (aligned >= cap)
            aligned -= cap;

        // Reduce size by padding consumed
        size_t pad = (aligned >= buffer_state_.head_)
                         ? (aligned - buffer_state_.head_)
                         : (cap - (buffer_state_.head_ - aligned));

        if (pad <= buffer_state_.size_)
            buffer_state_.size_ -= pad;
        else
            buffer_state_.size_ = 0;

        buffer_state_.head_ = aligned;
        buffer_state_.count_--;
    }

    BufferState buffer_state_;
    Accessor &accessor_;
    ChecksumPolicy checksum_;
    uint32_t next_sequence_id_;
    EntryState write_state_, read_state_;
};

template <typename A, typename C>
ImageBufferError ImageBuffer<A, C>::add_image(const ImageMetadata &meta)
{
    size_t total = sizeof(StorageHeader) +
                   sizeof(ImageMetadata) +
                   meta.payload_size +
                   sizeof(crc_t);

    if (buffer_state_.available() < total)
        return ImageBufferError::FULL_BUFFER;

    size_t tail = buffer_state_.tail_;

    // Align the tail to the accessor’s alignment
    size_t aligned_tail = align_up(tail);

    // If aligning pushes us past capacity, wrap
    if (aligned_tail >= buffer_state_.TOTAL_BUFFER_CAPACITY_)
        aligned_tail -= buffer_state_.TOTAL_BUFFER_CAPACITY_;

    // Check if the aligned position has enough space
    if (buffer_state_.available_from(aligned_tail) < total)
        return ImageBufferError::FULL_BUFFER;

    // Use the aligned position consistently
    buffer_state_.tail_ = aligned_tail;
    write_state_ = {aligned_tail, total, 0, meta.payload_size};

    // Header
    StorageHeader hdr{};
    hdr.magic       = STORAGE_MAGIC;
    hdr.version     = STORAGE_HEADER_VERSION;
    hdr.header_size = static_cast<uint16_t>(sizeof(StorageHeader));
    hdr.sequence_id = next_sequence_id_++;
    hdr.total_size  = static_cast<uint32_t>(total - sizeof(StorageHeader));

    auto err = process_struct(write_state_, hdr,
                              offsetof(StorageHeader, header_crc),
                              true);
    if (err != ImageBufferError::NO_ERROR)
        return err;

    // Metadata
    ImageMetadata m_out = meta;
    m_out.version       = 1;
    m_out.metadata_size = sizeof(ImageMetadata);

    err = process_struct(write_state_, m_out,
                         METADATA_SIZE_WO_CRC,
                         true);

    checksum_.reset();
    return err;
}

template <typename A, typename C>
ImageBufferError ImageBuffer<A, C>::push_image()
{
    crc_t tag = checksum_.get();

    auto err = ring_io(write_state_,
                       reinterpret_cast<uint8_t *>(&tag),
                       sizeof(crc_t),
                       true,
                       false);
    if (err != ImageBufferError::NO_ERROR)
        return err;

    buffer_state_.size_ += write_state_.entry_size;
    buffer_state_.tail_ = write_state_.offset;
    buffer_state_.count_++;

    return ImageBufferError::NO_ERROR;
}

template <typename A, typename C>
ImageBufferError ImageBuffer<A, C>::get_image(ImageMetadata &meta)
{
    if (is_empty())
        return ImageBufferError::EMPTY_BUFFER;
    read_state_ = {buffer_state_.head_, 0, 0, 0};

    StorageHeader hdr{};
    auto err = process_struct(read_state_, hdr, offsetof(StorageHeader, header_crc), false);
    if (err != ImageBufferError::NO_ERROR || hdr.magic != STORAGE_MAGIC)
        return ImageBufferError::CHECKSUM_ERROR;

    read_state_.entry_size = sizeof(StorageHeader) + hdr.total_size;
    err = process_struct(read_state_, meta, METADATA_SIZE_WO_CRC, false);

    read_state_.payload_size = meta.payload_size;
    checksum_.reset();
    return err;
}

template <typename A, typename C>
ImageBufferError ImageBuffer<A, C>::get_data_chunk(uint8_t *data, size_t &size)
{
    size_t overhead = sizeof(StorageHeader) + sizeof(ImageMetadata);
    size_t payload_done = (read_state_.consumed > overhead) ? read_state_.consumed - overhead : 0;
    size = std::min(size, read_state_.payload_size - payload_done);
    return ring_io(read_state_, data, size, false, true);
}

template <typename A, typename C>
ImageBufferError ImageBuffer<A, C>::pop_image()
{
    if (is_empty())
        return ImageBufferError::EMPTY_BUFFER;
    crc_t stored = 0;
    crc_t actual = checksum_.get();
    size_t old_head = buffer_state_.head_, total_sz = read_state_.entry_size;

    if (ring_io(read_state_, reinterpret_cast<uint8_t *>(&stored), sizeof(crc_t), false, false) != ImageBufferError::NO_ERROR)
        return ImageBufferError::READ_ERROR;
    if (stored != actual)
        return ImageBufferError::CHECKSUM_ERROR;

    adjust_head(total_sz);
    return erase_entry_blocks(old_head, total_sz);
}

template <typename A, typename C>
ImageBufferError ImageBuffer<A, C>::initialize_from_flash()
{
    const size_t cap = buffer_state_.TOTAL_BUFFER_CAPACITY_;
    buffer_state_.head_ = buffer_state_.tail_ =
        buffer_state_.size_ = buffer_state_.count_ =
            next_sequence_id_ = 0;

    if (cap == 0)
        return ImageBufferError::NO_ERROR;

    struct Found
    {
        size_t off, sz;
        uint32_t id;
    };
    std::vector<Found> entries;

    size_t scan = 0;
    size_t step = entry_alignment();

    while (scan < cap)
    {
        EntryState temp{scan, 0, 0, 0};
        StorageHeader hdr{};

        auto hdr_err = process_struct(temp, hdr,
                                      offsetof(StorageHeader, header_crc),
                                      false);

        if (hdr_err == ImageBufferError::NO_ERROR &&
            hdr.magic == STORAGE_MAGIC)
        {
            size_t e_size = sizeof(StorageHeader) + hdr.total_size;
            entries.push_back({scan, e_size, hdr.sequence_id});

            scan = align_up(scan + e_size);
            if (scan >= cap)
                scan -= cap;
        }
        else
        {
            scan = (scan + step) % cap;
        }

        if (scan == 0)
            break;
    }

    if (entries.empty())
        return ImageBufferError::NO_ERROR;

    // -------------------------
    // 2) VALIDATION PASS
    // -------------------------
    std::sort(entries.begin(), entries.end(),
              [](auto &a, auto &b)
              { return a.id < b.id; });

    std::vector<Found> good;
    good.reserve(entries.size());

    ImageBufferError first_err = ImageBufferError::NO_ERROR;

    for (auto &e : entries)
    {
        size_t validated_size = 0;
        uint32_t sid = 0;
        ImageMetadata dummy{};

        auto v_err = validate_entry(e.off, validated_size, sid, dummy);
        if (v_err != ImageBufferError::NO_ERROR)
        {
            first_err = v_err;
            break;
        }

        if (validated_size != e.sz)
        {
            first_err = ImageBufferError::DATA_ERROR;
            break;
        }

        if (!good.empty() && e.id != good.back().id + 1)
        {
            first_err = ImageBufferError::CHECKSUM_ERROR;
            break;
        }

        good.push_back(e);
    }

    if (good.empty())
        return first_err;

    // -------------------------
    // 3) COMMIT STATE
    // -------------------------
    buffer_state_.head_ = good.front().off;
    buffer_state_.size_ = 0;

    for (auto &e : good)
    {
        buffer_state_.size_ += e.sz;
        buffer_state_.tail_ = (e.off + e.sz) % cap;
    }

    buffer_state_.count_ = good.size();
    next_sequence_id_ = good.back().id + 1;

    return first_err;
}

template <typename A, typename C>
ImageBufferError ImageBuffer<A, C>::validate_entry(size_t offset,
                                                      size_t &entry_size,
                                                      uint32_t &seq_id,
                                                      ImageMetadata &meta_out)
{
    const size_t cap = buffer_state_.TOTAL_BUFFER_CAPACITY_;
    if (cap == 0)
        return ImageBufferError::CHECKSUM_ERROR;

    // Local per-entry state; do NOT touch read_state_/write_state_.
    EntryState s{offset, sizeof(StorageHeader), sizeof(ImageMetadata), 0};

    // 1) Read and validate StorageHeader
    StorageHeader hdr{};
    auto err = process_struct(s, hdr,
                              offsetof(StorageHeader, header_crc),
                              false /* read */);
    if (err != ImageBufferError::NO_ERROR)
        return err;

    if (hdr.magic != STORAGE_MAGIC)
        return ImageBufferError::CHECKSUM_ERROR;

    // Total entry size from header
    entry_size = sizeof(StorageHeader) + hdr.total_size;
    seq_id = hdr.sequence_id;

    // Sanity: entry_size must not exceed capacity
    if (entry_size > cap)
        return ImageBufferError::CHECKSUM_ERROR;

    // 2) Read and validate ImageMetadata
    err = process_struct(s, meta_out, METADATA_SIZE_WO_CRC, false /* read */);
    if (err != ImageBufferError::NO_ERROR)
        return err;

    const size_t payload_size = meta_out.payload_size;
    s.payload_size = payload_size;

    size_t expected_total =
        sizeof(ImageMetadata) +
        payload_size +
        sizeof(crc_t);

    if (hdr.total_size != expected_total)
        return ImageBufferError::DATA_ERROR;

    // 3) Read payload and accumulate CRC
    checksum_.reset();
    {
        size_t remaining = payload_size;
        uint8_t buf[64]; // small scratch buffer; size can be tuned

        while (remaining > 0)
        {
            size_t chunk = std::min(remaining, sizeof(buf));
            auto io_err = ring_io(s, buf, chunk,
                                  false /* read */,
                                  true /* update_crc */);
            if (io_err != ImageBufferError::NO_ERROR)
                return io_err;
            remaining -= chunk;
        }
    }

    // 4) Read stored trailing CRC tag
    crc_t stored = 0;
    err = ring_io(s,
                  reinterpret_cast<uint8_t *>(&stored),
                  sizeof(stored),
                  false /* read */,
                  false /* no CRC update */);
    if (err != ImageBufferError::NO_ERROR)
        return err;

    // // ---------------------------------------------------------------------
    // // DEBUG INSTRUMENTATION
    // // ---------------------------------------------------------------------
    // printf("\n=== validate_entry DEBUG ===\n");
    // printf("offset (entry start)      = %zu\n", offset);
    // printf("s.offset (after metadata) = %zu\n", s.offset);
    // printf("hdr.total_size            = %u\n", hdr.total_size);
    // printf("payload_size              = %lu\n", payload_size);
    // printf("entry_size                = %zu\n", entry_size);
    // printf("seq_id                    = %u\n", seq_id);

    // Compute where payload SHOULD begin
    size_t expected_payload_offset = offset + sizeof(StorageHeader) + sizeof(ImageMetadata);
    expected_payload_offset %= cap;
    // printf("expected payload ring offset = %zu\n", expected_payload_offset);

    // // Dump first 32 bytes of what was ACTUALLY read for CRC
    // {
    //     printf("payload bytes actually read (first 32): ");
    //     uint8_t dbg[32];
    //     size_t dbg_read = std::min<size_t>(32, payload_size);

    //     // We must manually read from flash using ring_io-like logic
    //     size_t dbg_off = expected_payload_offset;
    //     for (size_t i = 0; i < dbg_read; i++)
    //     {
    //         uint8_t b;
    //         size_t phys = (dbg_off + i) % cap;
    //         accessor_.read(buffer_state_.FLASH_START_ADDRESS_ + phys, &b, 1);
    //         dbg[i] = b;
    //     }

    //     for (size_t i = 0; i < dbg_read; i++)
    //         printf("%02X ", dbg[i]);
    //     printf("\n");
    // }

    // printf("computed CRC = 0x%08X\n", checksum_.get());
    // printf("stored CRC   = 0x%08X\n", stored);
    // printf("=== END DEBUG ===\n\n");
    // ---------------------------------------------------------------------

    // 5) Compare computed vs stored CRC
    crc_t actual = checksum_.get();
    if (actual != stored)
        return ImageBufferError::CHECKSUM_ERROR;

    return ImageBufferError::NO_ERROR;
}

#endif // IMAGE_BUFFER_HPP