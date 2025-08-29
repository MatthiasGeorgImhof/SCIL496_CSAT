#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <memory>
#include <tuple>
#include <vector>
#include <array>
#include <cstring> // For std::memcpy
#include <iostream>
#include <numeric>

#include "TaskRespondWrite.hpp" // Include the header for the class being tested
#include "TaskRequestWrite.hpp" // Include the header for the class being tested
#include "Task.hpp"             // Include necessary headers
#include "cyphal.hpp"
#include "loopard_adapter.hpp" // Or your specific adapter header
#include "RegistrationManager.hpp"
#include "InputOutputStream.hpp"

#include "uavcan/file/Write_1_1.h"
#include "uavcan/primitive/Unstructured_1_0.h"
#include "imagebuffer/image.hpp"

// Mock Buffer implementation
class MockBuffer
{
public:
    MockBuffer() : is_empty_(true), image_size_(0), chunk_size_(0), data_index_(0) {}

    bool is_empty() const { return is_empty_; }

    ImageBufferError get_image(ImageMetadata &metadata)
    {
        if (is_empty_)
            return ImageBufferError::EMPTY_BUFFER;
        metadata = metadata_;
        return ImageBufferError::NO_ERROR;
    }

    ImageBufferError get_data_chunk(uint8_t *data, size_t &size)
    {
        if (is_empty_)
        {
            size = 0;
            return ImageBufferError::EMPTY_BUFFER;
        }

        size_t remaining_data = image_size_ - data_index_;
        size_t chunk_size = std::min(size, remaining_data);

        std::memcpy(data, data_.data() + data_index_, chunk_size);
        data_index_ += chunk_size;
        size = chunk_size;

        if (data_index_ >= image_size_)
        {
            is_empty_ = true;
            data_index_ = 0;
        }

        return ImageBufferError::NO_ERROR;
    }

    ImageBufferError pop_image()
    {
        if (is_empty_)
            return ImageBufferError::EMPTY_BUFFER;
        is_empty_ = true;
        data_index_ = 0;
        return ImageBufferError::NO_ERROR;
    }

    void push_image(const std::vector<uint8_t> &data, const ImageMetadata &metadata)
    {
        is_empty_ = false;
        data_ = data;
        image_size_ = data_.size();
        metadata_ = metadata;
        data_index_ = 0;
    }

    size_t size() const { return image_size_; }

private:
    bool is_empty_;
    size_t image_size_;
    size_t chunk_size_;
    size_t data_index_;
    std::vector<uint8_t> data_;
    ImageMetadata metadata_;
};

static_assert(ImageBufferConcept<MockBuffer>,
              "MockBuffer does not satisfy ImageBufferConcept");


// Mock ImageInputStream with chunksize
template <typename Buffer>
class MockImageInputStream : public ImageInputStream<Buffer>
{
public:
    MockImageInputStream(Buffer &buffer, size_t chunk_size) : ImageInputStream<Buffer>(buffer), chunk_size_(chunk_size) {}

    size_t chunkSize(size_t max_chunk_size) const
    {
        return std::min(chunk_size_, max_chunk_size);
    }

private:
    size_t chunk_size_;
};

// Class to expose protected members for testing
template <typename ImageInputStream, typename... Adapters>
class MockTaskRequestWrite : public TaskRequestWrite<ImageInputStream, Adapters...>
{
public:
    MockTaskRequestWrite(ImageInputStream &source, uint32_t interval, uint32_t tick, CyphalNodeID node_id, CyphalTransferID transfer_id, std::tuple<Adapters...> &adapters)
        : TaskRequestWrite<ImageInputStream, Adapters...>(source, interval, tick, node_id, transfer_id, adapters)
    {
    }

    using TaskRequestWrite<ImageInputStream, Adapters...>::handleTaskImpl;
    using TaskRequestWrite<ImageInputStream, Adapters...>::buffer_;
};

// Class to expose protected members for testing
template <OutputStreamConcept Stream, typename... Adapters>
class MockTaskRespondWrite : public TaskRespondWrite<Stream, Adapters...>
{
public:
    MockTaskRespondWrite(Stream &stream, uint32_t interval, uint32_t tick, std::tuple<Adapters...> &adapters)
        : TaskRespondWrite<Stream, Adapters...>(stream, interval, tick, adapters)
    {
    }

    using TaskRespondWrite<Stream, Adapters...>::handleTaskImpl;
    using TaskRespondWrite<Stream, Adapters...>::buffer_;
};

uavcan_file_Write_Response_1_1 unpackResponse(std::shared_ptr<CyphalTransfer> transfer)
{
    uavcan_file_Write_Response_1_1 data {};
    size_t payload_size = transfer->payload_size;

    int retval = uavcan_file_Write_Response_1_1_deserialize_(&data, static_cast<const uint8_t *>(transfer->payload), &payload_size);
    (void) retval;
    return data;
}

