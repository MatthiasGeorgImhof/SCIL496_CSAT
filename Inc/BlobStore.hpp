#ifndef __BLOBSTORE_HPP__
#define __BLOBSTORE_HPP__

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>
#include <bit>
#include <concepts>
#include <array>
#include <variant>
#include <iostream>

// --------------------
// 📐 BlobStoreAccess Concept
// --------------------
template <typename T>
concept BlobStoreAccess = requires(T access, size_t offset, uint8_t *buffer, size_t buffer_size, const uint8_t *const_buffer, size_t const_buffer_size) {
    { access.read(offset, buffer, buffer_size) } -> std::same_as<bool>;
    { access.write(offset, const_buffer, const_buffer_size) } -> std::same_as<bool>;
    { access.get_flash_size() } -> std::convertible_to<size_t>;
};

// ---------------------------
// 💾 File-based Flash Backend
// ---------------------------
class FileBlobStoreAccess
{
public:
    FileBlobStoreAccess(const std::string &filename, size_t flash_size)
        : filename_(filename), flash_size_(flash_size), is_valid_(initialize_flash()) {}

    bool read(size_t offset, uint8_t *buffer, size_t size) const
    {
        if (!is_valid_ || offset + size > flash_size_)
            return false;
        std::ifstream file(filename_, std::ios::binary);
        if (!file)
            return false;
        file.seekg(offset);
        file.read(reinterpret_cast<char *>(buffer), size);
        return file.gcount() == static_cast<std::streamsize>(size);
    }

    bool write(size_t offset, const uint8_t *data, size_t size)
    {
        if (!is_valid_ || offset + size > flash_size_)
            return false;
        std::fstream file(filename_, std::ios::binary | std::ios::in | std::ios::out);
        if (!file)
            return false;
        file.seekp(offset);
        file.write(reinterpret_cast<const char *>(data), size);
        return !file.fail();
    }

    size_t get_flash_size() const { return flash_size_; }
    bool isValid() const { return is_valid_; }

private:
    bool initialize_flash()
    {
        std::ofstream file(filename_, std::ios::binary);
        if (!file)
            return false;
        file.seekp(flash_size_ - 1);
        file.write("\0", 1);
        return !file.fail();
    }

    std::string filename_;
    size_t flash_size_;
    bool is_valid_;
};

// ----------------------------------
// 🔌 RAM-Emulated SPI Flash Backend
// ----------------------------------
class SPIBlobStoreAccess
{
public:
    SPIBlobStoreAccess(size_t flash_size, uint8_t *memory)
        : flash_size_(flash_size), spi_memory_(memory), is_valid_(memory != nullptr)
    {
        if (is_valid_)
            std::memset(spi_memory_, 0xFF, flash_size_);
    }

    bool read(size_t offset, uint8_t *buffer, size_t size) const
    {
        if (!is_valid_ || offset + size > flash_size_)
            return false;
        std::memcpy(buffer, spi_memory_ + offset, size);
        return true;
    }

    bool write(size_t offset, const uint8_t *data, size_t size)
    {
        if (!is_valid_ || offset + size > flash_size_)
            return false;
        std::memcpy(spi_memory_ + offset, data, size);
        return true;
    }

    size_t get_flash_size() const { return flash_size_; }
    bool isValid() const { return is_valid_; }

private:
    size_t flash_size_;
    uint8_t *spi_memory_;
    bool is_valid_;
};

// ------------------------------
// 📦 Generic BlobStore Interface
// ------------------------------
template <BlobStoreAccess AccessType, typename BlobStruct>
class BlobStore
{
public:
    explicit BlobStore(AccessType access) : access_(access) {}

    template <auto MemberPtr>
    bool read_blob(uint8_t *buffer, size_t buffer_size)
    {
        const BlobStruct *obj = nullptr;
        std::size_t offset = reinterpret_cast<std::size_t>(&(obj->*MemberPtr));
        std::size_t size = sizeof(std::remove_reference_t<decltype(obj->*MemberPtr)>);

        if (buffer_size != size)
            return false;

        return access_.read(offset, buffer, buffer_size);
    }

    template <auto MemberPtr>
    bool write_blob(const uint8_t *data, size_t data_size)
    {
        const BlobStruct *obj = nullptr;
        std::size_t offset = reinterpret_cast<std::size_t>(&(obj->*MemberPtr));
        std::size_t size = sizeof(std::remove_reference_t<decltype(obj->*MemberPtr)>);

        if (data_size != size)
            return false;

        return access_.write(offset, data, data_size);
    }

private:
    AccessType access_;
};

// -----------------------------------
// 🏷️ Named BlobStore (with std::array)
// -----------------------------------
template <BlobStoreAccess AccessType, typename BlobStruct, typename MemberInfo, size_t MapSize>
class NamedBlobStore : public BlobStore<AccessType, BlobStruct>
{
public:
    using BlobStore<AccessType, BlobStruct>::BlobStore; // Inherit constructors

    NamedBlobStore(AccessType access, const std::array<MemberInfo, MapSize> &blob_map)
        : BlobStore<AccessType, BlobStruct>(access), blob_map_(blob_map) {}

    bool write_blob_by_name(const std::string &name, const uint8_t *data, size_t size)
    {
        for (const auto &entry : blob_map_)
        {
            if (entry.name == name)
            {
                if (entry.member_ptr.index() == 0)
                {
                    return BlobStore<AccessType, BlobStruct>::template write_blob<&BlobStruct::blob1>(data, size);
                }
                else
                {
                    return BlobStore<AccessType, BlobStruct>::template write_blob<&BlobStruct::blob2>(data, size);
                }
            }
        }
        return false; // Not found
    }

    bool read_blob_by_name(const std::string &name, uint8_t *data, size_t size)
    {
        for (const auto &entry : blob_map_)
        {
            if (entry.name == name)
            {
                if (entry.member_ptr.index() == 0)
                {
                    return BlobStore<AccessType, BlobStruct>::template read_blob<&BlobStruct::blob1>(data, size);
                }
                else
                {
                    return BlobStore<AccessType, BlobStruct>::template read_blob<&BlobStruct::blob2>(data, size);
                }
            }
        }
        return false; // Not found
    }

private:
    const std::array<MemberInfo, MapSize> &blob_map_;
};

#endif // __BLOBSTORE_HPP__