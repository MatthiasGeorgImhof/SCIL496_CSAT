#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "BoxSet.hpp"
#include "cyphal.hpp"
#include "canard_adapter.hpp"
#include "serard_adapter.hpp"
#include "udpard_adapter.hpp"
#include <iostream>
#include <cstdint>

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


void* serardMemoryAllocate(void* const user_reference, const size_t size) { return static_cast<void *>(malloc(size)); };
void serardMemoryDeallocate(void* const user_reference, const size_t size, void* const pointer) { free(pointer); };

TEST_CASE("Serard Adapter")
{
    SerardAdapter adapter;
    struct SerardMemoryResource serard_memory_resource = { &adapter.ins, serardMemoryDeallocate, serardMemoryAllocate };
    adapter.ins = serardInit(serard_memory_resource, serard_memory_resource);
    adapter.emitter = [](void *, uint8_t, const uint8_t *) -> bool{ return true; };
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

void* udpardMemoryAllocate(void* const user_reference, const size_t size) { return static_cast<void *>(malloc(size)); };
void udpardMemoryDeallocate(void* const user_reference, const size_t size, void* const pointer) { free(pointer); };

TEST_CASE("Udpard Adapter")
{
    UdpardAdapter adapter;
    struct UdpardMemoryDeleter udpard_memory_deleter = { &adapter.ins, udpardMemoryDeallocate };
    struct UdpardMemoryResource udpard_memory_resource = { &adapter.ins, udpardMemoryDeallocate, udpardMemoryAllocate };
    UdpardNodeID local_node_id = 11;
    Cyphal<UdpardAdapter> cyphal(&adapter);
    udpardTxInit(&adapter.ins, &local_node_id, 100, udpard_memory_resource);
    adapter.memory_resources = { udpard_memory_resource, udpard_memory_resource, udpard_memory_deleter };

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