#include "cyphal.hpp"
#include "serard.h"
#include "serard_adapter.hpp"
#include <cstdlib>

// Private Structure used to store adapter specific information
struct serardAdapterState {
    Serard serard;
};

// =====================  Adapter Functions  =====================
void* SerardAdapter::init(void* (*memory_allocate)(CyphalInstance*, size_t), void (*memory_free)(CyphalInstance*, void*)) {

    serardAdapterState* state = new serardAdapterState();
    if (!state) {
        return nullptr;
    }

    SerardMemoryResource payload_resource;
    payload_resource.user_reference = nullptr;
    payload_resource.allocate = (SerardMemoryAllocate)memory_allocate;
    payload_resource.deallocate = (SerardMemoryDeallocate)memory_free;

    SerardMemoryResource rx_session_resource;
    rx_session_resource.user_reference = nullptr;
    rx_session_resource.allocate = (SerardMemoryAllocate)memory_allocate;
    rx_session_resource.deallocate = (SerardMemoryDeallocate)memory_free;


    state->serard = serardInit(payload_resource, rx_session_resource);
    return state;
}

void SerardAdapter::deinit(void* impl) {
    delete (serardAdapterState*)impl;
}


void* SerardAdapter::tx_init(unsigned long capacity, unsigned long mtu_bytes) {
    (void)capacity;
    (void)mtu_bytes;
    // Serard doesn't have a tx queue
    return nullptr;
}

// ===================== Tx =====================
int32_t SerardAdapter::tx_push(CyphalTxQueue* const que,
                            CyphalInstance* const ins,
                            unsigned long tx_deadline_usec,
                            const CyphalTransferMetadata* const metadata,
                            unsigned long payload_size,
                            const void* const payload) {
    (void)que;
    (void)tx_deadline_usec;
    if (!ins || !metadata) {
        return -CYPHAL_ERROR_INVALID_ARGUMENT;
    }

    SerardTransferMetadata serard_metadata;
    serard_metadata.priority = (enum SerardPriority)metadata->priority;
    serard_metadata.transfer_kind = (enum SerardTransferKind)metadata->transfer_kind;
    serard_metadata.port_id = (SerardPortID)metadata->port_id;
    serard_metadata.remote_node_id = (SerardNodeID)metadata->remote_node_id;
    serard_metadata.transfer_id = (SerardTransferID)metadata->transfer_id;


    // this emitter is a dummy to just test, it should be defined somewhere else.
    auto emitter = [](void* user_reference, uint8_t data_size, const uint8_t* data) {
        (void)user_reference;
        // This is a dummy emitter, the user has to implement a more complex one
        // This is where you would write to serial bus.
        return true;
    };

    return serardTxPush((Serard*)ins->get_impl(),
                        &serard_metadata,
                        payload_size,
                        payload,
                        nullptr, //user_reference
                        emitter);
}

const CyphalTxQueueItem* SerardAdapter::tx_peek(const CyphalTxQueue* const que) {
    (void)que;
    // Serard doesn't have a tx queue, return null
    return nullptr;
}

CyphalTxQueueItem* SerardAdapter::tx_pop(const CyphalTxQueue* const que, const CyphalTxQueueItem* const item) {
    (void)que;
    (void)item;
    // Serard doesn't have a tx queue, return null
    return nullptr;
}


