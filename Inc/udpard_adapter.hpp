#include "cyphal.hpp"
#include "udpard.h"

#include "BoxSet.hpp"


struct UdpardPortSubscription
{
    UdpardPortID port_id;
    UdpardRxSubscription subscription;
};

struct UdpardAdapter
{
    static constexpr uint8_t SUBSCRIPTIONS = 32;
    struct UdpardTx ins;
    UdpardRxMemoryResources memory_resources;
    void *user_transfer_reference;
    BoxSet<UdpardPortSubscription, SUBSCRIPTIONS> subscriptions;
};

template <>
class Cyphal<UdpardAdapter>
{
private:
    UdpardAdapter *adapter_;

public:
    Cyphal<UdpardAdapter>() = delete;
    Cyphal<UdpardAdapter>(UdpardAdapter *adapter) : adapter_(adapter) {}

    int32_t cyphalTxPush(const CyphalMicrosecond tx_deadline_usec,
                         const CyphalTransferMetadata *const metadata,
                         const size_t payload_size,
                         const void *const payload)
    {
        struct UdpardPayload payload_ = {payload_size, payload};
        return udpardTxPublish(&adapter_->ins, tx_deadline_usec, static_cast<UdpardPriority>(metadata->priority), metadata->port_id, metadata->transfer_id, payload_, adapter_->user_transfer_reference);
    }

    int8_t cyphalRxSubscribe(const CyphalTransferKind transfer_kind,
                             const CyphalPortID port_id,
                             const size_t extent,
                             const CyphalMicrosecond transfer_id_timeout_usec)
    {
        UdpardPortSubscription stub = { port_id, {}};
        UdpardPortSubscription *subscription = adapter_->subscriptions.find_or_create(stub, [](const UdpardPortSubscription &a, const UdpardPortSubscription &b)
                                                                                       { return a.port_id == b.port_id; });
        if (! subscription) return -4;
        int_fast8_t result = udpardRxSubscriptionInit(&subscription->subscription, port_id, extent, adapter_->memory_resources);
        return result >=0 ? 1 : result;
    }

    int8_t cyphalRxUnsubscribe(const CyphalTransferKind transfer_kind,
                               const CyphalPortID port_id)
    {
        UdpardPortSubscription stub = { port_id, {}};
        UdpardPortSubscription *subscription = adapter_->subscriptions.find_or_create(stub, [](const UdpardPortSubscription &a, const UdpardPortSubscription &b)
                                                                                       { return a.port_id == b.port_id; });
        if (! subscription) return 0;
        
        udpardRxSubscriptionFree(&subscription->subscription);
        adapter_->subscriptions.remove(subscription);
        return 1;
    }
};
