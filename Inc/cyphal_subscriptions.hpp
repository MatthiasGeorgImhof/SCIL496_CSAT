#ifndef CYPHAL_SUBSCRIPTIONS_HPP_
#define CYPHAL_SUBSCRIPTIONS_HPP_

#include <array>
#include <algorithm>
#include <utility>

#include "cyphal.hpp"
#include "nunavut_assert.h"
#include "uavcan/node/port/List_1_0.h"
#include "uavcan/node/Heartbeat_1_0.h"

constexpr static std::array<CyphalSubscription, 2> CYPHAL_MESSAGES =
    {
        CyphalSubscription{uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_, uavcan_node_Heartbeat_1_0_EXTENT_BYTES_, CyphalTransferKindMessage},
        CyphalSubscription{uavcan_node_port_List_1_0_FIXED_PORT_ID_, uavcan_node_port_List_1_0_EXTENT_BYTES_, CyphalTransferKindMessage}
    };

constexpr CyphalSubscription const *findByPortIdCompileTime(CyphalPortID port_id)
{
    for (auto const &item : CYPHAL_MESSAGES)
    {
        if (item.port_id == port_id)
        {
            return &item;
        }
    }
    return nullptr;
}

CyphalSubscription const *findByPortIdRuntime(CyphalPortID port_id)
{
    for (auto const &item : CYPHAL_MESSAGES)
    {
        if (item.port_id == port_id)
        {
            return &item;
        }
    }
    return nullptr;
}

#endif // CYPHAL_SUBSCRIPTIONS_HPP_