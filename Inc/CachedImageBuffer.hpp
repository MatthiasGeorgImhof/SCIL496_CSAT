#ifndef CACHED_IMAGE_BUFFER_H
#define CACHED_IMAGE_BUFFER_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring>
#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>
#include "imagebuffer/image.hpp"
#include "imagebuffer/imagebuffer.hpp"
#include "Checksum.hpp"

typedef ImageBufferError CachedImageBufferError;
typedef BufferState CachedBufferState;

template <typename Accessor>
class CachedImageBuffer
{
public:
    CachedImageBuffer(Accessor &access)
        : buffer_state_(0, 0, 0, access.getFlashStartAddress(), access.getFlashMemorySize()), access_(access) {}

    CachedImageBufferError add_image(const ImageMetadata &metadata);
    CachedImageBufferError add_data_chunk(const uint8_t *data, size_t size);
    CachedImageBufferError push_image();

    CachedImageBufferError get_image(ImageMetadata &metadata);
    CachedImageBufferError get_data_chunk(uint8_t *data, size_t &size);
    CachedImageBufferError pop_image();

    inline bool is_empty() const { return buffer_state_.is_empty(); };
    inline size_t size() const { return buffer_state_.size(); };
    inline size_t count() const { return buffer_state_.count(); };
    inline size_t available() const { return buffer_state_.available(); };
    inline size_t capacity() const { return buffer_state_.capacity(); };

    inline size_t get_head() const { return buffer_state_.get_head(); }
    inline size_t get_tail() const { return buffer_state_.get_tail(); }

private:
    bool has_enough_space(size_t data_size) const { return (buffer_state_.available() >= data_size); };
    void wrap_around(size_t &address);
    CachedImageBufferError write(size_t address, const uint8_t *data, size_t size);
    CachedImageBufferError read(size_t address, uint8_t *data, size_t &size);

private:
    CachedBufferState buffer_state_;
    Accessor &access_;
    size_t current_offset_;
    size_t current_size_;
    ChecksumCalculator checksum_calculator_;
};

template <typename Accessor>
CachedImageBufferError CachedImageBuffer<Accessor>::write(size_t address, const uint8_t *data, size_t size)
{
    if (address + size <= buffer_state_.TOTAL_BUFFER_CAPACITY_)
    {
        CachedImageBufferError status = static_cast<CachedImageBufferError>(access_.write(address + buffer_state_.FLASH_START_ADDRESS_, data, size));
        return status;
    }

    size_t first_part = buffer_state_.TOTAL_BUFFER_CAPACITY_ - address;
    size_t second_part = size - first_part;
    CachedImageBufferError status = static_cast<CachedImageBufferError>(access_.write(address + buffer_state_.FLASH_START_ADDRESS_, data, first_part));
    if (status != CachedImageBufferError::NO_ERROR)
    {
        return status;
    }
    status = static_cast<CachedImageBufferError>(access_.write(buffer_state_.FLASH_START_ADDRESS_, data + first_part, second_part));
    if (status != CachedImageBufferError::NO_ERROR)
    {
        return status;
    }
    return CachedImageBufferError::NO_ERROR;
}

template <typename Accessor>
CachedImageBufferError CachedImageBuffer<Accessor>::read(size_t address, uint8_t *data, size_t &size)
{
    size = std::min(size, current_size_ - current_offset_);
    if (address + size <= buffer_state_.TOTAL_BUFFER_CAPACITY_)
    {
        CachedImageBufferError status = static_cast<CachedImageBufferError>(access_.read(address + buffer_state_.FLASH_START_ADDRESS_, data, size));
        return status;
    }

    size_t first_part = buffer_state_.TOTAL_BUFFER_CAPACITY_ - address;
    size_t second_part = size - first_part;
    CachedImageBufferError status = static_cast<CachedImageBufferError>(access_.read(address + buffer_state_.FLASH_START_ADDRESS_, data, first_part));
    if (status != CachedImageBufferError::NO_ERROR)
    {
        return status;
    }
    status = static_cast<CachedImageBufferError>(access_.read(buffer_state_.FLASH_START_ADDRESS_, data + first_part, second_part));
    if (status != CachedImageBufferError::NO_ERROR)
    {
        return status;
    }
    return CachedImageBufferError::NO_ERROR;
}

