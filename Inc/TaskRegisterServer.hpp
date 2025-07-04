#ifndef __TASKRESGISTERSERVER_HPP_
#define __TASKRESGISTERSERVER_HPP_

#include <string_view>
#include "Task.hpp"
#include "RegistrationManager.hpp"
#include "BlobStore.hpp"
#include "nunavut_assert.h"
#include "uavcan/_register/Access_1_0.h"
#include "uavcan/_register/Name_1_0.h"
#include "uavcan/_register/Value_1_0.h"

template <typename Accessor, typename Dictionary, typename Map, size_t MapSize, typename... Adapters>
class TaskRegisterServer : public TaskForServer<Adapters...>
{
public:
    TaskRegisterServer() = delete;
    TaskRegisterServer(NamedBlobStore<Accessor, Dictionary, Map, MapSize> store, Accessor& accessor, uint32_t interval, uint32_t tick, std::tuple<Adapters...> &adapters)
        : TaskForServer<Adapters...>(interval, tick, adapters), accessor_(accessor), named_store_(store) {}

    virtual void registerTask(RegistrationManager *manager, std::shared_ptr<Task> task) override;
    virtual void unregisterTask(RegistrationManager *manager, std::shared_ptr<Task> task) override;
    virtual void handleTaskImpl() override;

protected:
    Accessor& accessor_;
    NamedBlobStore<Accessor, Dictionary, Map, MapSize> named_store_;

private:
    std::shared_ptr<CyphalTransfer> receiveRequest(uavcan_register_Access_Request_1_0& request_data);
    void processRequest(const uavcan_register_Access_Request_1_0& request_data, uavcan_register_Access_Response_1_0& response_data);
    void answerRequest(uavcan_register_Access_Response_1_0& response_data, const std::shared_ptr<CyphalTransfer>& transfer);

};

// Receiver: Receives and deserializes the request, returns nullptr if not valid
template <typename Accessor, typename Dictionary, typename Map, size_t MapSize, typename... Adapters>
std::shared_ptr<CyphalTransfer> TaskRegisterServer<Accessor, Dictionary, Map, MapSize, Adapters...>::receiveRequest(uavcan_register_Access_Request_1_0& request_data)
{
    if (TaskForServer<Adapters...>::buffer_.is_empty())
        return nullptr;

    std::shared_ptr<CyphalTransfer> transfer = TaskForServer<Adapters...>::buffer_.pop();
    if (transfer->metadata.transfer_kind != CyphalTransferKindRequest)
        return nullptr;

    size_t payload_size = transfer->payload_size;
    int8_t deserialization_result = uavcan_register_Access_Request_1_0_deserialize_(&request_data, static_cast<const uint8_t *>(transfer->payload), &payload_size);
    if (deserialization_result < 0)
        return nullptr;

    return transfer;
}

// Process: Reads from the file and fills the response
template <typename Accessor, typename Dictionary, typename Map, size_t MapSize, typename... Adapters>
void TaskRegisterServer<Accessor, Dictionary, Map, MapSize, Adapters...>::processRequest(const uavcan_register_Access_Request_1_0& request_data, uavcan_register_Access_Response_1_0& response_data)
{
    const std::string_view key(reinterpret_cast<const char*>(request_data.name.name.elements), request_data.name.name.count);
    std::string key_(key);
    if (request_data.value._tag_ == 2U)
    {
        size_t size = request_data.value.unstructured.value.count;
        named_store_.write_blob_by_name(key_, request_data.value.unstructured.value.elements, size);
    }
    
    response_data.timestamp.microsecond = 1234567890;
    response_data._mutable = true;
    response_data.persistent = true;
    response_data.value._tag_ = 2U;
    response_data.value.unstructured.value.count = sizeof(response_data.value.unstructured.value.elements);
    named_store_.read_blob_by_name(key_, response_data.value.unstructured.value.elements, response_data.value.unstructured.value.count);
}


// Answer: Serializes and publishes the response
template <typename Accessor, typename Dictionary, typename Map, size_t MapSize, typename... Adapters>
void TaskRegisterServer<Accessor, Dictionary, Map, MapSize, Adapters...>::answerRequest(uavcan_register_Access_Response_1_0& response_data,
                                                              const std::shared_ptr<CyphalTransfer>& transfer)
{
    constexpr size_t PAYLOAD_SIZE = uavcan_register_Access_Response_1_0_SERIALIZATION_BUFFER_SIZE_BYTES_;
    uint8_t payload[PAYLOAD_SIZE];
    TaskForServer<Adapters...>::publish(PAYLOAD_SIZE, payload, &response_data,
                                         reinterpret_cast<int8_t (*)(const void *const, uint8_t *const, size_t *const)>(uavcan_register_Access_Response_1_0_serialize_),
                                         transfer->metadata.port_id, transfer->metadata.remote_node_id, transfer->metadata.transfer_id);
}

// Main handler: Calls the above three steps
template <typename Accessor, typename Dictionary, typename Map, size_t MapSize, typename... Adapters>
void TaskRegisterServer<Accessor, Dictionary, Map, MapSize, Adapters...>::handleTaskImpl()
{
    uavcan_register_Access_Request_1_0 request_data;
    auto transfer = receiveRequest(request_data);
    if (!transfer)
        return;

    uavcan_register_Access_Response_1_0 response_data = {};
    processRequest(request_data, response_data);
    answerRequest(response_data, transfer);
}

template <typename Accessor, typename Dictionary, typename Map, size_t MapSize, typename... Adapters>
void TaskRegisterServer<Accessor, Dictionary, Map, MapSize, Adapters...>::registerTask(RegistrationManager *manager, std::shared_ptr<Task> task)
{
    manager->server(uavcan_register_Access_1_0_FIXED_PORT_ID_, task);
}

template <typename Accessor, typename Dictionary, typename Map, size_t MapSize, typename... Adapters>
void TaskRegisterServer<Accessor, Dictionary, Map, MapSize, Adapters...>::unregisterTask(RegistrationManager *manager, std::shared_ptr<Task> task)
{
    manager->unserver(uavcan_register_Access_1_0_FIXED_PORT_ID_, task);
}

#endif // __TASKRESGISTERSERVER_HPP_