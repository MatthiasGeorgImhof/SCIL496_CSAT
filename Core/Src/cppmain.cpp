#include "main.h"
//#include "usb_device.h"

#include <memory>

#include "stdio.h"
#include "stdint.h"
#include "string.h"
//#include "usbd_cdc_if.h"

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

#include <CircularBuffer.hpp>
#include <ArrayList.hpp>
#include "Allocator.hpp"
#include "RegistrationManager.hpp"
#include "ServiceManager.hpp"
#include "ProcessRxQueue.hpp"
#include "TaskCheckMemory.hpp"
#include "TaskBlinkLED.hpp"
#include "TaskSendHeartBeat.hpp"

#include <cppmain.h>
#include "Logger.hpp"

CAN_HandleTypeDef *hcan1_;
CAN_HandleTypeDef *hcan2_;

constexpr size_t O1HEAP_SIZE = 16384;
uint8_t o1heap_buffer[O1HEAP_SIZE] __attribute__ ((aligned (O1HEAP_ALIGNMENT)));
O1HeapInstance *o1heap;

void* canardMemoryAllocate(CanardInstance *const canard, const size_t size)
{
	return o1heapAllocate(o1heap, size);
}

void canardMemoryDeallocate(CanardInstance *const canard, void *const pointer)
{
	o1heapFree(o1heap, pointer);
}

void* serardMemoryAllocate(void *const user_reference, const size_t size)
{
	return o1heapAllocate(o1heap, size);
}

void serardMemoryDeallocate(void *const user_reference, const size_t size, void *const pointer)
{
	o1heapFree(o1heap, pointer);
};

#ifndef CYPHAL_NODE_ID
#define CYPHAL_NODE_ID 21
#endif

constexpr CyphalNodeID cyphal_node_id = CYPHAL_NODE_ID;

constexpr size_t CAN_RX_BUFFER_SIZE = 32;
CircularBuffer<CanRxFrame, CAN_RX_BUFFER_SIZE> can_rx_buffer;

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	uint32_t num_messages = HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0);
	for(uint32_t n=0; n<num_messages; ++n)
	{
		if (can_rx_buffer.is_full()) return;

		CanRxFrame &frame = can_rx_buffer.next();
		HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &frame.header, frame.data);
	}
}

#ifdef __cplusplus
extern "C" {
#endif
void log_string(unsigned int level, const char *message, const char *str)
{
	log(level, message, str);
}

int uchar_buffer_to_hex(const unsigned char* uchar_buffer, size_t uchar_len, char* hex_string_buffer, size_t hex_string_buffer_size) {
	if (uchar_buffer == NULL || hex_string_buffer == NULL || uchar_len == 0 || hex_string_buffer_size == 0) {
		return -1; // Error: Invalid input
	}

	// Calculate the required buffer size (2 hex chars per byte + 1 space per byte + null terminator)
	size_t required_size = uchar_len * 3 + 1;

	if (hex_string_buffer_size < required_size) {
		return -1; // Error: Buffer too small
	}

	// Convert each uchar to its hexadecimal representation with spaces
	for (size_t i = 0; i < uchar_len; i++) {
		snprintf(hex_string_buffer + (i * 3), 3, "%02X", uchar_buffer[i]);  //Write 2 hex characters
		hex_string_buffer[(i * 3) + 2] = ' ';                            //Write a space after the hex characters
	}

	hex_string_buffer[uchar_len * 3 -1] = '\0'; //Remove trailing space and Null-terminate the string

	return 0; // Success
}

#ifdef __cplusplus
}
#endif


void cppmain(HAL_Handles handles)
{
	hcan1_ = handles.hcan1;
	hcan2_ = handles.hcan2;

	HAL_GPIO_WritePin(GPIOB, CAN1_STB_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, CAN1_SHTD_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, CAN2_STB_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, CAN2_SHTD_Pin, GPIO_PIN_RESET);

	if (HAL_CAN_Start(hcan1_) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_CAN_ActivateNotification(hcan1_, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
	{
		Error_Handler();
	}

	CAN_FilterTypeDef filter = { 0 };
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
	HAL_CAN_ConfigFilter(hcan1_, &filter);

	o1heap = o1heapInit(o1heap_buffer, O1HEAP_SIZE);
	O1HeapAllocator<CanardRxTransfer> alloc(o1heap);

	LoopardAdapter loopard_adapter;
	Cyphal<LoopardAdapter> loopard_cyphal(&loopard_adapter);
	loopard_cyphal.setNodeID(cyphal_node_id);

	CanardAdapter canard_adapter;
	canard_adapter.ins = canardInit(&canardMemoryAllocate, &canardMemoryDeallocate);
	canard_adapter.que = canardTxInit(16, CANARD_MTU_CAN_CLASSIC);
	Cyphal<CanardAdapter> canard_cyphal(&canard_adapter);
	canard_cyphal.setNodeID(cyphal_node_id);

	std::tuple<Cyphal<CanardAdapter>> canard_adapters = { canard_cyphal };
	std::tuple<> empty_adapters = {} ;

	RegistrationManager registration_manager;

	O1HeapAllocator<TaskSendHeartBeat<Cyphal<CanardAdapter>>> alloc_TaskSendHeartBeat(o1heap);
	registration_manager.add(allocate_unique_custom<TaskSendHeartBeat<Cyphal<CanardAdapter>>>(alloc_TaskSendHeartBeat, 1000, 100, 0, canard_adapters));

	O1HeapAllocator<TaskBlinkLED> alloc_TaskBlinkLED(o1heap);
	registration_manager.add(allocate_unique_custom<TaskBlinkLED>(alloc_TaskBlinkLED, GPIOC, LED1_Pin, 1000, 100));

	O1HeapAllocator<TaskCheckMemory> alloc_TaskCheckMemory(o1heap);
	registration_manager.add(allocate_unique_custom<TaskCheckMemory>(alloc_TaskCheckMemory, o1heap, 2000, 100));

	ServiceManager service_manager(registration_manager.getHandlers());
	service_manager.initializeServices(HAL_GetTick());

	O1HeapAllocator<CyphalTransfer> allocator(o1heap);
	LoopManager loop_manager(allocator);
	while(1)
	{
		log(LOG_LEVEL_TRACE, "while loop: %d\r\n", HAL_GetTick());
		loop_manager.CanProcessTxQueue(&canard_adapter, hcan1_);
		loop_manager.LoopProcessRxQueue(&loopard_cyphal, &service_manager, empty_adapters);
		service_manager.handleServices();
		HAL_Delay(100);
	}
}
