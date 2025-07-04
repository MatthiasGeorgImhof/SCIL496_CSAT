#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "BlobStore.hpp"
#include <fstream>
#include <cstdio> // for std::remove
#include <array>
#include <iostream>

// Define the blob storage structure
struct BlobStoreDirectory
{
    uint8_t blob1[10]; // Example blob
    uint8_t blob2[12]; // Example blob
};

struct BlobStoreDirectory3x8
{
    uint8_t blob1[8]; // Example blob
    uint8_t blob2[8]; // Example blob
    uint8_t blob3[8]; // Example blob
};

template <typename BlobStruct>
struct MemberInfo
{
    std::string name;
    std::variant<
        decltype(&BlobStruct::blob1),
        decltype(&BlobStruct::blob2)>
        member_ptr;
};

TEST_CASE("FileBlobStoreAccess + BlobStore Tests POINTERS")
{
    const size_t flash_size = 1024;
    const std::string filename = "test_flash.bin";

    {
        std::ofstream outfile(filename, std::ios::binary);
        outfile.seekp(flash_size - 1);
        outfile.write("\0", 1);
    }

    FileBlobStoreAccess file_access(filename, flash_size);
    REQUIRE(file_access.isValid());

    BlobStore<FileBlobStoreAccess, BlobStoreDirectory> blob_store(file_access);

    const uint8_t test_data1[] = {'!', 'T', 'e', 's', 't', 'D', 'a', 't', 'a', '!'};
    constexpr size_t test_data_size1 = sizeof(test_data1);
    const uint8_t test_data2[] = {'A', 'a', 'B', 'b', 'C', 'c', 'D', 'd', 'E', 'e', 'F', 'f'};
    constexpr size_t test_data_size2 = sizeof(test_data2);
    static_assert(sizeof(BlobStoreDirectory::blob1) == test_data_size1);
    static_assert(sizeof(BlobStoreDirectory::blob2) == test_data_size2);

    SUBCASE("Write Blob2")
    {
        size_t size = test_data_size2;
        REQUIRE(blob_store.write_blob(test_data2, size, &BlobStoreDirectory::blob2));
    }

    SUBCASE("Read Blob2")
    {
        uint8_t retrieve_buffer2[sizeof(BlobStoreDirectory::blob2)] = {0};
        size_t size = sizeof(retrieve_buffer2);
        REQUIRE(blob_store.read_blob(retrieve_buffer2, size, &BlobStoreDirectory::blob2));
    }

    SUBCASE("Write and Read Blob1")
    {
        uint8_t retrieve_buffer1[sizeof(BlobStoreDirectory::blob1)] = {0};

        size_t write_size = test_data_size1;
        REQUIRE(blob_store.write_blob(test_data1, write_size, &BlobStoreDirectory::blob1));

        size_t read_size = sizeof(retrieve_buffer1);
        REQUIRE(blob_store.read_blob(retrieve_buffer1, read_size, &BlobStoreDirectory::blob1));
        REQUIRE(write_size == read_size);
        REQUIRE(test_data_size1 == write_size);
        REQUIRE(std::memcmp(retrieve_buffer1, test_data1, test_data_size1) == 0);
    }

    SUBCASE("Write and Read Blob2")
    {
        uint8_t retrieve_buffer2[sizeof(BlobStoreDirectory::blob2)] = {0};

        size_t write_size = test_data_size2;
        REQUIRE(blob_store.write_blob(test_data2, write_size, &BlobStoreDirectory::blob2));

        size_t read_size = sizeof(retrieve_buffer2);
        REQUIRE(blob_store.read_blob(retrieve_buffer2, read_size, &BlobStoreDirectory::blob2));
        REQUIRE(write_size == read_size);
        REQUIRE(test_data_size2 == write_size);
        REQUIRE(std::memcmp(retrieve_buffer2, test_data2, test_data_size2) == 0);
    }

    std::remove(filename.c_str());
}

