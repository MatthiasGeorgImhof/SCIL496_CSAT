#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "BlobStore.hpp"
#include <fstream>
#include <cstdio> // for std::remove
#include <array>
#include <iostream>

// Define the blob storage structure
struct BlobStoreDirectory {
    uint8_t blob1[9];  // Example blob
    uint8_t blob2[8];  // Example blob
};

// Define a struct for the map entries
struct MapEntry {
    std::string name;
    decltype(&BlobStoreDirectory::blob1) member;
};

TEST_CASE("FileBlobStoreAccess + BlobStore Tests") {
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

    BlobStore<FileBlobStoreAccess, BlobStoreDirectory> blob_store(file_access);

    const uint8_t test_data[] = {'T', 'e', 's', 't', 'D', 'a', 't', 'a', '!'};
    constexpr size_t test_data_size = sizeof(test_data);

    SUBCASE("Write and Read Blob1") {
        uint8_t retrieve_buffer[sizeof(BlobStoreDirectory::blob1)] = {0};

        REQUIRE(blob_store.write_blob<&BlobStoreDirectory::blob1>(test_data, test_data_size));
        REQUIRE(blob_store.read_blob<&BlobStoreDirectory::blob1>(retrieve_buffer, sizeof(retrieve_buffer)));
        REQUIRE(std::memcmp(retrieve_buffer, test_data, test_data_size) == 0);
    }

    SUBCASE("Write and Read Blob2") {
        uint8_t retrieve_buffer[sizeof(BlobStoreDirectory::blob2)] = {0};

        REQUIRE(blob_store.write_blob<&BlobStoreDirectory::blob2>(test_data, test_data_size - 1));
        REQUIRE(blob_store.read_blob<&BlobStoreDirectory::blob2>(retrieve_buffer, sizeof(retrieve_buffer)));
        REQUIRE(std::memcmp(retrieve_buffer, test_data, test_data_size - 1) == 0);
    }

    SUBCASE("Check Padding After Partial Write") {
        // Clear flash file again
        {
            std::ofstream clearfile(filename, std::ios::binary | std::ios::trunc);
            clearfile.seekp(flash_size - 1);
            clearfile.write("\0", 1);
        }

        FileBlobStoreAccess file_access(filename, flash_size);
        REQUIRE(file_access.isValid());

        BlobStore<FileBlobStoreAccess, BlobStoreDirectory> clear_blob_store(file_access);

        REQUIRE(clear_blob_store.write_blob<&BlobStoreDirectory::blob1>(test_data, test_data_size - 1));

        // Inspect padded area in the file
        std::ifstream infile(filename, std::ios::binary);
        infile.seekg(offsetof(BlobStoreDirectory, blob1) + test_data_size - 1);

        uint8_t padding_test[1]; // Changed from char to uint8_t
        for (size_t i = test_data_size - 1; i < sizeof(BlobStoreDirectory::blob1); ++i) {
            infile.read(reinterpret_cast<char*>(padding_test), 1);  //Need to cast to char*
            REQUIRE(padding_test[0] == 0xFF); // Compare uint8_t to uint8_t
        }
        infile.close();
    }

    SUBCASE("Reject Write Larger Than Blob") {
        const uint8_t large_data[] = {
            'T','e','s','t','D','a','t','a','!',
            'M','o','r','e','T','e','s','t'
        };
        REQUIRE_FALSE(blob_store.write_blob<&BlobStoreDirectory::blob1>(large_data, sizeof(large_data)));
    }

    // Clean up test file
    std::remove(filename.c_str());
}

struct BlobStoreDirectory3x8 {
    uint8_t blob1[8];  // Example blob
    uint8_t blob2[8];  // Example blob
    uint8_t blob3[8];  // Example blob
};

