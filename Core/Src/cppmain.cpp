#include "main.h"
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

UART_HandleTypeDef *huart2_;
DMA_HandleTypeDef *hdma_usart2_rx_;
DMA_HandleTypeDef *hdma_usart2_tx_;
UART_HandleTypeDef *huart3_;
DMA_HandleTypeDef *hdma_usart3_rx_;
DMA_HandleTypeDef *hdma_usart3_tx_;

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

bool serialSendHuart2(void* user_reference, uint8_t data_size, const uint8_t* data)
{
	return (HAL_UART_Transmit(huart2_, data, data_size, 1000) == HAL_OK);
}

bool serialSendHuart3(void* user_reference, uint8_t data_size, const uint8_t* data)
{
	return (HAL_UART_Transmit(huart3_, data, data_size, 1000) == HAL_OK);
}

constexpr CyphalNodeID cyphal_node_id = 11;


constexpr uint32_t SERIAL_TIMEOUT = 1000;
constexpr size_t SERIAL_BUFFER_SIZE = 4;
using SerialCircularBuffer = CircularBuffer<SerialFrame, SERIAL_BUFFER_SIZE>;
SerialCircularBuffer serial_buffer;

constexpr size_t CAN_RX_BUFFER_SIZE = 32;
CircularBuffer<CanRxFrame, CAN_RX_BUFFER_SIZE> can_rx_buffer;

CanardTransferMetadata convert(const SerardTransferMetadata serard)
{
	CanardTransferMetadata canard;
	canard.port_id = serard.port_id;
	canard.priority = static_cast<CanardPriority>(serard.priority);
	canard.remote_node_id = serard.remote_node_id;
	canard.transfer_id = serard.transfer_id;
	canard.transfer_kind = static_cast<CanardTransferKind>(serard.transfer_kind);
	return canard;
}

CanardRxTransfer convert(const SerardRxTransfer serard)
{
	CanardRxTransfer canard;
	canard.metadata = convert(serard.metadata);
	canard.payload_size = serard.payload_size;
	canard.timestamp_usec = serard.timestamp_usec;
	canard.payload = serard.payload;
	return canard;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t pos)
{
	HAL_UART_RxEventTypeTypeDef event_type = HAL_UARTEx_GetRxEventType(huart);
	if (event_type == HAL_UART_RXEVENT_HT) return;
	SerialFrame frame = serial_buffer.peek();
	frame.size = pos;

	frame = serial_buffer.next();
	HAL_UARTEx_ReceiveToIdle_DMA(huart, frame.data, SERIAL_MTU);
}

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
bool serial_send(void* user_reference, uint8_t data_size, const uint8_t* data)
{
	HAL_StatusTypeDef status = HAL_UART_Transmit(huart2_, data, data_size, 1000);
// 	HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(&huart2_, data, data_size);

 	return status == HAL_OK;;
}
#ifdef __cplusplus
}
#endif


void cppmain(HAL_Handles handles)
{
	huart2_ = handles.huart2;
	huart3_ = handles.huart3;

	o1heap = o1heapInit(o1heap_buffer, O1HEAP_SIZE);
	O1HeapAllocator<CanardRxTransfer> alloc(o1heap);

    LoopardAdapter loopard_adapter;
    Cyphal<LoopardAdapter> loopard_cyphal(&loopard_adapter);
    loopard_cyphal.setNodeID(cyphal_node_id);

////    CanardAdapter canard_adapter;
////    canard_adapter.ins = canardInit(&canardMemoryAllocate, &canardMemoryDeallocate);
////    canard_adapter.que = canardTxInit(16, CANARD_MTU_CAN_CLASSIC);
////    Cyphal<CanardAdapter> canard_cyphal(&canard_adapter);
////    canard_cyphal.setNodeID(cyphal_node_id);
//
    SerardAdapter serard_adapter;
    struct SerardMemoryResource serard_memory_resource = {&serard_adapter.ins, serardMemoryDeallocate, serardMemoryAllocate};
    serard_adapter.ins = serardInit(serard_memory_resource, serard_memory_resource);
    serard_adapter.emitter = serial_send;
    Cyphal<SerardAdapter> serard_cyphal(&serard_adapter);
    serard_cyphal.setNodeID(cyphal_node_id);

	std::tuple<Cyphal<SerardAdapter>> adapters = { serard_cyphal };

    RegistrationManager registration_manager;

	O1HeapAllocator<TaskSendHeartBeat<Cyphal<SerardAdapter>>> alloc_TaskSendHeartBeat(o1heap);
	registration_manager.add(allocate_unique_custom<TaskSendHeartBeat<Cyphal<SerardAdapter>>>(alloc_TaskSendHeartBeat, 1000, 100, 0, adapters));

	O1HeapAllocator<TaskBlinkLED> alloc_TaskBlinkLED(o1heap);
	registration_manager.add(allocate_unique_custom<TaskBlinkLED>(alloc_TaskBlinkLED, GPIOC, LED1_Pin, 1000, 100));

	O1HeapAllocator<TaskCheckMemory> alloc_TaskCheckMemory(o1heap);
	registration_manager.add(allocate_unique_custom<TaskCheckMemory>(alloc_TaskCheckMemory, o1heap, 2000, 100));

	ServiceManager service_manager(registration_manager.getHandlers());

	O1HeapAllocator<CyphalTransfer> allocator(o1heap);
    LoopManager loop_manager(allocator);
	while(1)
	{
		log(LOG_LEVEL_TRACE, "while loop: %d\r\n", HAL_GetTick());
		loop_manager.SerialProcessRxQueue(&serard_cyphal, &service_manager, adapters, serial_buffer);
		loop_manager.LoopProcessRxQueue(&loopard_cyphal, &service_manager, adapters);
		service_manager.handleServices();
	}
}
