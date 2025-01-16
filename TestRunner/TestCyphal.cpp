#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "cyphal.hpp"
#include "canard_adapter.hpp"
#include "serard_adapter.hpp"
#include <cstring>

// Example Memory Allocator (replace with yours)
void* example_allocate(CyphalInstance*, size_t amount) { return malloc(amount); }
void example_free(CyphalInstance*, void* ptr) { free(ptr); }


TEST_CASE("Cyphal Instance Creation") {
    SUBCASE("Canard Instance") {
        CyphalImpl<CanardAdapter> canard_instance(example_allocate, example_free);
        REQUIRE(canard_instance.memory_allocate() != nullptr);
        REQUIRE(canard_instance.memory_free() != nullptr);
    }
    SUBCASE("Serard Instance") {
          CyphalImpl<SerardAdapter> serard_instance(example_allocate, example_free);
        REQUIRE(serard_instance.memory_allocate() != nullptr);
        REQUIRE(serard_instance.memory_free() != nullptr);
    }
}

TEST_CASE("Setting Node ID") {
    CyphalImpl<CanardAdapter> canard_instance(example_allocate, example_free);
    CyphalImpl<SerardAdapter> serard_instance(example_allocate, example_free);
    
    SUBCASE("Canard Node ID") {
        canard_instance.node_id = 10;
        REQUIRE(canard_instance.node_id == 10);
    }
    SUBCASE("Serard Node ID"){
         serard_instance.node_id = 20;
         REQUIRE(serard_instance.node_id == 20);
    }
}


TEST_CASE("Canard Tx, Serard Rx") {
    CyphalImpl<CanardAdapter> canard_instance(example_allocate, example_free);
    CyphalImpl<SerardAdapter> serard_instance(example_allocate, example_free);
    
    SUBCASE("Tx and Rx"){
        CyphalTxQueue canard_tx_queue = canard_instance.tx_init(100, 64);
        CyphalTransferMetadata metadata;
            metadata.priority = CyphalPriority::Nominal;
            metadata.transfer_kind = CyphalTransferKind::Message;
            metadata.port_id = 123;
            metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
            metadata.transfer_id = 1;
        uint8_t payload[10] = {0,1,2,3,4,5,6,7,8,9};
        
        int result = canard_instance.tx_push(canard_tx_queue, 0, metadata, 10, payload);
        REQUIRE(result >=0);


        // Example Rx
        CyphalFrame frame;
        uint8_t data[] = {1,2,3,4,5};
        frame.payload = data;
        frame.payload_size = 5;
        frame.impl =  new SerardReassembler(); //Serard requires that a pointer to a reassembler be passed
    

        CyphalRxTransfer transfer;
        CyphalRxSubscription* sub = nullptr;
         result = serard_instance.rx_accept(1000,frame,0,transfer,&sub);
         delete static_cast<SerardReassembler*>(frame.impl);
        REQUIRE(result == 0);
    }
    
    

}

TEST_CASE("Subscription and Unsubscription") {
     CyphalImpl<CanardAdapter> canard_instance(example_allocate, example_free);
     CyphalImpl<SerardAdapter> serard_instance(example_allocate, example_free);

    SUBCASE("Canard Subscription") {
        CyphalRxSubscription canard_subscription;
        int result = canard_instance.rx_subscribe(CyphalTransferKind::Message, 123, 200 , 1000, canard_subscription);
        REQUIRE(result == 1);
        result = canard_instance.rx_unsubscribe(CyphalTransferKind::Message,123);
        REQUIRE(result == 1);
    }

    SUBCASE("Serard Subscription") {
        CyphalRxSubscription serard_subscription;
        int result =  serard_instance.rx_subscribe(CyphalTransferKind::Message, 123, 200 , 1000, serard_subscription);
        REQUIRE(result == 1);
        result =  serard_instance.rx_unsubscribe(CyphalTransferKind::Message,123);
        REQUIRE(result >= 0);
    }
}

TEST_CASE("Filtering") {
    CyphalImpl<CanardAdapter> canard_instance(example_allocate, example_free);
    CyphalImpl<SerardAdapter> serard_instance(example_allocate, example_free);

    SUBCASE("Canard Filter"){
        CyphalFilter filter =   canard_instance.make_filter_for_subject(123);
        CyphalFilter filter2 = canard_instance.make_filter_for_services(10);
    }

      SUBCASE("Serard Filter"){
           CyphalFilter filter3 =  serard_instance.make_filter_for_subject(123);
     }
}