template <typename Accessor>
void CachedImageBuffer<Accessor>::wrap_around(size_t &address)
{
    if (address >= buffer_state_.TOTAL_BUFFER_CAPACITY_)
    {
        address -= buffer_state_.TOTAL_BUFFER_CAPACITY_;
    }
}

template <typename Accessor>
CachedImageBufferError CachedImageBuffer<Accessor>::add_image(const ImageMetadata &metadata)
{
    size_t total_size = sizeof(StorageHeader) + sizeof(ImageMetadata) + metadata.image_size + sizeof(crc_t);

    // Ensure image starts at a page boundary
    size_t align = access_.getAlignment();
    size_t misalignment = buffer_state_.tail_ % align;

    if (misalignment != 0)
    {
        size_t padding = align - misalignment;

        // Check space for padding + image
        if (!has_enough_space(padding + total_size))
            return CachedImageBufferError::FULL_BUFFER;

        // Write padding bytes (0xFF is NAND erase state)
        std::vector<uint8_t> pad(padding, 0xFF);
        if (write(buffer_state_.tail_, pad.data(), padding) != CachedImageBufferError::NO_ERROR)
            return CachedImageBufferError::WRITE_ERROR;

        buffer_state_.tail_ += padding;
        wrap_around(buffer_state_.tail_);
    }

    // Now tail_ is page-aligned
    current_offset_ = buffer_state_.tail_;

    // Build StorageHeader
    StorageHeader hdr{};
    hdr.magic = STORAGE_MAGIC;
    hdr.version = STORAGE_HEADER_VERSION;
    hdr.header_size = sizeof(StorageHeader);
    hdr.sequence_id = next_sequence_id_++; // You add this member to the class
    hdr.total_size = static_cast<uint32_t>(
        sizeof(ImageMetadata) + metadata.image_size + sizeof(crc_t));
    hdr.flags = 0;
    std::memset(hdr.reserved, 0, sizeof(hdr.reserved));

    // Compute header CRC
    checksum_calculator_.reset();
    checksum_calculator_.update(
        reinterpret_cast<const uint8_t *>(&hdr),
        offsetof(StorageHeader, header_crc));
    hdr.header_crc = checksum_calculator_.get_checksum();

    // Write StorageHeader
    if (write(buffer_state_.tail_,
              reinterpret_cast<const uint8_t *>(&hdr),
              sizeof(StorageHeader)) != NO_ERROR)
    {
        return WRITE_ERROR;
    }

    current_offset_ = buffer_state_.tail_ + sizeof(StorageHeader);
    wrap_around(current_offset_);

    // Compute metadata checksum
    checksum_calculator_.reset();
    ImageMetadata meta_copy = metadata;
    meta_copy.version = 1;
    meta_copy.metadata_size = static_cast<uint16_t>(sizeof(ImageMetadata));

    checksum_calculator_.reset();
    checksum_calculator_.update(
        reinterpret_cast<const uint8_t *>(&meta_copy),
        METADATA_SIZE_WO_CRC);
    meta_copy.meta_crc = checksum_calculator_.get_checksum();

    // Write metadata
    if (write(buffer_state_.tail_,
              reinterpret_cast<const uint8_t *>(&meta_copy),
              METADATA_SIZE) != CachedImageBufferError::NO_ERROR)
    {
        return CachedImageBufferError::WRITE_ERROR;
    }

    checksum_calculator_.reset();
    current_offset_ += METADATA_SIZE;
    wrap_around(current_offset_);

    return CachedImageBufferError::NO_ERROR;
}

template <typename Accessor>
CachedImageBufferError CachedImageBuffer<Accessor>::add_data_chunk(const uint8_t *data, size_t size)
{
    checksum_calculator_.update(data, size);

    if (write(current_offset_, data, size) != CachedImageBufferError::NO_ERROR)
    {
        return CachedImageBufferError::WRITE_ERROR;
    }

    current_offset_ += size;
    wrap_around(current_offset_);
    return CachedImageBufferError::NO_ERROR;
}

