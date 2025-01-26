#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "BoxSet.hpp"
#include "cyphal.hpp"
#include "canard_adapter.hpp"
#include "serard_adapter.hpp"
#include "udpard_adapter.hpp"
#include "loopard_adapter.hpp"
#include <iostream>
#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>

void *canardMemoryAllocate(CanardInstance *ins, size_t amount) { return static_cast<void *>(malloc(amount)); };
void canardMemoryFree(CanardInstance *ins, void *pointer) { free(pointer); };

TEST_CASE("Canard Adapter")
{
    CanardAdapter adapter;
    adapter.ins = canardInit(canardMemoryAllocate, canardMemoryFree);
    adapter.que = canardTxInit(16, CANARD_MTU_CAN_CLASSIC);
    Cyphal<CanardAdapter> cyphal(&adapter);

    CyphalTransferMetadata metadata;
    metadata.priority = CyphalPriorityNominal;
    metadata.transfer_kind = CyphalTransferKindMessage;
    metadata.port_id = 123;
    metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    metadata.transfer_id = 0;

    const char payload[] = "hello";
    size_t payload_size = sizeof(payload);

    SUBCASE("cyphalTxPush")
    {
        CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size, payload) == 1);
    }

    REQUIRE(adapter.subscriptions.size() == 0);
    SUBCASE("cyphalRxSubscribe and Unsubscribe")
    {

        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 42, 100, 2000000) == 1);
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 42, 100, 2000000) == 0);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 42) == 1);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 42) == 0);
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 43, 100, 2000000) == 1);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 43) == 1);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 43) == 0);
    }
    REQUIRE(adapter.subscriptions.size() == 0);

    SUBCASE("cyphalRxSubscribe returns negative on full boxset")
    {
        adapter.subscriptions.clear();
        for (int i = 0; i < CanardAdapter::SUBSCRIPTIONS; ++i)
        {
            CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, i, 100, 2000000) == 1);
        }
        REQUIRE(adapter.subscriptions.is_full());
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, CanardAdapter::SUBSCRIPTIONS + 1, 100, 2000000) == -2);
    }
}

TEST_CASE("Canard Send Receive")
{
    CanardAdapter adapter;
    adapter.ins = canardInit(canardMemoryAllocate, canardMemoryFree);
    adapter.ins.node_id = 11;
    adapter.que = canardTxInit(16, CANARD_MTU_CAN_CLASSIC);
    Cyphal<CanardAdapter> cyphal(&adapter);

    CyphalTransferMetadata metadata;
    metadata.priority = CyphalPriorityNominal;
    metadata.transfer_kind = CyphalTransferKindMessage;
    metadata.port_id = 123;
    metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    metadata.transfer_id = 0;

    const char payload1[] = "hello";
    size_t payload_size1 = sizeof(payload1);
    CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size1, payload1) == 1);

    metadata.priority = CyphalPriorityNominal;
    metadata.transfer_kind = CyphalTransferKindMessage;
    metadata.port_id = 123;
    metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    metadata.transfer_id++;

    const char payload2[] = "ehllo ehllo ehllo";
    size_t payload_size2 = sizeof(payload2);
    CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size2, payload2) == 3);

    CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 123, 100, 2000000) == 1);

    const CanardTxQueueItem *const const_ptr = canardTxPeek(&adapter.que);
    CHECK(const_ptr != nullptr);
    CanardTxQueueItem *ptr = canardTxPop(&adapter.que, const_ptr);
    CHECK(ptr != nullptr);

    CyphalTransfer transfer;
    size_t frame_size = 0;;
    CHECK(cyphal.cyphalRxReceive(ptr->frame.extended_can_id, &ptr->frame.payload_size, static_cast<const uint8_t*>(ptr->frame.payload), &transfer) == 1);
    CHECK(strncmp(payload1, static_cast<const char *>(transfer.payload), 5) == 0);

    const CanardTxQueueItem *const const_ptr2 = canardTxPeek(&adapter.que);
    CHECK(const_ptr2 != nullptr);
    ptr = canardTxPop(&adapter.que, const_ptr2);
    CHECK(ptr != nullptr);
    CHECK(cyphal.cyphalRxReceive(ptr->frame.extended_can_id, &ptr->frame.payload_size, static_cast<const uint8_t*>(ptr->frame.payload), &transfer) == 0);

    const CanardTxQueueItem *const const_ptr3 = canardTxPeek(&adapter.que);
    CHECK(const_ptr3 != nullptr);
    ptr = canardTxPop(&adapter.que, const_ptr3);
    CHECK(ptr != nullptr);
    CHECK(cyphal.cyphalRxReceive(ptr->frame.extended_can_id, &ptr->frame.payload_size, static_cast<const uint8_t*>(ptr->frame.payload), &transfer) == 0);

    const CanardTxQueueItem *const const_ptr4 = canardTxPeek(&adapter.que);
    CHECK(const_ptr4 != nullptr);
    ptr = canardTxPop(&adapter.que, const_ptr4);
    CHECK(ptr != nullptr);
    CHECK(cyphal.cyphalRxReceive(ptr->frame.extended_can_id, &ptr->frame.payload_size, static_cast<const uint8_t*>(ptr->frame.payload), &transfer) == 1);
    CHECK(strncmp(payload2, static_cast<const char *>(transfer.payload), 18) == 0);

}