uavcan_file_Write_Request_1_1 unpackRequest(std::shared_ptr<CyphalTransfer> transfer)
{
    uavcan_file_Write_Request_1_1 data {};
    size_t payload_size = transfer->payload_size;

    int retval = uavcan_file_Write_Request_1_1_deserialize_(&data, static_cast<const uint8_t *>(transfer->payload), &payload_size);
    (void) retval;
    return data;
}

void *loopardMemoryAllocate(size_t amount) { return static_cast<void *>(malloc(amount)); };
void loopardMemoryFree(void *pointer) { free(pointer); };


TEST_CASE("TaskRequestWrite - TaskRequestWrite: Handles small write")
{
    // Create adapter
    LoopardAdapter loopard;
    loopard.memory_allocate = loopardMemoryAllocate;
    loopard.memory_free = loopardMemoryFree;
    Cyphal<LoopardAdapter> loopard_cyphal(&loopard);
    loopard_cyphal.setNodeID(11);

    std::tuple<Cyphal<LoopardAdapter>> adapters(loopard_cyphal); // Single adapter

    // Mock Buffer and Stream
    MockBuffer mock_buffer;
    MockImageInputStream<MockBuffer> mock_stream(mock_buffer, 16);
    TrivialOuputStream output;

    // Task parameters
    CyphalNodeID node_id = 42;
    CyphalTransferID transfer_id = 7;
    uint32_t tick = 0;
    uint32_t interval = 1000;

    MockTaskRequestWrite task_request(mock_stream, interval, tick, node_id, transfer_id, adapters);
    MockTaskRespondWrite task_response(output, interval, tick, adapters);

    // Prepare test data and metadata
    std::vector<uint8_t> test_data(24);
    std::iota(test_data.begin(), test_data.end(), 0);
    ImageMetadata metadata;
    metadata.camera_index = 10;
    metadata.timestamp = 0x12345678;
    metadata.image_size = test_data.size();
    metadata.checksum = 0xff0000ff;
    mock_buffer.push_image(test_data, metadata);

    // Initial state
    CHECK(loopard.buffer.size() == 0);
    CHECK(task_request.buffer_.size() == 0);
    CHECK(task_response.buffer_.size() == 0);

    // First Task Execution: Sends the initial request
    task_request.handleTaskImpl(); // Calls Request() and Respond()
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_request.buffer_.size() == 0);

    std::shared_ptr<CyphalTransfer> transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    auto request = unpackRequest(transfer);
    CHECK(request.offset == 0);
    CHECK(strncmp(reinterpret_cast<char *>(request.data.value.elements), "ATMI", 4) == 0);
    loopard.buffer.clear();
    task_response.buffer_.push(transfer);
    task_response.handleTaskImpl();
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_response.buffer_.size() == 0);

     // Second Task Execution: Sends the data
    transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    auto response = unpackResponse(transfer);
    CHECK(response._error.value == uavcan_file_Error_1_0_OK);
    loopard.buffer.clear();
    task_request.buffer_.push(transfer);
    task_request.handleTaskImpl(); // Calls Request() and Respond()
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_request.buffer_.size() == 0);

    transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    request = unpackRequest(transfer);
    CHECK(request.offset == sizeof(ImageMetadata));
    CHECK(request.data.value.count == test_data.size());
    CHECK(strncmp(reinterpret_cast<char *>(request.data.value.elements), reinterpret_cast<char *>(test_data.data()), test_data.size()) == 0);
    loopard.buffer.clear();
    task_response.buffer_.push(transfer);
    task_response.handleTaskImpl();
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_response.buffer_.size() == 0);
   
     // Third Task Execution: termination
    transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    response = unpackResponse(transfer);
    CHECK(response._error.value == uavcan_file_Error_1_0_OK);
    loopard.buffer.clear();
    task_request.buffer_.push(transfer);
    task_request.handleTaskImpl(); // Calls Request() and Respond()
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_request.buffer_.size() == 0);

    transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    request = unpackRequest(transfer);
    CHECK(request.offset == sizeof(ImageMetadata) + test_data.size());
    CHECK(request.data.value.count == 0);
    loopard.buffer.clear();
    task_response.buffer_.push(transfer);
    task_response.handleTaskImpl();
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_response.buffer_.size() == 0);
}

