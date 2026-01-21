#ifndef __CAN_CALL_BACKS_HPP_
#define __CAN_CALL_BACKS_HPP_

#include "cyphal.hpp"
#include "canard_adapter.hpp"
#include "serard_adapter.hpp"
#include "loopard_adapter.hpp"

#ifndef __arm____
#include "stm32l4xx_hal.h"
#else
#include "mock_hal_hal.h"
#endif

constexpr size_t SERIAL_MTU = 640;
constexpr size_t CAN_MTU = 8;

struct SerialFrame
{
    size_t size;
    uint8_t data[SERIAL_MTU];
};

struct CanRxFrame
{
    CAN_RxHeaderTypeDef header;
    uint8_t data[CAN_MTU];
};

extern CanardAdapter canard_adapter;

constexpr size_t CAN_RX_BUFFER_SIZE = 64;
using CanRxBuffer = CircularBuffer<CanRxFrame, CAN_RX_BUFFER_SIZE>;
static CanRxBuffer can_rx_buffer;

void drain_canard_tx_queue(CAN_HandleTypeDef *hcan)
{
    const CanardTxQueueItem* ti = nullptr;

    while ((ti = canardTxPeek(&canard_adapter.que)) != nullptr)
    {
    	log(LOG_LEVEL_ERROR, "drain_canard_tx_queue: buffer size=%d\r\n", canard_adapter.que.size);
    	if (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0) break;

        CAN_TxHeaderTypeDef header;
        header.ExtId = ti->frame.extended_can_id;
        header.DLC   = ti->frame.payload_size;
        header.RTR   = CAN_RTR_DATA;
        header.IDE   = CAN_ID_EXT;

        uint32_t mailbox;
        HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(hcan, &header, (uint8_t*)ti->frame.payload, &mailbox);

        if (status != HAL_OK)
        {
            // Drop the frame to avoid memory leak
            log(LOG_LEVEL_ERROR, "TX fail in callback, dropping frame extid=%08lx status=%d\r\n", header.ExtId, status);
        }

        // pop and free
        canard_adapter.ins.memory_free(&canard_adapter.ins, canardTxPop(&canard_adapter.que, ti));
    }
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
    log(LOG_LEVEL_TRACE, "HAL_CAN_TxMailbox0CompleteCallback\r\n");
    drain_canard_tx_queue(hcan);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
    log(LOG_LEVEL_TRACE, "HAL_CAN_TxMailbox1CompleteCallback\r\n");
    drain_canard_tx_queue(hcan);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
    log(LOG_LEVEL_TRACE, "HAL_CAN_TxMailbox2CompleteCallback\r\n");
    drain_canard_tx_queue(hcan);
}

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

#endif // __CAN_CALL_BACKS_HPP_
