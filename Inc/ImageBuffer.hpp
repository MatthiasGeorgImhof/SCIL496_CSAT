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
    ImageBuffer(Accessor &accessor) : buffer_state_(0, 0, 0, accessor.getFlashStartAddress(), accessor.getFlashMemorySize()),
                                      accessor_(accessor), next_sequence_id_(0), current_offset_(0), current_entry_size_(0), current_entry_consumed_(0), current_payload_size_(0)
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

    // State queries
    inline bool is_empty() const { return buffer_state_.is_empty(); }
    inline size_t size() const { return buffer_state_.size(); }
    inline size_t count() const { return buffer_state_.count(); }
    inline size_t available() const { return buffer_state_.available(); }
    inline size_t capacity() const { return buffer_state_.capacity(); }
    inline size_t get_head() const { return buffer_state_.get_head(); }
    inline size_t get_tail() const { return buffer_state_.get_tail(); }

private:
    bool has_enough_space(size_t bytes) const
    {
        return buffer_state_.available() >= bytes;
    }

    void wrap(size_t &addr)
    {
        if (addr >= buffer_state_.TOTAL_BUFFER_CAPACITY_)
            addr -= buffer_state_.TOTAL_BUFFER_CAPACITY_;
    }

    ImageBufferError write(size_t address, const uint8_t *data, size_t size)
    {
        const size_t capacity = buffer_state_.TOTAL_BUFFER_CAPACITY_;

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

            addr = (addr + chunk == capacity) ? 0 : addr + chunk;
            ptr += chunk;
            remaining -= chunk;
        }

        return ImageBufferError::NO_ERROR;
    }

    ImageBufferError read(size_t address, uint8_t *data, size_t size)
    {
        const size_t capacity = buffer_state_.TOTAL_BUFFER_CAPACITY_;

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

            addr = (addr + chunk == capacity) ? 0 : addr + chunk;
            ptr += chunk;
            remaining -= chunk;
        }

        return ImageBufferError::NO_ERROR;
    }

private:
    BufferState buffer_state_;
    Accessor &accessor_;
    ChecksumPolicy checksum_;

    uint32_t next_sequence_id_;

    size_t current_offset_;
    size_t current_entry_size_;
    size_t current_entry_consumed_;
    size_t current_payload_size_;
};

// -----------------------------------------------------------------------------
// add_image
// -----------------------------------------------------------------------------
template <typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError
ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::add_image(const ImageMetadata &metadata_in)
{
    const size_t entry_size =
        sizeof(StorageHeader) +
        sizeof(ImageMetadata) +
        metadata_in.image_size +
        sizeof(crc_t);

    if (!has_enough_space(entry_size))
        return ImageBufferError::FULL_BUFFER;

    // Apply alignment policy
    size_t tail = buffer_state_.tail_;
    if (!AlignmentPolicy::align(tail,
                                accessor_,
                                entry_size,
                                buffer_state_.TOTAL_BUFFER_CAPACITY_))
    {
        return ImageBufferError::FULL_BUFFER;
    }

    buffer_state_.tail_ = tail;
    current_offset_ = tail;
    current_entry_size_ = entry_size;
    current_entry_consumed_ = 0;

    // -------- StorageHeader --------
    StorageHeader hdr{};
    hdr.magic = STORAGE_MAGIC;
    hdr.version = STORAGE_HEADER_VERSION;
    hdr.header_size = sizeof(StorageHeader);
    hdr.sequence_id = next_sequence_id_++;
    hdr.total_size = static_cast<uint32_t>(
        sizeof(ImageMetadata) + metadata_in.image_size + sizeof(crc_t));
    hdr.flags = 0;
    std::memset(hdr.reserved, 0, sizeof(hdr.reserved));

    checksum_.reset();
    checksum_.update(reinterpret_cast<const uint8_t *>(&hdr),
                     offsetof(StorageHeader, header_crc));
    hdr.header_crc = checksum_.get();

    ImageBufferError status =
        write(current_offset_,
              reinterpret_cast<const uint8_t *>(&hdr),
              sizeof(StorageHeader));
    if (status != ImageBufferError::NO_ERROR)
        return status;

    current_offset_ += sizeof(StorageHeader);
    current_entry_consumed_ += sizeof(StorageHeader);
    wrap(current_offset_);

    // -------- ImageMetadata --------
    ImageMetadata meta = metadata_in;
    meta.version = 1;
    meta.metadata_size = sizeof(ImageMetadata);

    checksum_.reset();
    checksum_.update(reinterpret_cast<const uint8_t *>(&meta),
                     METADATA_SIZE_WO_CRC);
    meta.meta_crc = checksum_.get();

    status = write(current_offset_,
                   reinterpret_cast<const uint8_t *>(&meta),
                   sizeof(ImageMetadata));
    if (status != ImageBufferError::NO_ERROR)
        return status;

    current_offset_ += sizeof(ImageMetadata);
    current_entry_consumed_ += sizeof(ImageMetadata);
    wrap(current_offset_);

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
    checksum_.update(data, size);

    ImageBufferError status = write(current_offset_, data, size);
    if (status != ImageBufferError::NO_ERROR)
        return status;

    current_offset_ += size;
    current_entry_consumed_ += size;
    wrap(current_offset_);

    return ImageBufferError::NO_ERROR;
}

