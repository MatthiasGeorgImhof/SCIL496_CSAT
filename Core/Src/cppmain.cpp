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

#include <cstdint>
#include <cstdio>

bool toHexAscii(const uint8_t* data,
                size_t dataSize,
                char* out,
                size_t outSize,
                size_t bytesPerLine = 16)
{
    if (!data || !out || outSize == 0)
        return false;

    size_t pos = 0;

    for (size_t i = 0; i < dataSize; ++i)
    {
        // Format for one byte: "0xNN" + space or newline
        int written = std::snprintf(
            out + pos,
            (pos < outSize ? outSize - pos : 0),
            "0x%02X",
            data[i]
        );

        if (written < 0 || pos + written >= outSize)
            return false; // buffer too small

        pos += written;

        // Add separator
        bool endOfLine = ((i + 1) % bytesPerLine == 0);
        bool lastByte  = (i + 1 == dataSize);

        if (!lastByte)
        {
            const char* sep = endOfLine ? "\n" : " ";
            size_t sepLen = endOfLine ? 1 : 1;

            if (pos + sepLen >= outSize)
                return false;

            out[pos++] = sep[0];
        }
    }

    // Null‑terminate
    if (pos >= outSize-3)
        return false;

    out[pos+0] = '\n';
    out[pos+1] = '\n';
    out[pos+2] = '\0';
    return true;
}