void *serardMemoryAllocate(void *const user_reference, const size_t size) { return static_cast<void *>(malloc(size)); };
void serardMemoryDeallocate(void *const user_reference, const size_t size, void *const pointer) { free(pointer); };

TEST_CASE("Serard Basics")
{
    CHECK(cyphalNodeIdToSerard(CYPHAL_NODE_ID_UNSET) == SERARD_NODE_ID_UNSET);
    CHECK(cyphalNodeIdToSerard(123) == 123);
    CHECK(serardNodeIdToCyphal(SERARD_NODE_ID_UNSET) == CYPHAL_NODE_ID_UNSET);
    CHECK(serardNodeIdToCyphal(123) == 123);
    CHECK(serardNodeIdToCyphal(0x1122) == 0x0022);

    SUBCASE("serard cyphal serard")
    {
        SerardTransferMetadata metadata{
            SerardPriorityNominal,
            SerardTransferKindMessage,
            123,
            SERARD_NODE_ID_UNSET,
            11};
        SerardTransferMetadata translated = cyphalMetadataToSerard(serardMetadataToCyphal(metadata));
        CHECK((metadata.priority == translated.priority && metadata.transfer_kind == translated.transfer_kind && metadata.port_id == translated.port_id && metadata.remote_node_id == translated.remote_node_id && metadata.transfer_id == translated.transfer_id));
    }
    SUBCASE("cyphal serard cyphal")
    {
        CyphalTransferMetadata metadata{
            CyphalPriorityNominal,
            CyphalTransferKindMessage,
            123,
            CYPHAL_NODE_ID_UNSET,
            11};
        CyphalTransferMetadata translated = serardMetadataToCyphal(cyphalMetadataToSerard(metadata));
        CHECK((metadata.priority == translated.priority && metadata.transfer_kind == translated.transfer_kind && metadata.port_id == translated.port_id && metadata.remote_node_id == translated.remote_node_id && metadata.transfer_id == translated.transfer_id));
    }
}