// -----------------------------------------------------------------------------
// push_image
// -----------------------------------------------------------------------------
template <typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError
ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::push_image()
{
    const crc_t data_crc = checksum_.get();

    ImageBufferError status =
        write(current_offset_,
              reinterpret_cast<const uint8_t *>(&data_crc),
              sizeof(crc_t));
    if (status != ImageBufferError::NO_ERROR)
        return status;

    current_offset_ += sizeof(crc_t);
    current_entry_consumed_ += sizeof(crc_t);
    wrap(current_offset_);

    if (current_entry_consumed_ != current_entry_size_)
        return ImageBufferError::WRITE_ERROR;

    buffer_state_.size_ += current_entry_size_;
    buffer_state_.tail_ = current_offset_;
    wrap(buffer_state_.tail_);
    buffer_state_.count_++;

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

    const size_t entry_start = buffer_state_.head_;
    current_offset_ = entry_start;
    current_entry_consumed_ = 0;

    // -------- StorageHeader --------
    StorageHeader hdr{};
    ImageBufferError status =
        read(current_offset_,
             reinterpret_cast<uint8_t *>(&hdr),
             sizeof(StorageHeader));
    if (status != ImageBufferError::NO_ERROR)
        return status;

    if (hdr.magic != STORAGE_MAGIC)
        return ImageBufferError::CHECKSUM_ERROR;

    checksum_.reset();
    checksum_.update(reinterpret_cast<const uint8_t *>(&hdr),
                     offsetof(StorageHeader, header_crc));
    if (checksum_.get() != hdr.header_crc)
        return ImageBufferError::CHECKSUM_ERROR;

    current_offset_ += sizeof(StorageHeader);
    current_entry_consumed_ += sizeof(StorageHeader);
    wrap(current_offset_);

    current_entry_size_ = sizeof(StorageHeader) + hdr.total_size;

    // -------- ImageMetadata --------
    status = read(current_offset_,
                  reinterpret_cast<uint8_t *>(&metadata),
                  sizeof(ImageMetadata));
    if (status != ImageBufferError::NO_ERROR)
        return status;

    checksum_.reset();
    checksum_.update(reinterpret_cast<const uint8_t *>(&metadata),
                     METADATA_SIZE_WO_CRC);
    if (checksum_.get() != metadata.meta_crc)
        return ImageBufferError::CHECKSUM_ERROR;

    current_offset_ += sizeof(ImageMetadata);
    current_entry_consumed_ += sizeof(ImageMetadata);
    wrap(current_offset_);

    checksum_.reset(); // prepare for payload
    current_payload_size_ = metadata.image_size;

    return ImageBufferError::NO_ERROR;
}

// -----------------------------------------------------------------------------
// get_data_chunk
// -----------------------------------------------------------------------------
template<typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::get_data_chunk(uint8_t* data, size_t& size)
{
    // We expect that current_entry_consumed_ is at least header + metadata
    const size_t header_and_meta =
        sizeof(StorageHeader) + sizeof(ImageMetadata);

    if (current_entry_consumed_ < header_and_meta)
        return ImageBufferError::READ_ERROR; // logic bug

    // How many bytes of payload have we consumed so far?
    const size_t payload_consumed =
        current_entry_consumed_ - header_and_meta;

    if (payload_consumed >= current_payload_size_)
    {
        size = 0; // no payload left; caller should now call pop_image()
        return ImageBufferError::NO_ERROR;
    }

    // Remaining payload bytes (do not include CRC)
    const size_t remaining_payload = current_payload_size_ - payload_consumed;

    if (size > remaining_payload)
        size = remaining_payload;

    if (size == 0)
        return ImageBufferError::NO_ERROR;

    ImageBufferError status = read(current_offset_, data, size);
    if (status != ImageBufferError::NO_ERROR)
        return status;

    // CRC is only over payload bytes
    checksum_.update(data, size);

    current_offset_         += size;
    current_entry_consumed_ += size;
    wrap(current_offset_);

    return ImageBufferError::NO_ERROR;
}

// -----------------------------------------------------------------------------
// pop_image
// -----------------------------------------------------------------------------
template<typename Accessor, typename AlignmentPolicy, typename ChecksumPolicy>
ImageBufferError ImageBuffer<Accessor, AlignmentPolicy, ChecksumPolicy>::pop_image()
{
    // At this point, caller should have drained all payload via get_data_chunk.
    // current_offset_ should point at the CRC field.

    crc_t stored_crc = 0;
    ImageBufferError status =
        read(current_offset_,
             reinterpret_cast<uint8_t*>(&stored_crc),
             sizeof(crc_t));
    if (status != ImageBufferError::NO_ERROR)
        return status;

    if (stored_crc != checksum_.get())
        return ImageBufferError::CHECKSUM_ERROR;

    current_offset_         += sizeof(crc_t);
    current_entry_consumed_ += sizeof(crc_t);
    wrap(current_offset_);

    if (current_entry_consumed_ != current_entry_size_)
        return ImageBufferError::READ_ERROR;

    buffer_state_.size_ -= current_entry_size_;
    buffer_state_.head_  = current_offset_;
    wrap(buffer_state_.head_);
    buffer_state_.count_--;

    return ImageBufferError::NO_ERROR;
}

#endif // IMAGE_BUFFER_HPP
