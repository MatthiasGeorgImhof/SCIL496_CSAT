#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <cyphal_subscriptions.hpp>
#include "cyphal.hpp"
#include "nunavut_assert.h"
#include "uavcan/node/port/List_1_0.h"
#include "uavcan/node/Heartbeat_1_0.h"
#include "uavcan/node/GetInfo_1_0.h"

TEST_CASE("findByPortIdCompileTime - Found")
{
constexpr CyphalSubscription const *result = findByPortIdCompileTime<CYPHAL_MESSAGES.size(), uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_>(CYPHAL_MESSAGES);
REQUIRE(result != nullptr);
CHECK(result->port_id == uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_);
CHECK(result->extent == uavcan_node_Heartbeat_1_0_EXTENT_BYTES_);
CHECK(result->transfer_kind == CyphalTransferKindMessage);
}

TEST_CASE("findByPortIdCompileTime - Not Found")
{
constexpr CyphalSubscription const *result = findByPortIdCompileTime<CYPHAL_MESSAGES.size(), 999>(CYPHAL_MESSAGES); // Non-existent port ID
REQUIRE(result == nullptr);
}

TEST_CASE("findByPortIdCompileTime - Compile time assert")
{
constexpr CyphalSubscription const *result = findByPortIdCompileTime<CYPHAL_MESSAGES.size(), uavcan_node_port_List_1_0_FIXED_PORT_ID_>(CYPHAL_MESSAGES);
REQUIRE(result != nullptr);
static_assert(result->port_id == uavcan_node_port_List_1_0_FIXED_PORT_ID_, "Port ID mismatch at compile time!");
static_assert(result->extent == uavcan_node_port_List_1_0_EXTENT_BYTES_, "Extent Bytes mismatch at compile time!");
}

TEST_CASE("findByPortIdRuntime - Found")
{
CyphalSubscription const *result = findByPortIdRuntime(CYPHAL_MESSAGES, uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_);
REQUIRE(result != nullptr);
CHECK(result->port_id == uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_);
CHECK(result->extent == uavcan_node_Heartbeat_1_0_EXTENT_BYTES_);
CHECK(result->transfer_kind == CyphalTransferKindMessage);
}

TEST_CASE("findByPortIdRuntime - Not Found")
{
CyphalSubscription const *result = findByPortIdRuntime(CYPHAL_MESSAGES, 999); // Non-existent port ID
REQUIRE(result == nullptr);
}

TEST_CASE("findByPortIdRuntime - Different Port ID")
{
CyphalSubscription const *result = findByPortIdRuntime(CYPHAL_MESSAGES, uavcan_node_port_List_1_0_FIXED_PORT_ID_);
REQUIRE(result != nullptr);
CHECK(result->port_id == uavcan_node_port_List_1_0_FIXED_PORT_ID_);
CHECK(result->extent == uavcan_node_port_List_1_0_EXTENT_BYTES_);
CHECK(result->transfer_kind == CyphalTransferKindMessage);
}

TEST_CASE("findByPortIdCompileTime - Found in CYPHAL_REQUESTS")
{
constexpr CyphalSubscription const *result = findByPortIdCompileTime<CYPHAL_REQUESTS.size(), uavcan_node_GetInfo_1_0_FIXED_PORT_ID_>(CYPHAL_REQUESTS);
REQUIRE(result != nullptr);
CHECK(result->port_id == uavcan_node_GetInfo_1_0_FIXED_PORT_ID_);
CHECK(result->extent == uavcan_node_GetInfo_Request_1_0_EXTENT_BYTES_);
CHECK(result->transfer_kind == CyphalTransferKindRequest);
}

TEST_CASE("findByPortIdCompileTime - Found in CYPHAL_RESPONSES")
{
constexpr CyphalSubscription const *result = findByPortIdCompileTime<CYPHAL_RESPONSES.size(), uavcan_node_GetInfo_1_0_FIXED_PORT_ID_>(CYPHAL_RESPONSES);
REQUIRE(result != nullptr);
CHECK(result->port_id == uavcan_node_GetInfo_1_0_FIXED_PORT_ID_);
CHECK(result->extent == uavcan_node_GetInfo_Response_1_0_EXTENT_BYTES_);
CHECK(result->transfer_kind == CyphalTransferKindResponse);
}

TEST_CASE("findByPortIdRuntime - Found in CYPHAL_REQUESTS")
{
CyphalSubscription const *result = findByPortIdRuntime(CYPHAL_REQUESTS, uavcan_node_GetInfo_1_0_FIXED_PORT_ID_);
REQUIRE(result != nullptr);
CHECK(result->port_id == uavcan_node_GetInfo_1_0_FIXED_PORT_ID_);
CHECK(result->extent == uavcan_node_GetInfo_Request_1_0_EXTENT_BYTES_);
CHECK(result->transfer_kind == CyphalTransferKindRequest);
}

TEST_CASE("findByPortIdRuntime - Found in CYPHAL_RESPONSES")
{
CyphalSubscription const *result = findByPortIdRuntime(CYPHAL_RESPONSES, uavcan_node_GetInfo_1_0_FIXED_PORT_ID_);
REQUIRE(result != nullptr);
CHECK(result->port_id == uavcan_node_GetInfo_1_0_FIXED_PORT_ID_);
CHECK(result->extent == uavcan_node_GetInfo_Response_1_0_EXTENT_BYTES_);
CHECK(result->transfer_kind == CyphalTransferKindResponse);
}

TEST_CASE("findByPortIdCompileTime - Not Found in CYPHAL_REQUESTS")
{
constexpr CyphalSubscription const *result = findByPortIdCompileTime<CYPHAL_REQUESTS.size(), 999>(CYPHAL_REQUESTS); // Non-existent port ID
REQUIRE(result == nullptr);
}

TEST_CASE("findByPortIdCompileTime - Not Found in CYPHAL_RESPONSES")
{
constexpr CyphalSubscription const *result = findByPortIdCompileTime<CYPHAL_RESPONSES.size(), 999>(CYPHAL_RESPONSES); // Non-existent port ID
REQUIRE(result == nullptr);
}

TEST_CASE("findByPortIdRuntime - Not Found in CYPHAL_REQUESTS")
{
CyphalSubscription const *result = findByPortIdRuntime(CYPHAL_REQUESTS, 999); // Non-existent port ID
REQUIRE(result == nullptr);
}

TEST_CASE("findByPortIdRuntime - Not Found in CYPHAL_RESPONSES")
{
CyphalSubscription const *result = findByPortIdRuntime(CYPHAL_RESPONSES, 999); // Non-existent port ID
REQUIRE(result == nullptr);
}