TEST_CASE("FileBlobStoreAccess + BlobStore Tests: same size blobs") {
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

    BlobStore<FileBlobStoreAccess, BlobStoreDirectory3x8> blob_store(file_access);

    const uint8_t test_data1[] = {'T', 'e', 's', 't', 'D', 'a', 't', 'a'};
    const uint8_t test_data2[] = {'A', 'a', 'B', 'b', 'C', 'c', 'D', 'D'};
    const uint8_t test_data3[] = {'1', '2', '3', '4', '5', '6', '7', '8'};
    static_assert(sizeof(test_data1) == sizeof(test_data2));
    static_assert(sizeof(test_data1) == sizeof(test_data3));
    
    constexpr size_t test_data_size = sizeof(test_data1);

    SUBCASE("Write and Read Blobs 1 2 3") {
        uint8_t retrieve_buffer1[sizeof(BlobStoreDirectory3x8::blob1)] = {0};
        uint8_t retrieve_buffer2[sizeof(BlobStoreDirectory3x8::blob2)] = {0};
        uint8_t retrieve_buffer3[sizeof(BlobStoreDirectory3x8::blob3)] = {0};

        REQUIRE(blob_store.write_blob<&BlobStoreDirectory3x8::blob1>(test_data1, test_data_size));
        REQUIRE(blob_store.write_blob<&BlobStoreDirectory3x8::blob2>(test_data2, test_data_size));
        REQUIRE(blob_store.write_blob<&BlobStoreDirectory3x8::blob3>(test_data3, test_data_size));
        
        
        REQUIRE(blob_store.read_blob<&BlobStoreDirectory3x8::blob1>(retrieve_buffer1, sizeof(retrieve_buffer1)));
        REQUIRE(blob_store.read_blob<&BlobStoreDirectory3x8::blob2>(retrieve_buffer2, sizeof(retrieve_buffer2)));
        REQUIRE(blob_store.read_blob<&BlobStoreDirectory3x8::blob3>(retrieve_buffer3, sizeof(retrieve_buffer3)));
        
        REQUIRE(std::memcmp(retrieve_buffer1, test_data1, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer2, test_data2, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer3, test_data3, test_data_size) == 0);
    }

        SUBCASE("Write and Read Blob 3 2 1") {
        uint8_t retrieve_buffer1[sizeof(BlobStoreDirectory3x8::blob1)] = {0};
        uint8_t retrieve_buffer2[sizeof(BlobStoreDirectory3x8::blob2)] = {0};
        uint8_t retrieve_buffer3[sizeof(BlobStoreDirectory3x8::blob3)] = {0};

        REQUIRE(blob_store.write_blob<&BlobStoreDirectory3x8::blob1>(test_data1, test_data_size));
        REQUIRE(blob_store.write_blob<&BlobStoreDirectory3x8::blob2>(test_data2, test_data_size));
        REQUIRE(blob_store.write_blob<&BlobStoreDirectory3x8::blob3>(test_data3, test_data_size));
        
        
        REQUIRE(blob_store.read_blob<&BlobStoreDirectory3x8::blob3>(retrieve_buffer3, sizeof(retrieve_buffer3)));
        REQUIRE(blob_store.read_blob<&BlobStoreDirectory3x8::blob2>(retrieve_buffer2, sizeof(retrieve_buffer2)));
        REQUIRE(blob_store.read_blob<&BlobStoreDirectory3x8::blob1>(retrieve_buffer1, sizeof(retrieve_buffer1)));
        
        REQUIRE(std::memcmp(retrieve_buffer1, test_data1, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer2, test_data2, test_data_size) == 0);
        REQUIRE(std::memcmp(retrieve_buffer3, test_data3, test_data_size) == 0);
    }

}



TEST_CASE("NamedBlobStore Tests") {
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

    // Define a struct for the map entries
    struct MapEntry {
        std::string name;
        decltype(&BlobStoreDirectory::blob1) member;
    };
    constexpr std::array<MapEntry, 2> my_blob_map = {
        {{"blob1", &BlobStoreDirectory::blob1},
         {"blob2", &BlobStoreDirectory::blob2}}
    };

    // Create a NamedBlobStore object, injecting the map
    NamedBlobStore<FileBlobStoreAccess, BlobStoreDirectory, MapEntry, 2> named_store(file_access, my_blob_map);

    const uint8_t test_data[] = {'T', 'e', 's', 't', 'D', 'a', 't', 'a', '!'};
    constexpr size_t test_data_size = sizeof(test_data);

    SUBCASE("Write and Read Blob1 by Name") {
        uint8_t retrieve_buffer[sizeof(BlobStoreDirectory::blob1)] = {0};

        REQUIRE(named_store.write_blob_by_name("blob1", test_data, test_data_size));

        // Verify the data in file
        std::ifstream infile(filename, std::ios::binary);
        infile.seekg(0); // Assuming blob1 starts at offset 0
        infile.read(reinterpret_cast<char*>(retrieve_buffer), sizeof(retrieve_buffer));
        REQUIRE(std::memcmp(retrieve_buffer, test_data, test_data_size) == 0);
        infile.close();
    }

    SUBCASE("Write and Read Blob2 by Name") {
        uint8_t retrieve_buffer[sizeof(BlobStoreDirectory::blob2)] = {0};

        REQUIRE(named_store.write_blob_by_name("blob2", test_data, test_data_size - 1));

        // Verify the data in file
        std::ifstream infile(filename, std::ios::binary);
        infile.seekg(sizeof(BlobStoreDirectory::blob1)); // Assuming blob2 starts after blob1
        infile.read(reinterpret_cast<char*>(retrieve_buffer), sizeof(retrieve_buffer));
        REQUIRE(std::memcmp(retrieve_buffer, test_data, test_data_size - 1) == 0);
        infile.close();
    }

    SUBCASE("Invalid Blob Name") {
        REQUIRE_FALSE(named_store.write_blob_by_name("invalid_blob", test_data, test_data_size));
    }

    // Clean up test file
    std::remove(filename.c_str());
}