TEST_CASE("TaskRequestWrite - TaskRequestWrite: Handles large write")
{
    // Create adapter
    LoopardAdapter loopard;
    loopard.memory_allocate = loopardMemoryAllocate;
    loopard.memory_free = loopardMemoryFree;
    Cyphal<LoopardAdapter> loopard_cyphal(&loopard);
    loopard_cyphal.setNodeID(11);

    std::tuple<Cyphal<LoopardAdapter>> adapters(loopard_cyphal); // Single adapter

    // Mock Buffer and Stream
    MockBuffer mock_buffer;
    MockImageInputStream<MockBuffer> mock_stream(mock_buffer, 16);
    TrivialOuputStream output;

    // Task parameters
    CyphalNodeID node_id = 42;
    CyphalTransferID transfer_id = 7;
    uint32_t tick = 0;
    uint32_t interval = 1000;

    MockTaskRequestWrite task_request(mock_stream, interval, tick, node_id, transfer_id, adapters);
    MockTaskRespondWrite task_response(output, interval, tick, adapters);

    // Prepare test data and metadata
    std::vector<uint8_t> test_data(400);
    std::iota(test_data.begin(), test_data.end(), 0);
    ImageMetadata metadata;
    metadata.camera_index = 10;
    metadata.timestamp = 0x12345678;
    metadata.image_size = test_data.size();
    metadata.checksum = 0xff0000ff;
    mock_buffer.push_image(test_data, metadata);

    // Initial state
    CHECK(loopard.buffer.size() == 0);
    CHECK(task_request.buffer_.size() == 0);
    CHECK(task_response.buffer_.size() == 0);

    // First Task Execution: Sends the initial request
    task_request.handleTaskImpl(); // Calls Request() and Respond()
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_request.buffer_.size() == 0);

    std::shared_ptr<CyphalTransfer> transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    auto request = unpackRequest(transfer);
    CHECK(request.offset == 0);
    CHECK(strncmp(reinterpret_cast<char *>(request.data.value.elements), "ATMI", 4) == 0);
    loopard.buffer.clear();
    task_response.buffer_.push(transfer);
    task_response.handleTaskImpl();
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_response.buffer_.size() == 0);

     // Second Task Execution: Sends the data
    transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    auto response = unpackResponse(transfer);
    CHECK(response._error.value == uavcan_file_Error_1_0_OK);
    loopard.buffer.clear();
    task_request.buffer_.push(transfer);
    task_request.handleTaskImpl(); // Calls Request() and Respond()
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_request.buffer_.size() == 0);

    transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    request = unpackRequest(transfer);
    CHECK(request.offset == sizeof(ImageMetadata));
    CHECK(request.data.value.count == uavcan_primitive_Unstructured_1_0_value_ARRAY_CAPACITY_);
    CHECK(strncmp(reinterpret_cast<char *>(request.data.value.elements), reinterpret_cast<char *>(test_data.data()), test_data.size()) == 0);
    loopard.buffer.clear();
    task_response.buffer_.push(transfer);
    task_response.handleTaskImpl();
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_response.buffer_.size() == 0);
   
     // Second Task Execution: Sends the data second part
    transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    response = unpackResponse(transfer);
    CHECK(response._error.value == uavcan_file_Error_1_0_OK);
    loopard.buffer.clear();
    task_request.buffer_.push(transfer);
    task_request.handleTaskImpl(); // Calls Request() and Respond()
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_request.buffer_.size() == 0);

    transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    request = unpackRequest(transfer);
    CHECK(request.offset == sizeof(ImageMetadata) + uavcan_primitive_Unstructured_1_0_value_ARRAY_CAPACITY_);
    CHECK(request.data.value.count == test_data.size()-uavcan_primitive_Unstructured_1_0_value_ARRAY_CAPACITY_);
    CHECK(strncmp(reinterpret_cast<char *>(request.data.value.elements), reinterpret_cast<char *>(test_data.data()), test_data.size()) == 0);
    loopard.buffer.clear();
    task_response.buffer_.push(transfer);
    task_response.handleTaskImpl();
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_response.buffer_.size() == 0);
   
     // Third Task Execution: termination
    transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    response = unpackResponse(transfer);
    CHECK(response._error.value == uavcan_file_Error_1_0_OK);
    loopard.buffer.clear();
    task_request.buffer_.push(transfer);
    task_request.handleTaskImpl(); // Calls Request() and Respond()
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_request.buffer_.size() == 0);

    transfer = std::make_shared<CyphalTransfer>(loopard.buffer.pop());
    request = unpackRequest(transfer);
    CHECK(request.offset == sizeof(ImageMetadata) + test_data.size());
    CHECK(request.data.value.count == 0);
    loopard.buffer.clear();
    task_response.buffer_.push(transfer);
    task_response.handleTaskImpl();
    CHECK(loopard.buffer.size() == 1);
    CHECK(task_response.buffer_.size() == 0);
}

