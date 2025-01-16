#ifndef CANARD_ADAPTER_HPP_INCLUDED
#define CANARD_ADAPTER_HPP_INCLUDED

#include "cyphal.hpp"
#include "canard.h"

struct CanardAdapter {
    static void* init(void* (*memory_allocate)(CyphalInstance*, size_t), void (*memory_free)(CyphalInstance*, void*));
    static void deinit(void* impl);
    static void* tx_init(unsigned long capacity, unsigned long mtu_bytes);
    static int32_t tx_push(CyphalTxQueue* const que,
                            CyphalInstance* const ins,
                            unsigned long tx_deadline_usec,
                            const CyphalTransferMetadata* const metadata,
                            unsigned long payload_size,
                            const void* const payload);
    static const CyphalTxQueueItem* tx_peek(const CyphalTxQueue* const que);
    static CyphalTxQueueItem* tx_pop(const CyphalTxQueue* const que, const CyphalTxQueueItem* const item);
    static int8_t rx_accept(CyphalInstance* const ins,
                            unsigned long timestamp_usec,
                            const CyphalFrame* const frame,
                            unsigned char redundant_iface_index,
                            CyphalRxTransfer* const out_transfer,
                            CyphalRxSubscription** const out_subscription);
   static void* rx_subscribe(CyphalInstance* const ins,
                            const CyphalTransferKind transfer_kind,
                            unsigned short port_id,
                            unsigned long extent,
                            unsigned long transfer_id_timeout_usec,
                            CyphalRxSubscription* const out_subscription);
    static int8_t rx_unsubscribe(CyphalInstance* const ins,
                            const CyphalTransferKind transfer_kind,
                            unsigned short port_id);

    static int8_t rx_get_subscription(CyphalInstance* const ins,
                            const CyphalTransferKind transfer_kind,
                            unsigned short port_id,
                            CyphalRxSubscription** const out_subscription);

    static CyphalFilter make_filter_for_subject(unsigned short subject_id);
    static CyphalFilter make_filter_for_service(unsigned short service_id, unsigned char local_node_id);
    static CyphalFilter make_filter_for_services(unsigned char local_node_id);
    static CyphalFilter consolidate_filters(const CyphalFilter* const a, const CyphalFilter* const b);
};

#endif