TEST_CASE("Serard Adapter")
{
    SerardAdapter adapter;
    struct SerardMemoryResource serard_memory_resource = {&adapter.ins, serardMemoryDeallocate, serardMemoryAllocate};
    adapter.ins = serardInit(serard_memory_resource, serard_memory_resource);
    adapter.emitter = [](void *, uint8_t, const uint8_t *) -> bool
    { return true; };
    Cyphal<SerardAdapter> cyphal(&adapter);

    CyphalTransferMetadata metadata;
    metadata.priority = CyphalPriorityNominal;
    metadata.transfer_kind = CyphalTransferKindMessage;
    metadata.port_id = 123;
    metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    metadata.transfer_id = 0;

    const char payload[] = "hello";
    size_t payload_size = sizeof(payload);

    SUBCASE("cyphalTxPush")
    {
        CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size, payload) == 1);
    }

    REQUIRE(adapter.subscriptions.size() == 0);
    SUBCASE("cyphalRxSubscribe and Unsubscribe")
    {
        SerardRxSubscription sub;
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 42, 100, 2000000) == 1);
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 42, 100, 2000000) == 0);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 42) == 1);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 42) == 0);
        SerardRxSubscription sub2;
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 43, 100, 2000000) == 1);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 43) == 1);
    }
    REQUIRE(adapter.subscriptions.size() == 0);

    SUBCASE("cyphalRxSubscribe returns negative on full boxset")
    {
        adapter.subscriptions.clear();
        SerardRxSubscription sub[SerardAdapter::SUBSCRIPTIONS];
        for (int i = 0; i < SerardAdapter::SUBSCRIPTIONS; ++i)
        {
            CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, i, 100, 2000000) == 1);
        }
        REQUIRE(adapter.subscriptions.is_full());
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, SerardAdapter::SUBSCRIPTIONS + 1, 100, 2000000) == -2);
    }
}

static std::vector<uint8_t> rxtx_buffer;
bool emit(void *user_reference, uint8_t size, const uint8_t *data)
{
    for (size_t i = 0; i < size; ++i)
        rxtx_buffer.push_back(data[i]);
    return true;
}

TEST_CASE("Cyphal<serard_adapter> Send Receive")
{
    rxtx_buffer.clear();

    SerardAdapter adapter;
    struct SerardMemoryResource serard_memory_resource = {&adapter.ins, serardMemoryDeallocate, serardMemoryAllocate};
    adapter.ins = serardInit(serard_memory_resource, serard_memory_resource);
    adapter.ins.node_id = 11;
    adapter.user_reference = &adapter.ins;
    adapter.ins.user_reference = &adapter.ins;
    adapter.reass = serardReassemblerInit();
    adapter.emitter = emit;
    Cyphal<SerardAdapter> cyphal(&adapter);

    auto res1 = cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 123, 128, 0);
    CHECK(res1 == 1);

    CyphalTransferMetadata metadata;
    metadata.priority = CyphalPriorityNominal;
    metadata.transfer_kind = CyphalTransferKindMessage;
    metadata.port_id = 123;
    metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    metadata.transfer_id = 0;
    const char payload1[] = "hello\0";
    CHECK(cyphal.cyphalTxPush(0, &metadata, strlen(payload1), payload1) == 1);

    metadata.priority = CyphalPriorityNominal;
    metadata.transfer_kind = CyphalTransferKindMessage;
    metadata.port_id = 123;
    metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    metadata.transfer_id++;
    const char payload2[] = "ehllo\0";
    CHECK(cyphal.cyphalTxPush(0, &metadata, strlen(payload2), payload2) == 1);

    CyphalTransfer transfer;
    size_t in_out = rxtx_buffer.size();
    CHECK(cyphal.cyphalRxReceive(&in_out, rxtx_buffer.data(), &transfer) == 1);
    CHECK(strncmp(payload1, static_cast<const char *>(transfer.payload), 5) == 0);
    CHECK(in_out != 0);

    in_out = rxtx_buffer.size() - in_out;
    CHECK(cyphal.cyphalRxReceive(&in_out, rxtx_buffer.data() + in_out, &transfer) == 1);
    CHECK(strncmp(payload2, static_cast<const char *>(transfer.payload), 5) == 0);
    CHECK(in_out == 0);
}

void *udpardMemoryAllocate(void *const user_reference, const size_t size) { return static_cast<void *>(malloc(size)); };
void udpardMemoryDeallocate(void *const user_reference, const size_t size, void *const pointer) { free(pointer); };