// ===================== Rx =====================
int8_t SerardAdapter::rx_accept(CyphalInstance* const ins,
                            unsigned long timestamp_usec,
                            const CyphalFrame* const frame,
                            unsigned char redundant_iface_index,
                            CyphalRxTransfer* const out_transfer,
                            CyphalRxSubscription** const out_subscription) {
    (void)redundant_iface_index;
    if (!ins || !frame || !out_transfer) {
        return -CYPHAL_ERROR_INVALID_ARGUMENT;
    }

    SerardRxTransfer serard_transfer;
    SerardRxSubscription* serard_subscription = nullptr;
    struct SerardReassembler* reassembler = (struct SerardReassembler*) const_cast<void*>(frame->get_impl());


    int8_t result = serardRxAccept((Serard*)ins->get_impl(),
                        reassembler,
                        timestamp_usec,
                        (size_t*)&frame->payload_size,
                        (const uint8_t*)frame->payload,
                        &serard_transfer,
                        &serard_subscription
                        );

    if (result > 0) {
        out_transfer->metadata.priority = (CyphalPriority)serard_transfer.metadata.priority;
        out_transfer->metadata.transfer_kind = (CyphalTransferKind)serard_transfer.metadata.transfer_kind;
        out_transfer->metadata.port_id = (CyphalPortID)serard_transfer.metadata.port_id;
        out_transfer->metadata.remote_node_id = (CyphalNodeID)serard_transfer.metadata.remote_node_id;
        out_transfer->metadata.transfer_id = (CyphalTransferID)serard_transfer.metadata.transfer_id;
        out_transfer->timestamp_usec = serard_transfer.timestamp_usec;
        out_transfer->payload_size = serard_transfer.payload_size;
        out_transfer->payload = serard_transfer.payload;
    }

    if (out_subscription) {
        *out_subscription = (CyphalRxSubscription*)serard_subscription;
    }


    return result;

}


void* SerardAdapter::rx_subscribe(CyphalInstance* const ins,
                                    const CyphalTransferKind transfer_kind,
                                    unsigned short port_id,
                                    unsigned long extent,
                                    unsigned long transfer_id_timeout_usec,
                                    CyphalRxSubscription* const out_subscription) {

    if (!ins || !out_subscription) {
        return nullptr;
    }

    SerardRxSubscription* serard_subscription = new SerardRxSubscription();
    int8_t result = serardRxSubscribe((Serard*)ins->get_impl(),
        (enum SerardTransferKind)transfer_kind,
        (SerardPortID)port_id,
        extent,
        transfer_id_timeout_usec,
        serard_subscription
    );
    if (result >= 0) {
        out_subscription->transfer_id_timeout_usec = transfer_id_timeout_usec;
        out_subscription->extent = extent;
        out_subscription->port_id = port_id;
        return serard_subscription;
    } else {
        delete serard_subscription;
        return nullptr;
    }
}

int8_t SerardAdapter::rx_unsubscribe(CyphalInstance* const ins,
                                   const CyphalTransferKind transfer_kind,
                                   unsigned short port_id) {
    if (!ins) {
        return -CYPHAL_ERROR_INVALID_ARGUMENT;
    }

    return serardRxUnsubscribe((Serard*)ins->get_impl(),
                                (enum SerardTransferKind)transfer_kind,
                                (SerardPortID)port_id);
}


int8_t SerardAdapter::rx_get_subscription(CyphalInstance* const ins,
                                   const CyphalTransferKind transfer_kind,
                                   unsigned short port_id,
                                   CyphalRxSubscription** const out_subscription) {
    (void)ins;
    (void)transfer_kind;
    (void)port_id;
    *out_subscription = nullptr;
    return -CYPHAL_ERROR_ANONYMOUS;
}

CyphalFilter SerardAdapter::make_filter_for_subject(unsigned short subject_id) {
    (void)subject_id;
    // Serard doesn't have filters, so return empty
     CyphalFilter cyphal_filter;
    cyphal_filter.extended_can_id = 0;
    cyphal_filter.extended_mask = 0;
    return cyphal_filter;
}

CyphalFilter SerardAdapter::make_filter_for_service(unsigned short service_id, unsigned char local_node_id) {
     (void)service_id;
      (void)local_node_id;
    // Serard doesn't have filters, so return empty
    CyphalFilter cyphal_filter;
    cyphal_filter.extended_can_id = 0;
    cyphal_filter.extended_mask = 0;
    return cyphal_filter;
}

CyphalFilter SerardAdapter::make_filter_for_services(unsigned char local_node_id) {
    (void)local_node_id;
     // Serard doesn't have filters, so return empty
    CyphalFilter cyphal_filter;
    cyphal_filter.extended_can_id = 0;
    cyphal_filter.extended_mask = 0;
    return cyphal_filter;
}

CyphalFilter SerardAdapter::consolidate_filters(const CyphalFilter* const a, const CyphalFilter* const b) {
    (void)a;
    (void)b;
    // Serard doesn't have filters, so return empty
    CyphalFilter cyphal_filter;
    cyphal_filter.extended_can_id = 0;
    cyphal_filter.extended_mask = 0;
    return cyphal_filter;
}