#ifndef IMAGE_BUFFER_HPP
#define IMAGE_BUFFER_HPP

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cstring>

#include "imagebuffer/image.hpp"        // ImageMetadata, crc_t, METADATA_SIZE_WO_CRC
#include "imagebuffer/storage_header.hpp" // StorageHeader, STORAGE_MAGIC, STORAGE_HEADER_VERSION
#include "imagebuffer/imagebuffer.hpp"  // BufferState, ImageBufferError
#include "Checksum.hpp"

template <typename Accessor>
class ImageBuffer
{
public:
    ImageBuffer(Accessor& accessor)
        : buffer_state_(0,
                        0,
                        0,
                        accessor.getFlashStartAddress(),
                        accessor.getFlashMemorySize())
        , accessor_(accessor)
        , current_offset_(0)
        , current_entry_size_(0)
        , current_entry_consumed_(0)
        , next_sequence_id_(0)
    {}

    ImageBufferError add_image(const ImageMetadata& metadata);
    ImageBufferError add_data_chunk(const uint8_t* data, size_t size);
    ImageBufferError push_image();

    ImageBufferError get_image(ImageMetadata& metadata);
    ImageBufferError get_data_chunk(uint8_t* data, size_t& size);
    ImageBufferError pop_image();

    inline bool   is_empty()   const { return buffer_state_.is_empty(); }
    inline size_t size()       const { return buffer_state_.size(); }
    inline size_t count()      const { return buffer_state_.count(); }
    inline size_t available()  const { return buffer_state_.available(); }
    inline size_t capacity()   const { return buffer_state_.capacity(); }
    inline size_t get_head()   const { return buffer_state_.get_head(); }
    inline size_t get_tail()   const { return buffer_state_.get_tail(); }

private:
    bool has_enough_space(size_t data_size) const
    {
        return buffer_state_.available() >= data_size;
    }

    void wrap_around(size_t& address)
    {
        if (address >= buffer_state_.TOTAL_BUFFER_CAPACITY_)
        {
            address -= buffer_state_.TOTAL_BUFFER_CAPACITY_;
        }
    }

    ImageBufferError write(size_t address, const uint8_t* data, size_t size);
    ImageBufferError read(size_t address, uint8_t* data, size_t size);

private:
    BufferState        buffer_state_;
    Accessor&          accessor_;
    size_t             current_offset_;          // absolute offset in ring for next R/W
    size_t             current_entry_size_;      // total bytes in current entry (from start of header)
    size_t             current_entry_consumed_;  // bytes consumed so far from the entry start (for reads)
    uint32_t           next_sequence_id_;        // monotonic sequence for StorageHeader
    ChecksumCalculator checksum_calculator_;
};

template <typename Accessor>
ImageBufferError ImageBuffer<Accessor>::write(size_t address, const uint8_t* data, size_t size)
{
    // This implementation assumes we never write an entry that needs to wrap
    // across the end of the ring. If needed, we can split into two writes.
    if (address + size > buffer_state_.TOTAL_BUFFER_CAPACITY_)
    {
        return ImageBufferError::OUT_OF_BOUNDS;
    }

    return static_cast<ImageBufferError>(
        accessor_.write(address + buffer_state_.FLASH_START_ADDRESS_, data, size));
}

template <typename Accessor>
ImageBufferError ImageBuffer<Accessor>::read(size_t address, uint8_t* data, size_t size)
{
    if (address + size > buffer_state_.TOTAL_BUFFER_CAPACITY_)
    {
        return ImageBufferError::OUT_OF_BOUNDS;
    }

    return static_cast<ImageBufferError>(
        accessor_.read(address + buffer_state_.FLASH_START_ADDRESS_, data, size));
}