TEST_CASE("FileBlobStoreAccess + BlobStore Tests: same size blobs POINTER")
{
    const size_t flash_size = 1024;
    const std::string filename = "test_flash.bin";

    {
        std::ofstream outfile(filename, std::ios::binary);
        outfile.seekp(flash_size - 1);
        outfile.write("\0", 1);
    }

    FileBlobStoreAccess file_access(filename, flash_size);
    REQUIRE(file_access.isValid());

    BlobStore<FileBlobStoreAccess, BlobStoreDirectory3x8> blob_store(file_access);

    const uint8_t test_data1[] = {'T', 'e', 's', 't', 'D', 'a', 't', 'a'};
    const uint8_t test_data2[] = {'A', 'a', 'B', 'b', 'C', 'c', 'D', 'D'};
    const uint8_t test_data3[] = {'1', '2', '3', '4', '5', '6', '7', '8'};
    static_assert(sizeof(test_data1) == sizeof(test_data2));
    static_assert(sizeof(test_data1) == sizeof(test_data3));
    constexpr size_t test_data_size = sizeof(test_data1);

    SUBCASE("Write and Read Blobs 1 2 3")
    {
        uint8_t retrieve_buffer1[sizeof(BlobStoreDirectory3x8::blob1)] = {0};
        uint8_t retrieve_buffer2[sizeof(BlobStoreDirectory3x8::blob2)] = {0};
        uint8_t retrieve_buffer3[sizeof(BlobStoreDirectory3x8::blob3)] = {0};

        size_t wsize1 = test_data_size;
        size_t wsize2 = test_data_size;
        size_t wsize3 = test_data_size;
        REQUIRE(blob_store.write_blob(test_data1, wsize1, &BlobStoreDirectory3x8::blob1));
        REQUIRE(blob_store.write_blob(test_data2, wsize2, &BlobStoreDirectory3x8::blob2));
        REQUIRE(blob_store.write_blob(test_data3, wsize3, &BlobStoreDirectory3x8::blob3));
        REQUIRE(wsize1 == wsize2);
        REQUIRE(wsize1 == wsize3);
        REQUIRE(wsize1 == test_data_size);

        size_t rsize1 = sizeof(retrieve_buffer1);
        size_t rsize2 = sizeof(retrieve_buffer2);
        size_t rsize3 = sizeof(retrieve_buffer3);
        REQUIRE(blob_store.read_blob(retrieve_buffer1, rsize1, &BlobStoreDirectory3x8::blob1));
        REQUIRE(blob_store.read_blob(retrieve_buffer2, rsize2, &BlobStoreDirectory3x8::blob2));
        REQUIRE(blob_store.read_blob(retrieve_buffer3, rsize3, &BlobStoreDirectory3x8::blob3));

        REQUIRE(rsize1 == rsize2);
        REQUIRE(rsize1 == rsize3);
        REQUIRE(rsize1 == test_data_size);

        REQUIRE(std::memcmp(retrieve_buffer1, test_data1, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer2, test_data2, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer3, test_data3, test_data_size) == 0);
    }

    SUBCASE("Write and Read Blob 3 2 1")
    {
        uint8_t retrieve_buffer1[sizeof(BlobStoreDirectory3x8::blob1)] = {0};
        uint8_t retrieve_buffer2[sizeof(BlobStoreDirectory3x8::blob2)] = {0};
        uint8_t retrieve_buffer3[sizeof(BlobStoreDirectory3x8::blob3)] = {0};

        size_t wsize1 = test_data_size;
        size_t wsize2 = test_data_size;
        size_t wsize3 = test_data_size;
        REQUIRE(blob_store.write_blob(test_data1, wsize1, &BlobStoreDirectory3x8::blob1));
        REQUIRE(blob_store.write_blob(test_data2, wsize2, &BlobStoreDirectory3x8::blob2));
        REQUIRE(blob_store.write_blob(test_data3, wsize3, &BlobStoreDirectory3x8::blob3));

        size_t rsize1 = sizeof(retrieve_buffer1);
        size_t rsize2 = sizeof(retrieve_buffer2);
        size_t rsize3 = sizeof(retrieve_buffer3);
        REQUIRE(blob_store.read_blob(retrieve_buffer3, rsize1, &BlobStoreDirectory3x8::blob3));
        REQUIRE(blob_store.read_blob(retrieve_buffer2, rsize2, &BlobStoreDirectory3x8::blob2));
        REQUIRE(blob_store.read_blob(retrieve_buffer1, rsize3, &BlobStoreDirectory3x8::blob1));

        REQUIRE(std::memcmp(retrieve_buffer1, test_data1, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer2, test_data2, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer3, test_data3, test_data_size) == 0);
    }

    std::remove(filename.c_str());
}

