#pragma once

#include "cyphal.hpp"
#include "canard.h"

#include "BoxSet.hpp"

static_assert(CYPHAL_NODE_ID_UNSET == CANARD_NODE_ID_UNSET, "unset lengths differ");
static_assert(sizeof(CyphalTransferMetadata) == sizeof(CanardTransferMetadata), "metadata sizes differ");
static_assert(sizeof(CyphalTransfer) == sizeof(CanardRxTransfer), "rxtransfer sizes differ");

struct CanardAdapter
{
    static constexpr uint8_t SUBSCRIPTIONS = 32;
    CanardInstance ins;
    CanardTxQueue que;
    BoxSet<CanardRxSubscription, SUBSCRIPTIONS> subscriptions;
};

template <>
class Cyphal<CanardAdapter>
{
private:
    CanardAdapter *adapter_;

private:


public:
    Cyphal<CanardAdapter>() = delete;
    Cyphal<CanardAdapter>(CanardAdapter *adapter) : adapter_(adapter) {}

    int32_t cyphalTxPush(const CyphalMicrosecond tx_deadline_usec,
                         const CyphalTransferMetadata *const metadata,
                         const size_t payload_size,
                         const void *const payload)
    {
        return canardTxPush(&adapter_->que, &adapter_->ins, tx_deadline_usec, reinterpret_cast<const CanardTransferMetadata*>(metadata), payload_size, payload);
    }

    int8_t cyphalRxSubscribe(const CyphalTransferKind transfer_kind,
                             const CyphalPortID port_id,
                             const size_t extent,
                             const CyphalMicrosecond transfer_id_timeout_usec)
    {
        CanardRxSubscription stub{ .port_id = port_id };
        CanardRxSubscription *subscription = adapter_->subscriptions.find_or_create(stub, [](const CanardRxSubscription &a, const CanardRxSubscription &b)
                                                                                { return a.port_id == b.port_id; });
        return canardRxSubscribe(&adapter_->ins, static_cast<const CanardTransferKind>(transfer_kind), port_id, extent, transfer_id_timeout_usec, subscription);
    }

    int8_t cyphalRxUnsubscribe(const CyphalTransferKind transfer_kind,
                               const CyphalPortID port_id)
    {
        CanardRxSubscription stub{ .port_id = port_id };
        CanardRxSubscription *subscription = adapter_->subscriptions.find(stub, [](const CanardRxSubscription &a, const CanardRxSubscription &b)
                                                                                { return a.port_id == b.port_id; });
        auto result = canardRxUnsubscribe(&adapter_->ins, static_cast<const CanardTransferKind>(transfer_kind), port_id);
        if (subscription) adapter_->subscriptions.remove(subscription);
        return result;
    }

        int32_t cyphalRxReceive(uint32_t extended_can_id, const size_t *frame_size, const uint8_t *const frame, CyphalTransfer *out_transfer)
    {
        CanardFrame canard_frame = { extended_can_id, *frame_size, frame};
        return canardRxAccept(&adapter_->ins, 0, &canard_frame, 0, reinterpret_cast<CanardRxTransfer*>(out_transfer), nullptr);
    }

};
