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
uint8_t o1heap_buffer[O1HEAP_SIZE] __attribute__ ((aligned (256)));
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
	free(pointer);
};

bool serialSendHuart2(void* user_reference, uint8_t data_size, const uint8_t* data)
{
	return (HAL_UART_Transmit(huart2_, data, data_size, 1000) == HAL_OK);
}

bool serialSendHuart3(void* user_reference, uint8_t data_size, const uint8_t* data)
{
	return (HAL_UART_Transmit(huart3_, data, data_size, 1000) == HAL_OK);
}

constexpr CanardNodeID canard_node_id = 0x6f;
constexpr SerardNodeID serard_node_id = static_cast<SerardNodeID>(canard_node_id);

constexpr uint32_t SERIAL_TIMEOUT = 1000;
constexpr size_t SERIAL_BUFFER_SIZE = 4;
//constexpr size_t SERIAL_MTU = 640;
//struct SerialFrame { size_t size; uint8_t data[SERIAL_MTU]; };
CircularBuffer<SerialFrame, SERIAL_BUFFER_SIZE> serial_buffer;

constexpr size_t CAN_RX_BUFFER_SIZE = 32;
//constexpr size_t CAN_MTU = 8;
//struct CanRxFrame { CAN_RxHeaderTypeDef header; uint8_t data[CAN_MTU]; };
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


void cppmain(HAL_Handles handles)
{
	huart2_ = handles.huart2;
	huart3_ = handles.huart3;

	o1heap = o1heapInit(o1heap_buffer, O1HEAP_SIZE);
	O1HeapAllocator<CanardRxTransfer> alloc(o1heap);

    LoopardAdapter loopard_adapter;
    Cyphal<LoopardAdapter> loopard_cyphal(&loopard_adapter);
    loopard_cyphal.setNodeID(11);

    CanardAdapter canard_adapter;
    canard_adapter.ins = canardInit(&canardMemoryAllocate, &canardMemoryDeallocate);
    canard_adapter.ins.node_id = 11;
    canard_adapter.que = canardTxInit(16, CANARD_MTU_CAN_CLASSIC);
    Cyphal<CanardAdapter> canard_cyphal(&canard_adapter);
    canard_cyphal.setNodeID(12);

	RegistrationManager registration_manager;

	std::tuple<Cyphal<LoopardAdapter>, Cyphal<CanardAdapter>> adapters = { loopard_cyphal, canard_cyphal};
	O1HeapAllocator<TaskSendHeartBeat<Cyphal<LoopardAdapter>, Cyphal<CanardAdapter>>> alloc_TaskSendHeartBeat(o1heap);
	registration_manager.add(allocate_unique_custom<TaskSendHeartBeat<Cyphal<LoopardAdapter>, Cyphal<CanardAdapter>>>(alloc_TaskSendHeartBeat, 1000, 100, 0, adapters));

	ServiceManager service_manager(registration_manager.getHandlers());

	log(LOG_LEVEL_INFO, "asfd");

	log(LOG_LEVEL_INFO, "Logger test");
	while(1)
	{
		log(LOG_LEVEL_INFO, "while loop");
		service_manager.handleServices();
	}
}