TEST_CASE("SPIBlobStoreAccess + BlobStore Tests: same size blobs POINTER")
{
    BlobStoreDirectory3x8 store {};
    constexpr size_t flash_size = sizeof(BlobStoreDirectory3x8);
    uint8_t *memory = reinterpret_cast<uint8_t*>(&store);

    SPIBlobStoreAccess memory_access(flash_size, memory);
    REQUIRE(memory_access.isValid());

    BlobStore<SPIBlobStoreAccess, BlobStoreDirectory3x8> blob_store(memory_access);

    const uint8_t test_data1[] = {'T', 'e', 's', 't', 'D', 'a', 't', 'a'};
    const uint8_t test_data2[] = {'A', 'a', 'B', 'b', 'C', 'c', 'D', 'D'};
    const uint8_t test_data3[] = {'1', '2', '3', '4', '5', '6', '7', '8'};
    static_assert(sizeof(test_data1) == sizeof(test_data2));
    static_assert(sizeof(test_data1) == sizeof(test_data3));
    constexpr size_t test_data_size = sizeof(test_data1);

    SUBCASE("Write and Read Blobs 1 2 3")
    {
        uint8_t retrieve_buffer1[test_data_size] = {};
        uint8_t retrieve_buffer2[test_data_size] = {};
        uint8_t retrieve_buffer3[test_data_size] = {};

        size_t wsize1 = test_data_size;
        size_t wsize2 = test_data_size;
        size_t wsize3 = test_data_size;
        REQUIRE(blob_store.write_blob(test_data1, wsize1, &BlobStoreDirectory3x8::blob1));
        REQUIRE(blob_store.write_blob(test_data2, wsize2, &BlobStoreDirectory3x8::blob2));
        REQUIRE(blob_store.write_blob(test_data3, wsize3, &BlobStoreDirectory3x8::blob3));

        size_t rsize1 = test_data_size;
        size_t rsize2 = test_data_size;
        size_t rsize3 = test_data_size;
        REQUIRE(blob_store.read_blob(retrieve_buffer1, rsize1, &BlobStoreDirectory3x8::blob1));
        REQUIRE(blob_store.read_blob(retrieve_buffer2, rsize2, &BlobStoreDirectory3x8::blob2));
        REQUIRE(blob_store.read_blob(retrieve_buffer3, rsize3, &BlobStoreDirectory3x8::blob3));

        REQUIRE(std::memcmp(retrieve_buffer1, test_data1, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer2, test_data2, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer3, test_data3, test_data_size) == 0);
    }

    SUBCASE("Write and Read Blobs 3 2 1")
    {
        uint8_t retrieve_buffer1[test_data_size] = {};
        uint8_t retrieve_buffer2[test_data_size] = {};
        uint8_t retrieve_buffer3[test_data_size] = {};

        size_t wsize1 = test_data_size;
        size_t wsize2 = test_data_size;
        size_t wsize3 = test_data_size;
        REQUIRE(blob_store.write_blob(test_data1, wsize1, &BlobStoreDirectory3x8::blob1));
        REQUIRE(blob_store.write_blob(test_data2, wsize2, &BlobStoreDirectory3x8::blob2));
        REQUIRE(blob_store.write_blob(test_data3, wsize3, &BlobStoreDirectory3x8::blob3));

        size_t rsize1 = test_data_size;
        size_t rsize2 = test_data_size;
        size_t rsize3 = test_data_size;
        REQUIRE(blob_store.read_blob(retrieve_buffer3, rsize3, &BlobStoreDirectory3x8::blob3));
        REQUIRE(blob_store.read_blob(retrieve_buffer2, rsize2, &BlobStoreDirectory3x8::blob2));
        REQUIRE(blob_store.read_blob(retrieve_buffer1, rsize1, &BlobStoreDirectory3x8::blob1));

        REQUIRE(std::memcmp(retrieve_buffer1, test_data1, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer2, test_data2, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer3, test_data3, test_data_size) == 0);
    }
}

