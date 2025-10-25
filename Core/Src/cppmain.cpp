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
#include "Transport.hpp"
#include "I2CSwitch.hpp"
#include "CameraSwitch.hpp"
#include "OV5640.hpp"

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

	constexpr uint8_t GPIO_EXPANDER = 32;
    using PowerSwitchConfig = I2C_Config<hi2c4, GPIO_EXPANDER>;
    using PowerSwitchTransport = I2CTransport<PowerSwitchConfig>;
    PowerSwitchTransport ps_transport;
	PowerSwitch<PowerSwitchTransport> power_switch(ps_transport, GPIOB, POWER_RST_Pin);
	power_switch.on(CIRCUITS::CIRCUIT_0);

	constexpr uint8_t INA226 = 64;
    using PowerMonitorConfig = I2C_Config<hi2c4, INA226>;
    using PowerMonitorTransport = I2CTransport<PowerMonitorConfig>;
    PowerMonitorTransport pm_transport;
	PowerMonitor<PowerMonitorTransport> power_monitor(pm_transport);

	HAL_GPIO_WritePin(ENABLE_1V5_GPIO_Port, ENABLE_1V5_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(ENABLE_2V8_GPIO_Port, ENABLE_2V8_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(ENABLE_A_GPIO_Port, ENABLE_A_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(ENABLE_B_GPIO_Port, ENABLE_B_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(ENABLE_C_GPIO_Port, ENABLE_C_Pin, GPIO_PIN_SET);

	O1HeapAllocator<CyphalTransfer> allocator(o1heap);
	LoopManager loop_manager(allocator);

//	constexpr uint8_t CAMERA_SWITCH = 0x70;
//    using CameraSwitchConfig = I2C_Config<hi2c1, CAMERA_SWITCH>;
//    using CameraSwitchTransport = I2CTransport<CameraSwitchConfig>;
//    CameraSwitchTransport cam_sw_transport;
//	CameraSwitch<CameraSwitchTransport> camera_switch(cam_sw_transport, I2C1_RST_GPIO_Port, I2C1_RST_Pin, ENABLE_A_GPIO_Port, { ENABLE_A_Pin, ENABLE_B_Pin, ENABLE_C_Pin, ENABLE_C_Pin});
//	camera_switch.select(I2CSwitchChannel::Channel2);
//
//	constexpr uint8_t CAMERA = OV5640_ID;
//    using CameraConfig = I2C_Config<hi2c1, CAMERA>;
//    using CameraTransport = I2CTransport<CameraConfig>;
//    CameraTransport cam_transport;
//    using CameraClockOE = GpioPin<&GPIOC_object, CAMERA_HW_CLK_Pin>;
//    CameraClockOE cam_clock;
//    using CameraPowerDn = GpioPin<&GPIOB_object, CAMERA_PWR_DN_Pin>;
//    CameraPowerDn cam_powrdn;
//    using CameraReset = GpioPin<&GPIOB_object, CAMERA_RST_Pin>;
//    CameraReset cam_reset;
//    OV5640<CameraTransport, CameraClockOE, CameraPowerDn, CameraReset> camera(cam_transport, cam_clock, cam_powrdn, cam_reset);
//    camera.powerUp();

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

//		uint16_t camera_id{0};
//		camera.readRegister(OV5640_Register::CHIP_ID, reinterpret_cast<uint8_t*>(&camera_id), sizeof(camera_id));
//		char buffer[256];
//		sprintf(buffer, "OV5640: %04x\r\n", camera_id);
//		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));

//		uint8_t camera_id_h = camera.readRegister(OV5640_Register::CHIP_ID_H);
//		uint8_t camera_id_l = camera.readRegister(OV5640_Register::CHIP_ID_L);
//		char buffer[256];
//		sprintf(buffer, "OV5640: %02x %02x\r\n", camera_id_h, camera_id_l);
//		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));


//		uint16_t adc_value = 0;
//
//		// Start ADC conversion
//		if (HAL_ADC_Start(&hadc1) != HAL_OK)
//		{
//		    Error_Handler(); // Handle start error
//		}
//
//		// Wait for conversion to complete
//		if (HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY) == HAL_OK)
//		{
//		    // Read the converted value
//		    adc_value = HAL_ADC_GetValue(&hadc1);
//		}
//
//		// Stop ADC (optional but good practice)
//		HAL_ADC_Stop(&hadc1);
//
//		PowerMonitorData data;
//		power_monitor(data);
//		char buffer[256];
//		sprintf(buffer, "INA226: %4x %4x % 6d % 6d % 6d % 6d (%6u %6u)\r\n",
//			  data.manufacturer_id, data.die_id, data.voltage_shunt_uV, data.voltage_bus_mV, data.power_mW, data.current_uA, adc_value&0xfff, adc_value&0xfff);
//		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));


		HAL_Delay(100);
	}
}