template <typename Accessor>
ImageBufferError ImageBuffer<Accessor>::add_image(const ImageMetadata& metadata_in)
{
    // Total size of the entry on flash: header + metadata + payload + data CRC
    const size_t entry_size =
        sizeof(StorageHeader) +
        sizeof(ImageMetadata) +
        metadata_in.image_size +
        sizeof(crc_t);

    if (!has_enough_space(entry_size))
    {
        return ImageBufferError::FULL_BUFFER;
    }

    // Entry starts at current tail
    const size_t entry_start = buffer_state_.tail_;
    current_offset_          = entry_start;
    current_entry_size_      = entry_size;
    current_entry_consumed_  = 0;

    // -------- StorageHeader --------
    StorageHeader hdr{};
    hdr.magic       = STORAGE_MAGIC;
    hdr.version     = STORAGE_HEADER_VERSION;
    hdr.header_size = static_cast<uint16_t>(sizeof(StorageHeader));
    hdr.sequence_id = next_sequence_id_++;
    hdr.total_size  = static_cast<uint32_t>(
        sizeof(ImageMetadata) + metadata_in.image_size + sizeof(crc_t));
    hdr.flags       = 0;
    std::memset(hdr.reserved, 0, sizeof(hdr.reserved));

    // Compute header CRC
    checksum_calculator_.reset();
    checksum_calculator_.update(
        reinterpret_cast<const uint8_t*>(&hdr),
        offsetof(StorageHeader, header_crc));
    hdr.header_crc = checksum_calculator_.get_checksum();

    // Write StorageHeader
    ImageBufferError status = write(current_offset_,
                                    reinterpret_cast<const uint8_t*>(&hdr),
                                    sizeof(StorageHeader));
    if (status != ImageBufferError::NO_ERROR)
    {
        return status;
    }

    current_offset_         += sizeof(StorageHeader);
    current_entry_consumed_ += sizeof(StorageHeader);
    wrap_around(current_offset_);

    // -------- ImageMetadata --------
    ImageMetadata meta = metadata_in;
    meta.version       = 1;
    meta.metadata_size = static_cast<uint16_t>(sizeof(ImageMetadata));

    checksum_calculator_.reset();
    checksum_calculator_.update(
        reinterpret_cast<const uint8_t*>(&meta),
        METADATA_SIZE_WO_CRC);
    meta.meta_crc = checksum_calculator_.get_checksum();

    status = write(current_offset_,
                   reinterpret_cast<const uint8_t*>(&meta),
                   sizeof(ImageMetadata));
    if (status != ImageBufferError::NO_ERROR)
    {
        return status;
    }

    current_offset_         += sizeof(ImageMetadata);
    current_entry_consumed_ += sizeof(ImageMetadata);
    wrap_around(current_offset_);

    // Prepare for payload CRC
    checksum_calculator_.reset();

    return ImageBufferError::NO_ERROR;
}

template <typename Accessor>
ImageBufferError ImageBuffer<Accessor>::add_data_chunk(const uint8_t* data, size_t size)
{
    // Accumulate CRC over payload only
    checksum_calculator_.update(data, size);

    ImageBufferError status = write(current_offset_, data, size);
    if (status != ImageBufferError::NO_ERROR)
    {
        return status;
    }

    current_offset_         += size;
    current_entry_consumed_ += size;
    wrap_around(current_offset_);

    return ImageBufferError::NO_ERROR;
}

template <typename Accessor>
ImageBufferError ImageBuffer<Accessor>::push_image()
{
    // Finalize and write data CRC
    const crc_t data_crc = checksum_calculator_.get_checksum();

    ImageBufferError status = write(
        current_offset_,
        reinterpret_cast<const uint8_t*>(&data_crc),
        sizeof(crc_t));
    if (status != ImageBufferError::NO_ERROR)
    {
        return status;
    }

    current_offset_         += sizeof(crc_t);
    current_entry_consumed_ += sizeof(crc_t);
    wrap_around(current_offset_);

    // Sanity check: we should have consumed exactly current_entry_size_
    if (current_entry_consumed_ != current_entry_size_)
    {
        // This indicates a logic bug in how we accounted bytes.
        return ImageBufferError::WRITE_ERROR;
    }

    // Advance tail and buffer_state bookkeeping
    buffer_state_.size_ += current_entry_size_;
    buffer_state_.tail_  = current_offset_;
    wrap_around(buffer_state_.tail_);
    buffer_state_.count_++;

    return ImageBufferError::NO_ERROR;
}