TEST_CASE("Udpard Adapter")
{
    UdpardAdapter adapter;
    struct UdpardMemoryDeleter udpard_memory_deleter = {&adapter.ins, udpardMemoryDeallocate};
    struct UdpardMemoryResource udpard_memory_resource = {&adapter.ins, udpardMemoryDeallocate, udpardMemoryAllocate};
    UdpardNodeID local_node_id = 11;
    Cyphal<UdpardAdapter> cyphal(&adapter);
    udpardTxInit(&adapter.ins, &local_node_id, 100, udpard_memory_resource);
    adapter.memory_resources = {udpard_memory_resource, udpard_memory_resource, udpard_memory_deleter};

    CyphalTransferMetadata metadata;
    metadata.priority = CyphalPriorityNominal;
    metadata.transfer_kind = CyphalTransferKindMessage;
    metadata.port_id = 123;
    metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    metadata.transfer_id = 0;

    const char payload[] = "hello";
    size_t payload_size = sizeof(payload);

    SUBCASE("cyphalTxPush")
    {
        CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size, payload) == 1);
    }
    REQUIRE(adapter.subscriptions.size() == 0);
    SUBCASE("cyphalRxSubscribe and Unsubscribe")
    {
        UdpardRxSubscription sub;
        auto result = cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 42, 100, 2000000);
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 42, 100, 2000000) == 1);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 42) == 1);
        UdpardRxSubscription sub2;
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 43, 100, 2000000) == 1);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 43) == 1);
    }
    REQUIRE(adapter.subscriptions.size() == 0);

    SUBCASE("cyphalRxSubscribe returns negative on full boxset")
    {
        adapter.subscriptions.clear();
        UdpardRxSubscription sub[UdpardAdapter::SUBSCRIPTIONS];
        for (int i = 0; i < UdpardAdapter::SUBSCRIPTIONS; ++i)
        {
            CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, i, 100, 2000000) == 1);
        }
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, UdpardAdapter::SUBSCRIPTIONS + 1, 100, 2000000) == -4);
    }
}

TEST_CASE("Udpard Adapter Send and Receive")
{
    UdpardAdapter adapter;
    struct UdpardMemoryDeleter udpard_memory_deleter = {&adapter.ins, udpardMemoryDeallocate};
    struct UdpardMemoryResource udpard_memory_resource = {&adapter.ins, udpardMemoryDeallocate, udpardMemoryAllocate};
    UdpardNodeID local_node_id = 11;
    Cyphal<UdpardAdapter> cyphal(&adapter);
    udpardTxInit(&adapter.ins, &local_node_id, 100, udpard_memory_resource);
    adapter.memory_resources = {udpard_memory_resource, udpard_memory_resource, udpard_memory_deleter};

    CyphalTransferMetadata metadata;
    metadata.priority = CyphalPriorityNominal;
    metadata.transfer_kind = CyphalTransferKindMessage;
    metadata.port_id = 123;
    metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    metadata.transfer_id = 13;

    const char payload[] = "hello";
    size_t payload_size = sizeof(payload);
    CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size, payload) == 1);
    
    CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 123, 100, 2000000) == 1);

    const UdpardTxItem *const_ptr = udpardTxPeek(&adapter.ins);
    CHECK(const_ptr != nullptr);
    UdpardTxItem *ptr = udpardTxPop(&adapter.ins, const_ptr);
    CHECK(ptr != nullptr);

    // struct UdpardPortSubscription stub = {123, {}};
    // UdpardPortSubscription *subpair = adapter.subscriptions.find(stub, [](const UdpardPortSubscription &a, const UdpardPortSubscription &b) { return a.port_id == b.port_id; });
    // REQUIRE(subpair != nullptr);
    // UdpardRxTransfer transfer;
    // udpardRxSubscriptionReceive(&subpair->subscription, 0, ptr->datagram_payload, 0, &transfer);
    // CHECK(transfer.payload_size != 0);
    // CHECK(strncmp(payload, static_cast<const char *>(transfer.payload.view.data), 5) == 0);
    // CHECK(0==1);
 
    CyphalTransfer transfer;
    cyphal.cyphalRxReceive(&ptr->datagram_payload.size, static_cast<uint8_t*>(ptr->datagram_payload.data), &transfer);
    CHECK(transfer.payload_size != 0);
    CHECK(strncmp(payload, static_cast<const char *>(transfer.payload), 5) == 0);
}

