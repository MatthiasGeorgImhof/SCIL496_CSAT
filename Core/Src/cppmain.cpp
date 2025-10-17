#include <cppmain.h>
#include <cpphal.h>
#include "usb_device.h"

#include <memory>

#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "usbd_cdc_if.h"

#include "o1heap.h"
#include "canard.h"
#include "serard.h"

#include "cyphal.hpp"
#include "canard_adapter.hpp"
#include "serard_adapter.hpp"
#include "loopard_adapter.hpp"

#ifndef NUNAVUT_ASSERT
#define NUNAVUT_ASSERT(x) assert(x)
#endif
#include "uavcan/diagnostic/Record_1_1.h"

#include <CircularBuffer.hpp>
#include <ArrayList.hpp>
#include "Allocator.hpp"
#include "RegistrationManager.hpp"
#include "ServiceManager.hpp"
#include "SubscriptionManager.hpp"
#include "ProcessRxQueue.hpp"
#include "TaskCheckMemory.hpp"
#include "TaskBlinkLED.hpp"
#include "TaskSendHeartBeat.hpp"
#include "TaskProcessHeartBeat.hpp"
#include "TaskSendNodePortList.hpp"
#include "TaskSubscribeNodePortList.hpp"
#include "TaskRespondGetInfo.hpp"
#include "TaskRequestGetInfo.hpp"
#include "PowerSwitch.hpp"
#include "PowerMonitor.hpp"

#include "au.hh"
#include "au.hpp"

#include "Logger.hpp"

constexpr size_t O1HEAP_SIZE = 65536;
uint8_t o1heap_buffer[O1HEAP_SIZE] __attribute__ ((aligned (O1HEAP_ALIGNMENT)));
O1HeapInstance *o1heap;

void* canardMemoryAllocate(CanardInstance *const /*canard*/, const size_t size)
{
	return o1heapAllocate(o1heap, size);
}

void canardMemoryDeallocate(CanardInstance *const /*canard*/, void *const pointer)
{
	o1heapFree(o1heap, pointer);
}

void* serardMemoryAllocate(void *const /*user_reference*/, const size_t size)
{
	return o1heapAllocate(o1heap, size);
}

void serardMemoryDeallocate(void *const /*user_reference*/, const size_t /*size*/, void *const pointer)
{
	o1heapFree(o1heap, pointer);
};

#ifndef CYPHAL_NODE_ID
#define CYPHAL_NODE_ID 21
#endif

constexpr CyphalNodeID cyphal_node_id = CYPHAL_NODE_ID;

constexpr size_t CAN_RX_BUFFER_SIZE = 64;
CircularBuffer<CanRxFrame, CAN_RX_BUFFER_SIZE> can_rx_buffer;

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	uint32_t num_messages = HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0);
	log(LOG_LEVEL_TRACE, "HAL_CAN_RxFifo0MsgPendingCallback %d\r\n", num_messages);
	for(uint32_t n=0; n<num_messages; ++n)
	{
		if (can_rx_buffer.is_full()) return;

		CanRxFrame &frame = can_rx_buffer.next();
		HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &frame.header, frame.data);
	}
}

uint16_t endian_swap(uint16_t num) {return (num>>8) | (num<<8); };
int16_t endian_swap(int16_t num) {return (num>>8) | (num<<8); };

