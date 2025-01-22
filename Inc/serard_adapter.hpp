#pragma once

#include "cyphal.hpp"
#include "serard.h"

#include "BoxSet.hpp"

struct SerardAdapter
{
    static constexpr uint8_t SUBSCRIPTIONS = 32;
    struct Serard ins;
    struct SerardReassembler reass;
    SerardTxEmit emitter;
    void *user_reference;
    BoxSet<SerardRxSubscription, SUBSCRIPTIONS> subscriptions;
};

template <>
class Cyphal<SerardAdapter>
{
private:
    SerardAdapter *adapter_;

public:
    Cyphal<SerardAdapter>() = delete;
    Cyphal<SerardAdapter>(SerardAdapter *adapter) : adapter_(adapter) {}

    int32_t cyphalTxPush(const CyphalMicrosecond tx_deadline_usec,
                         const CyphalTransferMetadata *const metadata,
                         const size_t payload_size,
                         const void *const payload)
    {
        return serardTxPush(&adapter_->ins, reinterpret_cast<const SerardTransferMetadata *>(metadata), payload_size, payload, adapter_->user_reference, adapter_->emitter);
    }

    int8_t cyphalRxSubscribe(const CyphalTransferKind transfer_kind,
                             const CyphalPortID port_id,
                             const size_t extent,
                             const CyphalMicrosecond transfer_id_timeout_usec)
    {
        SerardRxSubscription stub{};
        SerardRxSubscription *subscription = adapter_->subscriptions.find_or_create(stub, [port_id](const SerardRxSubscription &a, const SerardRxSubscription &b)
                                                                                    { return port_id == b.port_id; });
        return serardRxSubscribe(&adapter_->ins, static_cast<SerardTransferKind>(transfer_kind), port_id, extent, transfer_id_timeout_usec, subscription);
    }

    int8_t cyphalRxUnsubscribe(const CyphalTransferKind transfer_kind,
                               const CyphalPortID port_id)
    {
        SerardRxSubscription stub{};
        auto it = adapter_->subscriptions.find(stub, [port_id](const SerardRxSubscription &a, const SerardRxSubscription &b)
                                                                                { return port_id == b.port_id; });
        auto result = serardRxUnsubscribe(&adapter_-> ins, static_cast<SerardTransferKind>(transfer_kind), port_id);
        if (it) adapter_->subscriptions.remove(it);
        return result;
    }

};