TEST_CASE("Loopard Adapter")
{
    LoopardAdapter adapter;
    Cyphal<LoopardAdapter> cyphal(&adapter);

    CyphalTransferMetadata metadata;
    metadata.priority = CyphalPriorityNominal;
    metadata.transfer_kind = CyphalTransferKindMessage;
    metadata.port_id = 123;
    metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    metadata.transfer_id = 0;

    const char payload[] = "hello";
    size_t payload_size = sizeof(payload);

    SUBCASE("cyphalTxPush: Successful push")
    {
        CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size, payload) == 1);
        REQUIRE(adapter.buffer.size() == 1);
        const auto &transfer = adapter.buffer.peek();
        CHECK(transfer.metadata.port_id == metadata.port_id);
        CHECK(transfer.metadata.priority == metadata.priority);
        CHECK(transfer.payload_size == payload_size);
        CHECK(std::memcmp(transfer.payload, payload, payload_size) == 0);
    }
    SUBCASE("cyphalTxPush: returns 0 when buffer full")
    {
        // fill the buffer
        for (int i = 0; i < LoopardAdapter::BUFFER; ++i)
        {
            CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size, payload) == 1);
        }
        CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size, payload) == 0);
    }

    SUBCASE("cyphalRxSubscribe and Unsubscribe")
    {

        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 42, 100, 2000000) == 1);
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 42, 100, 2000000) == 1);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 42) == 1);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 42) == 1);
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 43, 100, 2000000) == 1);
        CHECK(cyphal.cyphalRxUnsubscribe(CyphalTransferKindMessage, 43) == 1);
    }

    REQUIRE(adapter.subscriptions.size() == 0);
    SUBCASE("cyphalRxSubscribe returns negative on full boxset")
    {
        adapter.subscriptions.clear();
        for (int i = 0; i < LoopardAdapter::SUBSCRIPTIONS; ++i)
        {
            CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, i, 100, 2000000) == 1);
        }
        REQUIRE(adapter.subscriptions.is_full());
        CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, LoopardAdapter::SUBSCRIPTIONS + 1, 100, 2000000) == 1);
    }
}

TEST_CASE("Loopard Send Receive")
{
    LoopardAdapter adapter;
    Cyphal<LoopardAdapter> cyphal(&adapter);

    CyphalTransferMetadata metadata;
    metadata.priority = CyphalPriorityNominal;
    metadata.transfer_kind = CyphalTransferKindMessage;
    metadata.port_id = 123;
    metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    metadata.transfer_id = 0;

    const char payload1[] = "hello";
    size_t payload_size1 = sizeof(payload1);

    CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size1, payload1) == 1);
    REQUIRE(adapter.buffer.size() == 1);

    const char payload2[] = "ehllo ";
    size_t payload_size2 = sizeof(payload2);
    metadata.transfer_id++;
    CHECK(cyphal.cyphalTxPush(0, &metadata, payload_size2, payload2) == 1);

    CHECK(cyphal.cyphalRxSubscribe(CyphalTransferKindMessage, 123, 100, 2000000) == 1);

    CyphalTransfer transfer = {};
    size_t inout_payload_size = 0;
    CHECK(cyphal.cyphalRxReceive(nullptr, inout_payload_size, &transfer) == 2);
    CHECK(transfer.payload_size == payload_size1);
    CHECK(strncmp(payload1, static_cast<const char *>(transfer.payload), 5) == 0);
    CHECK(cyphal.cyphalRxReceive(nullptr, inout_payload_size, &transfer) == 1);
    CHECK(transfer.payload_size == payload_size2);
    CHECK(strncmp(payload2, static_cast<const char *>(transfer.payload), 5) == 0);
    CHECK(cyphal.cyphalRxReceive(nullptr, inout_payload_size, &transfer) == 0);
}