void cppmain()
{
	if (HAL_CAN_Start(&hcan1) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
	{
		Error_Handler();
	}

	CAN_FilterTypeDef filter = {};
	filter.FilterIdHigh = 0x1fff;
	filter.FilterIdLow = 0xffff;
	filter.FilterMaskIdHigh = 0;
	filter.FilterMaskIdLow = 0;
	filter.FilterFIFOAssignment = CAN_RX_FIFO0;
	filter.FilterBank = 0;
	filter.FilterMode = CAN_FILTERMODE_IDMASK;
	filter.FilterScale = CAN_FILTERSCALE_16BIT;
	filter.FilterActivation = ENABLE;
	filter.SlaveStartFilterBank = 0;
	HAL_CAN_ConfigFilter(&hcan1, &filter);

	o1heap = o1heapInit(o1heap_buffer, O1HEAP_SIZE);
	O1HeapAllocator<CanardRxTransfer> alloc(o1heap);

	LoopardAdapter loopard_adapter;
	Cyphal<LoopardAdapter> loopard_cyphal(&loopard_adapter);
	loopard_cyphal.setNodeID(cyphal_node_id);

	CanardAdapter canard_adapter;
	canard_adapter.ins = canardInit(&canardMemoryAllocate, &canardMemoryDeallocate);
	canard_adapter.que = canardTxInit(512, CANARD_MTU_CAN_CLASSIC);
	Cyphal<CanardAdapter> canard_cyphal(&canard_adapter);
	canard_cyphal.setNodeID(cyphal_node_id);

//	Logger::setCyphalCanardAdapter(&canard_cyphal);
	std::tuple<Cyphal<CanardAdapter>> canard_adapters = { canard_cyphal };
	std::tuple<> empty_adapters = {} ;

	RegistrationManager registration_manager;
	SubscriptionManager subscription_manager;
	registration_manager.subscribe(uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_);
	registration_manager.subscribe(uavcan_node_port_List_1_0_FIXED_PORT_ID_);
	registration_manager.subscribe(uavcan_diagnostic_Record_1_1_FIXED_PORT_ID_);
	registration_manager.publish(uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_);
	registration_manager.publish(uavcan_node_port_List_1_0_FIXED_PORT_ID_);
	registration_manager.publish(uavcan_diagnostic_Record_1_1_FIXED_PORT_ID_);

	O1HeapAllocator<TaskSendHeartBeat<Cyphal<CanardAdapter>>> alloc_TaskSendHeartBeat(o1heap);
	registration_manager.add(allocate_unique_custom<TaskSendHeartBeat<Cyphal<CanardAdapter>>>(alloc_TaskSendHeartBeat, 2000, 100, 0, canard_adapters));

	O1HeapAllocator<TaskProcessHeartBeat<Cyphal<CanardAdapter>>> alloc_TaskProcessHeartBeat(o1heap);
	registration_manager.add(allocate_unique_custom<TaskProcessHeartBeat<Cyphal<CanardAdapter>>>(alloc_TaskProcessHeartBeat, 2000, 100, canard_adapters));

	O1HeapAllocator<TaskSendNodePortList<Cyphal<CanardAdapter>>> alloc_TaskSendNodePortList(o1heap);
	registration_manager.add(allocate_unique_custom<TaskSendNodePortList<Cyphal<CanardAdapter>>>(alloc_TaskSendNodePortList, &registration_manager, 10000, 100, 0, canard_adapters));

	O1HeapAllocator<TaskSubscribeNodePortList<Cyphal<CanardAdapter>>> alloc_TaskSubscribeNodePortList(o1heap);
	registration_manager.add(allocate_unique_custom<TaskSubscribeNodePortList<Cyphal<CanardAdapter>>>(alloc_TaskSubscribeNodePortList, &subscription_manager, 10000, 100, canard_adapters));

	constexpr uint8_t uuid[] = {0x1a, 0xb7, 0x9f, 0x23, 0x7c, 0x51, 0x4e, 0x0b, 0x8d, 0x69, 0x32, 0xfa, 0x15, 0x0c, 0x6e, 0x41};
	constexpr char node_name[50] = "SCIL496_CSAT";
	O1HeapAllocator<TaskRespondGetInfo<Cyphal<CanardAdapter>>> alloc_TaskRespondGetInfo(o1heap);
	registration_manager.add(allocate_unique_custom<TaskRespondGetInfo<Cyphal<CanardAdapter>>>(alloc_TaskRespondGetInfo, uuid, node_name, 10000, 100, canard_adapters));

	O1HeapAllocator<TaskRequestGetInfo<Cyphal<CanardAdapter>>> alloc_TaskRequestGetInfo(o1heap);
	registration_manager.add(allocate_unique_custom<TaskRequestGetInfo<Cyphal<CanardAdapter>>>(alloc_TaskRequestGetInfo, 10000, 100, 13, 0, canard_adapters));

	O1HeapAllocator<TaskBlinkLED> alloc_TaskBlinkLED(o1heap);
	registration_manager.add(allocate_unique_custom<TaskBlinkLED>(alloc_TaskBlinkLED, GPIOB, LED1_Pin, 1000, 100));

	O1HeapAllocator<TaskCheckMemory> alloc_TaskCheckMemory(o1heap);
	registration_manager.add(allocate_unique_custom<TaskCheckMemory>(alloc_TaskCheckMemory, o1heap, 2000, 100));

    subscription_manager.subscribe<SubscriptionManager::MessageTag>(registration_manager.getSubscriptions(), canard_adapters);
    subscription_manager.subscribe<SubscriptionManager::ResponseTag>(registration_manager.getServers(), canard_adapters);
    subscription_manager.subscribe<SubscriptionManager::RequestTag>(registration_manager.getClients(), canard_adapters);

    ServiceManager service_manager(registration_manager.getHandlers());
	service_manager.initializeServices(HAL_GetTick());

	constexpr uint8_t INA226 = 64 + 1;
//	constexpr uint8_t INS226_CONFIG = 0x00;
//	constexpr uint8_t INS226_SHUNT = 0x01;
//	constexpr uint8_t INS226_BUS = 0x02;
//	constexpr uint8_t INS226_POWER = 0x03;
//	constexpr uint8_t INS226_CURRENT = 0x04;
//	constexpr uint8_t INS226_CALIBRATION = 0x05;
//	constexpr uint8_t INS226_MANUFACTURER_ID = 0xfe;
//	constexpr uint8_t INS226_DIE_ID = 0xff;

//	constexpr uint8_t GPIO_EXPANDER = 32;
//	constexpr uint8_t ENABLE3 = 0x01;
//	constexpr uint8_t ENABLE2 = 0x04;
//	constexpr uint8_t ENABLE1 = 0x10;
//	constexpr uint8_t ENABLE0 = 0x40;

//	HAL_GPIO_WritePin(GPIOB, POWER_RST_Pin, GPIO_PIN_SET);
//	HAL_Delay(1);
//	PowerSwitch power_switch(hi2c4_, GPIO_EXPANDER);
//	power_switch.on(2);
//
//	PowerMonitor power_monitor(hi2c4_, INA226);

	O1HeapAllocator<CyphalTransfer> allocator(o1heap);
	LoopManager loop_manager(allocator);
	while(1)
	{
		log(LOG_LEVEL_TRACE, "while loop: %d\r\n", HAL_GetTick());
		log(LOG_LEVEL_TRACE, "RegistrationManager: (%d %d) (%d %d) \r\n",
				registration_manager.getHandlers().capacity(), registration_manager.getHandlers().size(),
				registration_manager.getSubscriptions().capacity(), registration_manager.getSubscriptions().size());
		log(LOG_LEVEL_TRACE, "ServiceManager: (%d %d) \r\n",
				service_manager.getHandlers().capacity(), service_manager.getHandlers().size());
		log(LOG_LEVEL_TRACE, "CanProcessRxQueue: (%d %d) \r\n",
				can_rx_buffer.capacity(), can_rx_buffer.size());
		loop_manager.CanProcessTxQueue(&canard_adapter, &hcan1);
		loop_manager.CanProcessRxQueue(&canard_cyphal, &service_manager, empty_adapters, can_rx_buffer);
		loop_manager.LoopProcessRxQueue(&loopard_cyphal, &service_manager, empty_adapters);
		service_manager.handleServices();

//		PowerMonitorData data;
//		power_monitor(data);
//		char buffer[256];
//		sprintf(buffer, "INA226: %4x %4x % 6d % 6d % 6d % 6d\r\n",
//			  data.manufacturer_id, data.die_id, data.voltage_shunt_uV, data.voltage_bus_mV, data.power_mW, data.current_uA);
//		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));


		HAL_Delay(100);
	}
}