bool toHexAsciiWords(const uint8_t* data,
                     size_t dataSize,
                     char* out,
                     size_t outSize,
                     size_t wordsPerLine = 8)
{
    if (!data || !out || outSize == 0 || (dataSize % 2) != 0)
        return false;

    size_t pos = 0;
    size_t wordCount = dataSize / 2;

    for (size_t w = 0; w < wordCount; w++)
    {
        uint16_t be = (uint16_t(data[2*w]) << 8) | data[2*w + 1];

        int written = std::snprintf(
            out + pos,
            (pos < outSize ? outSize - pos : 0),
            "0x%04X", be
        );

        if (written < 0 || pos + written >= outSize)
            return false;

        pos += written;

        bool lastWord = (w + 1 == wordCount);
        bool endOfLine = ((w + 1) % wordsPerLine == 0);

        if (!lastWord)
        {
            const char* sep = endOfLine ? ",\n" : ", ";
            size_t sepLen = endOfLine ? 2 : 2;

            if (pos + sepLen >= outSize)
                return false;

            out[pos++] = sep[0];
            out[pos++] = sep[1];
        }
    }

    if (pos + 3 >= outSize)
        return false;

    out[pos++] = '\n';
    out[pos++] = '\n';
    out[pos]   = '\0';

    return true;
}


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
	power_switch.on(CIRCUITS::CIRCUIT_1);
	power_switch.on(CIRCUITS::CIRCUIT_2);
	power_switch.on(CIRCUITS::CIRCUIT_3);

	constexpr uint8_t INA226 = 64;
    using PowerMonitorConfig = I2C_Config<hi2c4, INA226>;
    using PowerMonitorTransport = I2CTransport<PowerMonitorConfig>;
    PowerMonitorTransport pm_transport;
	PowerMonitor<PowerMonitorTransport> power_monitor(pm_transport);

	HAL_GPIO_WritePin(ENABLE_1V5_GPIO_Port, ENABLE_1V5_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(ENABLE_2V8_GPIO_Port, ENABLE_2V8_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(I2C1_RST_GPIO_Port, I2C1_RST_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(ENABLE_A_GPIO_Port, ENABLE_A_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(ENABLE_B_GPIO_Port, ENABLE_B_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(ENABLE_C_GPIO_Port, ENABLE_C_Pin, GPIO_PIN_RESET);

//	constexpr uint8_t CAMERA_SWITCH = 0x70;
//	using I2CSwitchConfig = I2C_Config<hi2c1, CAMERA_SWITCH>;
//	using I2CSwitchTransport = I2CTransport<I2CSwitchConfig>;
//	I2CSwitchTransport cam_sw_transport;
//	I2CSwitch<I2CSwitchTransport> camera_switch(cam_sw_transport, I2C1_RST_GPIO_Port, I2C1_RST_Pin);
//	camera_switch.select(I2CSwitchChannel::Channel2);

	O1HeapAllocator<CyphalTransfer> allocator(o1heap);
	LoopManager loop_manager(allocator);

	constexpr uint8_t CAMERA = OV5640_ID;
    using CameraConfig = I2C_Config<hi2c1, CAMERA>;
    using CameraTransport = I2CTransport<CameraConfig>;
    CameraTransport cam_transport;
    using CameraClockOE = GpioPin<&GPIOC_object, CAMERA_HW_CLK_Pin>;
    CameraClockOE cam_clock;
    using CameraPowerDn = GpioPin<&GPIOB_object, CAMERA_PWR_DN_Pin>;
    CameraPowerDn cam_powrdn;
    using CameraReset = GpioPin<&GPIOB_object, CAMERA_RST_Pin>;
    CameraReset cam_reset;
    OV5640<CameraTransport, CameraClockOE, CameraPowerDn, CameraReset> camera(cam_transport, cam_clock, cam_powrdn, cam_reset);
    camera.powerUp();

	constexpr uint8_t CAMERA_SWITCH = 0x70;
    using CameraSwitchConfig = I2C_Config<hi2c1, CAMERA_SWITCH>;
    using CameraSwitchTransport = I2CTransport<CameraSwitchConfig>;
    CameraSwitchTransport cam_sw_transport;
	CameraSwitch<CameraSwitchTransport> camera_switch(cam_sw_transport, I2C1_RST_GPIO_Port, I2C1_RST_Pin, ENABLE_A_GPIO_Port, { ENABLE_A_Pin, ENABLE_B_Pin, ENABLE_C_Pin, ENABLE_C_Pin});
	camera_switch.select(I2CSwitchChannel::Channel2);
//    	using I2CSwitchConfig = I2C_Config<hi2c1, CAMERA_SWITCH>;
//    	using I2CSwitchTransport = I2CTransport<I2CSwitchConfig>;
//    	I2CSwitchTransport cam_sw_transport;
//    	I2CSwitch<I2CSwitchTransport> camera_switch(cam_sw_transport, I2C1_RST_GPIO_Port, I2C1_RST_Pin);
//    	camera_switch.select(I2CSwitchChannel::Channel2);

//	uint8_t channel = static_cast<uint8_t>(I2CSwitchChannel::Channel2);
//	HAL_I2C_StateTypeDef state1 = HAL_I2C_GetState(&hi2c1);
//	HAL_I2C_ModeTypeDef mode1 = HAL_I2C_GetMode(&hi2c1);
//	HAL_StatusTypeDef code1 = HAL_I2C_Master_Transmit(&hi2c1, CAMERA_SWITCH << 1, &channel, 1, 100);
//	uint32_t error1 = HAL_I2C_GetError(&hi2c1);

	uint counter = 0;
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

//		uint8_t data1 = 0;
//		HAL_I2C_StateTypeDef state1 = HAL_I2C_GetState(&hi2c1);
//		HAL_I2C_ModeTypeDef mode1 = HAL_I2C_GetMode(&hi2c1);
//		HAL_StatusTypeDef code1 = HAL_I2C_Master_Receive(&hi2c1, CAMERA_SWITCH << 1, &data1, 1, 100);
//		uint32_t error1 = HAL_I2C_GetError(&hi2c1);

//		constexpr uint8_t THERMO_I2C_ADR = 0x33;
//		uint8_t data2[3] = {0,0,0};
//		HAL_I2C_StateTypeDef state2 = HAL_I2C_GetState(&hi2c2);
//		HAL_I2C_ModeTypeDef mode2 = HAL_I2C_GetMode(&hi2c2);
////		HAL_StatusTypeDef code2 = HAL_I2C_Master_Receive(&hi2c2, THERMO_I2C_ADR << 1, &data2, 1, 100);
//		HAL_StatusTypeDef code2 = HAL_I2C_Mem_Read(&hi2c2, THERMO_I2C_ADR << 1, 0x2407, I2C_MEMADD_SIZE_16BIT, data2, 3, HAL_MAX_DELAY);
//		uint32_t error2 = HAL_I2C_GetError(&hi2c2);
//
//		char buffer[256];
//		sprintf(buffer, "step %ld: (%x %x) (%x %x %x %x)\r\n", HAL_GetTick(), code1, code2, code2, data2[0], data2[1], data2[2]);
//		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));

		constexpr uint8_t THERMO_I2C_ADR = 0x33;
		constexpr size_t EEPROM_SIZE = 0x340 * 2;
		uint8_t data2[EEPROM_SIZE];
		HAL_I2C_StateTypeDef state2 = HAL_I2C_GetState(&hi2c2);
		HAL_I2C_ModeTypeDef mode2 = HAL_I2C_GetMode(&hi2c2);
		HAL_StatusTypeDef code2 = HAL_I2C_Mem_Read(&hi2c2, THERMO_I2C_ADR << 1, 0x2400, I2C_MEMADD_SIZE_16BIT, data2, EEPROM_SIZE, HAL_MAX_DELAY);
		uint32_t error2 = HAL_I2C_GetError(&hi2c2);

		char buffer[8*EEPROM_SIZE];
		toHexAsciiWords(data2, EEPROM_SIZE, buffer, sizeof(buffer), 16);
		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));

		uint16_t camera_id{0};
		camera.readRegister(OV5640_Register::CHIP_ID, reinterpret_cast<uint8_t*>(&camera_id), sizeof(camera_id));
//		char buffer[256];
//		sprintf(buffer, "OV5640: %04x\r\n", camera_id);
//		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));

		uint8_t camera_id_h = camera.readRegister(OV5640_Register::CHIP_ID_H);
		uint8_t camera_id_l = camera.readRegister(OV5640_Register::CHIP_ID_L);
//		char buffer[256];
//		sprintf(buffer, "Channel: %02x OV5640: %04x %02x %02x\r\n", code2, camera_id, camera_id_h, camera_id_l);
//		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));


//		uint16_t adc_value1 = 0;
//		uint16_t adc_value2 = 0;
//
//		ADC_ChannelConfTypeDef sConfig = {0};
//
//		/* --- 1. READ PC2 (ADC_CHANNEL_3) --- */
//		sConfig.Channel = ADC_CHANNEL_3;
//		sConfig.Rank = ADC_REGULAR_RANK_1;
//		sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5; // Keep the same timing as your Init
//		sConfig.SingleDiff = ADC_SINGLE_ENDED;
//		sConfig.OffsetNumber = ADC_OFFSET_NONE;
//		sConfig.Offset = 0;
//
//		if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
//		    Error_Handler();
//		}
//
//		HAL_ADC_Start(&hadc1);
//		if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
//		    adc_value1 = HAL_ADC_GetValue(&hadc1);
//		}
//		HAL_ADC_Stop(&hadc1);
//
//
//		/* --- 2. READ PC3 (ADC_CHANNEL_4) --- */
//		sConfig.Channel = ADC_CHANNEL_4; // Switch to the other channel
//		// (Rank and SamplingTime stay the same, so we don't strictly need to redefine them,
//		// but it's safe to keep them in the sConfig struct)
//
//		if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
//		    Error_Handler();
//		}
//
//		HAL_ADC_Start(&hadc1);
//		if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
//		    adc_value2 = HAL_ADC_GetValue(&hadc1);
//		}
//		HAL_ADC_Stop(&hadc1);
//
//
//		char buffer[256];
//		sprintf(buffer, "step %ld: (%x %x %x %lx: %x) (%x %x %x %lx: %x) %3x %3x\r\n", HAL_GetTick(), state1, mode1, code1, error1, channel, state2, mode2, code2, error2, data, adc_value1, adc_value2);
//		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));


//		PowerMonitorData data;
//		power_monitor(data);
//		char buffer[256];
//		sprintf(buffer, "INA226: %4x %4x % 6d % 6d % 6d % 6d\r\n",
//			  data.manufacturer_id, data.die_id, data.voltage_shunt_uV, data.voltage_bus_mV, data.power_mW, data.current_uA);
//		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));
//
//				char buffer[256];
//				sprintf(buffer, "time: %ld\r\n", HAL_GetTick());
//				CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));

		HAL_Delay(1000);
		++counter;
	}
}