TEST_CASE("NamedBlobStore Tests")
{
    const size_t flash_size = 1024;
    const std::string filename = "test_flash.bin";

    // Create dummy flash file
    {
        std::ofstream outfile(filename, std::ios::binary);
        outfile.seekp(flash_size - 1);
        outfile.write("\0", 1);
    }

    FileBlobStoreAccess file_access(filename, flash_size);
    REQUIRE(file_access.isValid());

    // Define the member map
    static constexpr std::array<MemberInfo<BlobStoreDirectory>, 2> my_blob_map = {
        {
            {"blob1", &BlobStoreDirectory::blob1},
            {"blob2", &BlobStoreDirectory::blob2}
        }
    };

    // Create a NamedBlobStore object, injecting the map
    NamedBlobStore<FileBlobStoreAccess, BlobStoreDirectory, MemberInfo<BlobStoreDirectory>, my_blob_map.size()> named_store(file_access, my_blob_map);

    const uint8_t test_data1[] = {'!', 'T', 'e', 's', 't', 'D', 'a', 't', 'a', '!'};
    constexpr size_t test_data_size1 = sizeof(test_data1);
    const uint8_t test_data2[] = {'A', 'a', 'B', 'b', 'C', 'c', 'D', 'd', 'E', 'e', 'F', 'f'};
    constexpr size_t test_data_size2 = sizeof(test_data2);
    static_assert(sizeof(BlobStoreDirectory::blob1) == test_data_size1);
    static_assert(sizeof(BlobStoreDirectory::blob2) == test_data_size2);

    SUBCASE("Write and Read Blob1 by Name")
    {
        uint8_t retrieve_buffer[sizeof(BlobStoreDirectory::blob1)] = {0};

        size_t wsize = test_data_size1;
        size_t rsize = sizeof(retrieve_buffer);
        REQUIRE(named_store.write_blob_by_name("blob1", test_data1, wsize));
        REQUIRE(named_store.read_blob_by_name("blob1", retrieve_buffer, rsize));
        REQUIRE(std::memcmp(retrieve_buffer, test_data1, test_data_size1) == 0);
    }

    SUBCASE("Write and Read Blob2 by Name")
    {
        uint8_t retrieve_buffer[sizeof(BlobStoreDirectory::blob2)] = {0};

        size_t wsize = test_data_size2;
        size_t rsize = sizeof(retrieve_buffer);
        REQUIRE(named_store.write_blob_by_name("blob2", test_data2, wsize));
        REQUIRE(named_store.read_blob_by_name("blob2", retrieve_buffer, rsize));
        REQUIRE(std::memcmp(retrieve_buffer, test_data2, test_data_size2) == 0);
    }

    SUBCASE("Invalid Blob Name")
    {
        const uint8_t test_data[] = {'X', 'Y', 'Z'};
        size_t wsize = sizeof(test_data);
        REQUIRE_FALSE(named_store.write_blob_by_name("invalid_blob", test_data, wsize));
    }

    // Clean up test file
    std::remove(filename.c_str());
}