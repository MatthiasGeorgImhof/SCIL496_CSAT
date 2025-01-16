#include "cyphal.hpp"
#include "canard.h"
#include "canard_adapter.hpp"
#include <cstdlib>

// Private Structure used to store adapter specific information
struct canardAdapterState {
    CanardInstance canard;
};

// =====================  Adapter Functions  =====================
void* CanardAdapter::init(void* (*memory_allocate)(CyphalInstance*, size_t), void (*memory_free)(CyphalInstance*, void*)) {
    canardAdapterState* state = new canardAdapterState();
    if (!state) {
        return nullptr;
    }

    state->canard = canardInit(
        (CanardMemoryAllocate)memory_allocate,
        (CanardMemoryFree)memory_free
    );
    return state;
}

void CanardAdapter::deinit(void* impl) {
    delete (canardAdapterState*)impl;
}

void* CanardAdapter::tx_init(unsigned long capacity, unsigned long mtu_bytes) {
    CanardTxQueue* queue = new CanardTxQueue();
    if (!queue) {
        return nullptr;
    }
    *queue = canardTxInit(capacity, mtu_bytes);
    return queue;
}

int32_t CanardAdapter::tx_push(CyphalTxQueue* const que,
                            CyphalInstance* const ins,
                            unsigned long tx_deadline_usec,
                            const CyphalTransferMetadata* const metadata,
                            unsigned long payload_size,
                            const void* const payload) {
    if (!que || !ins || !metadata) {
        return -CYPHAL_ERROR_INVALID_ARGUMENT;
    }

    CanardTransferMetadata canard_metadata;
    canard_metadata.priority = (CanardPriority)metadata->priority;
    canard_metadata.transfer_kind = (CanardTransferKind)metadata->transfer_kind;
    canard_metadata.port_id = (CanardPortID)metadata->port_id;
    canard_metadata.remote_node_id = (CanardNodeID)metadata->remote_node_id;
    canard_metadata.transfer_id = (CanardTransferID)metadata->transfer_id;

    return canardTxPush((CanardTxQueue*)que->get_impl(),
                        (CanardInstance*)ins->get_impl(),
                        tx_deadline_usec,
                        &canard_metadata,
                        payload_size,
                        payload);
}

const CyphalTxQueueItem* CanardAdapter::tx_peek(const CyphalTxQueue* const que) {
    if (!que) {
        return nullptr;
    }
    return (const CyphalTxQueueItem*)canardTxPeek((const CanardTxQueue*)que->get_impl());
}

CyphalTxQueueItem* CanardAdapter::tx_pop(const CyphalTxQueue* const que, const CyphalTxQueueItem* const item) {
    if (!que || !item) {
        return nullptr;
    }
    return (CyphalTxQueueItem*)canardTxPop((CanardTxQueue*)que->get_impl(), (const CanardTxQueueItem*)item->get_impl());
}

int8_t CanardAdapter::rx_accept(CyphalInstance* const ins,
                            unsigned long timestamp_usec,
                            const CyphalFrame* const frame,
                            unsigned char redundant_iface_index,
                            CyphalRxTransfer* const out_transfer,
                            CyphalRxSubscription** const out_subscription) {
    if (!ins || !frame || !out_transfer) {
        return -CYPHAL_ERROR_INVALID_ARGUMENT;
    }

    CanardFrame canard_frame;
    canard_frame.payload_size = frame->payload_size;
    canard_frame.payload = frame->payload;

    CanardRxTransfer canard_transfer;
    CanardRxSubscription* canard_subscription = nullptr;

    int8_t result = canardRxAccept(
        (CanardInstance*)ins->get_impl(),
        timestamp_usec,
        &canard_frame,
        redundant_iface_index,
        &canard_transfer,
        &canard_subscription
    );

    if (result > 0) {
        out_transfer->metadata.priority = (CyphalPriority)canard_transfer.metadata.priority;
        out_transfer->metadata.transfer_kind = (CyphalTransferKind)canard_transfer.metadata.transfer_kind;
        out_transfer->metadata.port_id = (CyphalPortID)canard_transfer.metadata.port_id;
        out_transfer->metadata.remote_node_id = (CyphalNodeID)canard_transfer.metadata.remote_node_id;
        out_transfer->metadata.transfer_id = (CyphalTransferID)canard_transfer.metadata.transfer_id;
        out_transfer->timestamp_usec = canard_transfer.timestamp_usec;
        out_transfer->payload_size = canard_transfer.payload_size;
        out_transfer->payload = canard_transfer.payload;
    }

    if (out_subscription) {
        *out_subscription = (CyphalRxSubscription*)canard_subscription;
    }
    return result;
}