template <typename Accessor>
CachedImageBufferError CachedImageBuffer<Accessor>::push_image()
{
    crc_t data_crc = checksum_calculator_.get_checksum();

    if (write(current_offset_,
              reinterpret_cast<const uint8_t *>(&data_crc),
              sizeof(crc_t)) != NO_ERROR)
    {
        return WRITE_ERROR;
    }

    current_offset_ += sizeof(crc_t);
    wrap_around(current_offset_);

    // Compute total entry size using StorageHeader.total_size
    size_t entry_size = sizeof(StorageHeader) + hdr.total_size;

    buffer_state_.size_ += entry_size;
    buffer_state_.tail_ = current_offset_;
    wrap_around(buffer_state_.tail_);
    buffer_state_.count_++;

    return NO_ERROR;
}

template <typename Accessor>
CachedImageBufferError CachedImageBuffer<Accessor>::get_image(ImageMetadata &metadata)
{
    if (is_empty())
    {
        return CachedImageBufferError::EMPTY_BUFFER;
    }

    current_offset_ = buffer_state_.head_;
    StorageHeader hdr{};
    size_t hdr_size = sizeof(StorageHeader);

    if (read(buffer_state_.head_,
             reinterpret_cast<uint8_t *>(&hdr),
             hdr_size) != NO_ERROR)
    {
        return READ_ERROR;
    }

    // Validate magic
    if (hdr.magic != STORAGE_MAGIC)
        return CHECKSUM_ERROR;

    // Validate header CRC
    checksum_calculator_.reset();
    checksum_calculator_.update(
        reinterpret_cast<const uint8_t *>(&hdr),
        offsetof(StorageHeader, header_crc));
    crc_t hdr_crc = checksum_calculator_.get_checksum();

    if (hdr_crc != hdr.header_crc)
        return CHECKSUM_ERROR;

    current_size_ = METADATA_SIZE;
    size_t size = METADATA_SIZE;
    if (read(current_offset_, reinterpret_cast<uint8_t *>(&metadata), size) != CachedImageBufferError::NO_ERROR)
    {
        return CachedImageBufferError::READ_ERROR;
    }
    if (size != METADATA_SIZE)
    {
        return CachedImageBufferError::READ_ERROR;
    }

    checksum_calculator_.reset();
    checksum_calculator_.update(
        reinterpret_cast<const uint8_t *>(&metadata),
        METADATA_SIZE_WO_CRC);
    crc_t crc = checksum_calculator_.get_checksum();
    if (metadata.meta_crc != crc)
        return CachedImageBufferError::CHECKSUM_ERROR;

    checksum_calculator_.reset();
    current_offset_ += METADATA_SIZE;
    current_size_ += metadata.image_size;
    wrap_around(current_offset_);
    return CachedImageBufferError::NO_ERROR;
}

template <typename Accessor>
CachedImageBufferError CachedImageBuffer<Accessor>::get_data_chunk(uint8_t *data, size_t &size)
{
    if (read(current_offset_, data, size) != CachedImageBufferError::NO_ERROR)
    {
        return CachedImageBufferError::READ_ERROR;
    }
    checksum_calculator_.update(data, size);
    current_offset_ += size;
    wrap_around(current_offset_);
    return CachedImageBufferError::NO_ERROR;
}

template <typename Accessor>
CachedImageBufferError CachedImageBuffer<Accessor>::pop_image()
{
    crc_t checksum = 0;
    current_size_ += sizeof(crc_t);
    size_t size = sizeof(crc_t);
    if (read(current_offset_, reinterpret_cast<uint8_t *>(&checksum), size) != CachedImageBufferError::NO_ERROR)
    {
        return CachedImageBufferError::READ_ERROR;
    }
    if (size != sizeof(crc_t))
    {
        return CachedImageBufferError::READ_ERROR;
    }

    crc_t data_checksum = checksum_calculator_.get_checksum();
    if (checksum != data_checksum)
    {
        return CachedImageBufferError::CHECKSUM_ERROR;
    }

    current_offset_ += sizeof(crc_t);
    wrap_around(current_offset_);

    size_t image_size = current_offset_ - buffer_state_.head_;
    if (current_offset_ < buffer_state_.head_)
    {
        image_size = buffer_state_.TOTAL_BUFFER_CAPACITY_ - buffer_state_.head_ + current_offset_;
    }

    buffer_state_.size_ -= image_size;
    buffer_state_.head_ = current_offset_;
    wrap_around(buffer_state_.head_);
    buffer_state_.count_--;

    return CachedImageBufferError::NO_ERROR;
}

#endif // CACHED_IMAGE_BUFFER_H