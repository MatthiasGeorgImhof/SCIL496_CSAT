#include "cyphal.hpp"
#include "canard_adapter.hpp"
#include "serard_adapter.hpp"
#include <cstdlib>
#include <iostream>
// Example Memory Allocator (replace with yours)
void* example_allocate(CyphalInstance*, size_t amount) { return malloc(amount); }
void example_free(CyphalInstance*, void* ptr) { free(ptr); }


int main() {
    // Create Cyphal instances with different adapters
    CyphalImpl<CanardAdapter> canard_instance(example_allocate,example_free);
    CyphalImpl<SerardAdapter> serard_instance(example_allocate,example_free);


    canard_instance.node_id = 10;
    serard_instance.node_id = 20;

    // Example Tx

    CyphalTxQueue canard_tx_queue = canard_instance.tx_init(100, 64);
    CyphalTransferMetadata metadata;
        metadata.priority = CyphalPriority::Nominal;
        metadata.transfer_kind = CyphalTransferKind::Message;
        metadata.port_id = 123;
        metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
        metadata.transfer_id = 1;
        uint8_t payload[10] = {0,1,2,3,4,5,6,7,8,9};
        canard_instance.tx_push(canard_tx_queue, 0, metadata, 10, payload);

     // Example Rx
     CyphalFrame frame;
     uint8_t data[] = {1,2,3,4,5};
     frame.payload = data;
     frame.payload_size = 5;
      frame.impl = nullptr; // Serard requires that a pointer to an structure be passed (the reassembler)

    CyphalRxTransfer transfer;
    CyphalRxSubscription* sub = nullptr;
    canard_instance.rx_accept(1000,frame,0,transfer,&sub);

     frame.impl =  new SerardReassembler();
    serard_instance.rx_accept(1000,frame,0,transfer,&sub);
    delete static_cast<SerardReassembler*>(frame.impl);

     CyphalRxSubscription canard_subscription;
      int result = canard_instance.rx_subscribe(CyphalTransferKind::Message, 123, 200 , 1000, canard_subscription);
      if(result != 1){
          std::cout << "canard subscribe failed" << std::endl;
      }
       CyphalRxSubscription serard_subscription;
       result =  serard_instance.rx_subscribe(CyphalTransferKind::Message, 123, 200 , 1000, serard_subscription);
         if(result != 1){
             std::cout << "serard subscribe failed" << std::endl;
        }

       canard_instance.rx_unsubscribe(CyphalTransferKind::Message,123);
        serard_instance.rx_unsubscribe(CyphalTransferKind::Message,123);



     CyphalFilter filter =   canard_instance.make_filter_for_subject(123);
        filter =  serard_instance.make_filter_for_subject(123);

        CyphalFilter filter2 =  canard_instance.make_filter_for_services(10);

    return 0;
}