void* CanardAdapter::rx_subscribe(CyphalInstance* const ins,
                                    const CyphalTransferKind transfer_kind,
                                    unsigned short port_id,
                                    unsigned long extent,
                                    unsigned long transfer_id_timeout_usec,
                                    CyphalRxSubscription* const out_subscription) {

    if (!ins || !out_subscription) {
        return nullptr;
    }

    CanardRxSubscription* canard_subscription = new CanardRxSubscription();
    int8_t result = canardRxSubscribe((CanardInstance*)ins->get_impl(),
                                (CanardTransferKind)transfer_kind,
                                (CanardPortID)port_id,
                                extent,
                                transfer_id_timeout_usec,
                                canard_subscription
                                );
    if (result >= 0) {
        out_subscription->transfer_id_timeout_usec = transfer_id_timeout_usec;
        out_subscription->extent = extent;
        out_subscription->port_id = port_id;
        return canard_subscription;
    } else {
        delete canard_subscription;
        return nullptr;
    }


}

int8_t CanardAdapter::rx_unsubscribe(CyphalInstance* const ins,
                                   const CyphalTransferKind transfer_kind,
                                   unsigned short port_id) {
    if (!ins) {
        return -CYPHAL_ERROR_INVALID_ARGUMENT;
    }

    return canardRxUnsubscribe((CanardInstance*)ins->get_impl(),
                                (CanardTransferKind)transfer_kind,
                                (CanardPortID)port_id);
}

int8_t CanardAdapter::rx_get_subscription(CyphalInstance* const ins,
                                   const CyphalTransferKind transfer_kind,
                                   unsigned short port_id,
                                   CyphalRxSubscription** const out_subscription) {

    if (!ins || !out_subscription) {
        return -CYPHAL_ERROR_INVALID_ARGUMENT;
    }
    CanardRxSubscription* canard_subscription = nullptr;
    int8_t result = canardRxGetSubscription(
        (CanardInstance*)ins->get_impl(),
        (CanardTransferKind)transfer_kind,
        (CanardPortID)port_id,
        &canard_subscription
    );

    if (result >= 0 && canard_subscription) {
        *out_subscription = (CyphalRxSubscription*)canard_subscription;
    }
    return result;
}

CyphalFilter CanardAdapter::make_filter_for_subject(unsigned short subject_id) {
    CanardFilter canard_filter = canardMakeFilterForSubject(subject_id);
    CyphalFilter cyphal_filter;
    cyphal_filter.extended_can_id = canard_filter.extended_can_id;
    cyphal_filter.extended_mask = canard_filter.extended_mask;
    return cyphal_filter;
}

CyphalFilter CanardAdapter::make_filter_for_service(unsigned short service_id, unsigned char local_node_id) {
    CanardFilter canard_filter = canardMakeFilterForService(service_id, local_node_id);
    CyphalFilter cyphal_filter;
    cyphal_filter.extended_can_id = canard_filter.extended_can_id;
    cyphal_filter.extended_mask = canard_filter.extended_mask;
    return cyphal_filter;
}

CyphalFilter CanardAdapter::make_filter_for_services(unsigned char local_node_id) {
    CanardFilter canard_filter = canardMakeFilterForServices(local_node_id);
    CyphalFilter cyphal_filter;
    cyphal_filter.extended_can_id = canard_filter.extended_can_id;
    cyphal_filter.extended_mask = canard_filter.extended_mask;
    return cyphal_filter;
}

CyphalFilter CanardAdapter::consolidate_filters(const CyphalFilter* const a, const CyphalFilter* const b) {
    CanardFilter canard_a;
    canard_a.extended_can_id = a->extended_can_id;
    canard_a.extended_mask = a->extended_mask;

    CanardFilter canard_b;
    canard_b.extended_can_id = b->extended_can_id;
    canard_b.extended_mask = b->extended_mask;

    CanardFilter canard_filter = canardConsolidateFilters(&canard_a, &canard_b);

    CyphalFilter cyphal_filter;
    cyphal_filter.extended_can_id = canard_filter.extended_can_id;
    cyphal_filter.extended_mask = canard_filter.extended_mask;
    return cyphal_filter;
}