template <typename Accessor>
ImageBufferError ImageBuffer<Accessor>::get_image(ImageMetadata& metadata)
{
    if (is_empty())
    {
        return ImageBufferError::EMPTY_BUFFER;
    }

    // Entry starts at head
    const size_t entry_start = buffer_state_.head_;
    current_offset_          = entry_start;
    current_entry_consumed_  = 0;

    // -------- Read and validate StorageHeader --------
    StorageHeader hdr{};
    ImageBufferError status = read(
        current_offset_,
        reinterpret_cast<uint8_t*>(&hdr),
        sizeof(StorageHeader));
    if (status != ImageBufferError::NO_ERROR)
    {
        return status;
    }

    if (hdr.magic != STORAGE_MAGIC)
    {
        return ImageBufferError::CHECKSUM_ERROR;
    }

    checksum_calculator_.reset();
    checksum_calculator_.update(
        reinterpret_cast<const uint8_t*>(&hdr),
        offsetof(StorageHeader, header_crc));
    const crc_t hdr_crc = checksum_calculator_.get_checksum();

    if (hdr_crc != hdr.header_crc)
    {
        return ImageBufferError::CHECKSUM_ERROR;
    }

    current_offset_         += sizeof(StorageHeader);
    current_entry_consumed_ += sizeof(StorageHeader);
    wrap_around(current_offset_);

    // Total entry size from header
    current_entry_size_ = sizeof(StorageHeader) + hdr.total_size;

    // -------- Read and validate ImageMetadata --------
    status = read(
        current_offset_,
        reinterpret_cast<uint8_t*>(&metadata),
        sizeof(ImageMetadata));
    if (status != ImageBufferError::NO_ERROR)
    {
        return status;
    }

    checksum_calculator_.reset();
    checksum_calculator_.update(
        reinterpret_cast<const uint8_t*>(&metadata),
        METADATA_SIZE_WO_CRC);
    const crc_t meta_crc = checksum_calculator_.get_checksum();

    if (meta_crc != metadata.meta_crc)
    {
        return ImageBufferError::CHECKSUM_ERROR;
    }

    current_offset_         += sizeof(ImageMetadata);
    current_entry_consumed_ += sizeof(ImageMetadata);
    wrap_around(current_offset_);

    // Prepare for payload CRC
    checksum_calculator_.reset();

    return ImageBufferError::NO_ERROR;
}

template <typename Accessor>
ImageBufferError ImageBuffer<Accessor>::get_data_chunk(uint8_t* data, size_t& size)
{
    // Remaining bytes of [metadata + payload + data CRC] after what we've consumed.
    const size_t remaining_in_entry =
        (current_entry_size_ > current_entry_consumed_)
            ? (current_entry_size_ - current_entry_consumed_)
            : 0;

    if (remaining_in_entry == 0)
    {
        size = 0;
        return ImageBufferError::NO_ERROR;
    }

    // We should not read past remaining_in_entry; caller's size is an upper bound.
    size = std::min(size, remaining_in_entry);

    ImageBufferError status = read(current_offset_, data, size);
    if (status != ImageBufferError::NO_ERROR)
    {
        return status;
    }

    // We are in the payload region until the last sizeof(crc_t) bytes
    // The caller is expected to stop calling get_data_chunk before we hit the CRC field.
    // So we only update the CRC over payload here.
    checksum_calculator_.update(data, size);

    current_offset_         += size;
    current_entry_consumed_ += size;
    wrap_around(current_offset_);

    return ImageBufferError::NO_ERROR;
}

template <typename Accessor>
ImageBufferError ImageBuffer<Accessor>::pop_image()
{
    // At this point, caller should have read all payload bytes via get_data_chunk
    // current_offset_ should point to the data CRC at the end of the entry.

    // Read stored CRC
    crc_t stored_crc = 0;
    size_t crc_size  = sizeof(crc_t);

    ImageBufferError status = read(
        current_offset_,
        reinterpret_cast<uint8_t*>(&stored_crc),
        crc_size);
    if (status != ImageBufferError::NO_ERROR)
    {
        return status;
    }

    const crc_t computed_crc = checksum_calculator_.get_checksum();
    if (stored_crc != computed_crc)
    {
        return ImageBufferError::CHECKSUM_ERROR;
    }

    current_offset_         += sizeof(crc_t);
    current_entry_consumed_ += sizeof(crc_t);
    wrap_around(current_offset_);

    // Sanity: we must have consumed the whole entry
    if (current_entry_consumed_ != current_entry_size_)
    {
        return ImageBufferError::READ_ERROR;
    }

    // Advance head and update buffer_state
    buffer_state_.size_ -= current_entry_size_;
    buffer_state_.head_  = current_offset_;
    wrap_around(buffer_state_.head_);
    buffer_state_.count_--;

    return ImageBufferError::NO_ERROR;
}

#endif // IMAGE_BUFFER_HPP
