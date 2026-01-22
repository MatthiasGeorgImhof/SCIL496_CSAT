#include <cppmain.h>
#include <cpphal.h>
#include "usb_device.h"
#include "stm32l4xx_ll_i2c.h"

#include <memory>
#include <cstdio>
#include <cstdint>
#include <cstring>

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
#include "OV5640.hpp"
#include "OV2640.hpp"
#include "MLX90640.hpp"
#include "NullImageBuffer.hpp"
#include "Trigger.hpp"
#include "TaskMLX90640.hpp"
#include "CameraPowerRails.hpp"
#include "CameraPowerConverters.hpp"
#include "CameraControls.hpp"
#include "HeapAllocation.hpp"

#include "au.hh"
#include "au.hpp"

#include "Logger.hpp"
#include "SCCB.hpp"

constexpr size_t O1HEAP_SIZE = 65536;
using LocalHeap = HeapAllocation<O1HEAP_SIZE>;

LoopardAdapter loopard_adapter;
CanardAdapter canard_adapter;

#ifndef CYPHAL_NODE_ID
#define CYPHAL_NODE_ID 21
#endif
constexpr CyphalNodeID cyphal_node_id = CYPHAL_NODE_ID;

constexpr size_t CAN_RX_BUFFER_SIZE = 64;
using CAN_Circular_Rx_Buffer = CircularBuffer<CanRxFrame, CAN_RX_BUFFER_SIZE>;
CAN_Circular_Rx_Buffer can_rx_buffer;

#ifdef __cplusplus
extern "C" {
#endif

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef */*hcan*/)
{
//	uint32_t num_messages = HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0);
////	log(LOG_LEVEL_TRACE, "HAL_CAN_RxFifo0MsgPendingCallback %d\r\n", num_messages);
//	for(uint32_t n=0; n<num_messages; ++n)
//	{
//		if (can_rx_buffer.is_full()) return;
//
//		CanRxFrame &frame = can_rx_buffer.next();
//		HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &frame.header, frame.data);
//	}
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef */*hcan*/)
{
//    log(LOG_LEVEL_TRACE, "HAL_CAN_TxMailbox0CompleteCallback\r\n");
//    drain_canard_tx_queue(hcan);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef */*hcan*/)
{
//    log(LOG_LEVEL_TRACE, "HAL_CAN_TxMailbox1CompleteCallback\r\n");
//    drain_canard_tx_queue(hcan);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef */*hcan*/)
{
//    log(LOG_LEVEL_TRACE, "HAL_CAN_TxMailbox2CompleteCallback\r\n");
//    drain_canard_tx_queue(hcan);
}

#ifdef __cplusplus
}
#endif


constexpr uint16_t endian_swap(uint16_t num) {return (num>>8) | (num<<8); };
constexpr int16_t endian_swap(int16_t num) {return (num>>8) | (num<<8); };

template<typename T, typename... Args>
static void register_task_with_heap(RegistrationManager& rm, Args&&... args)
{
    static SafeAllocator<T, LocalHeap> alloc;
    rm.add(alloc_unique_custom<T>(alloc, std::forward<Args>(args)...));
}

void cppmain()
{
	HAL_Delay(3000);
	HAL_GPIO_WritePin(GPIOA, LED5_Pin, GPIO_PIN_SET);

	LocalHeap::initialize();

	CAN_FilterTypeDef filter{};
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

	if (HAL_CAN_Start(&hcan1) != HAL_OK) {
	    Error_Handler();
	}

	if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
	{
	    // Error_Handler();
		HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
		uint32_t counter = 0;
		while(1)
		{
			if ((counter % 30000) == 0)
			{
				HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
			}
			counter++;

		}
	}

	using LoopardCyphal = Cyphal<LoopardAdapter>;
	loopard_adapter.memory_allocate = LocalHeap::loopardMemoryAllocate;
	loopard_adapter.memory_free = LocalHeap::loopardMemoryDeallocate;
	LoopardCyphal loopard_cyphal(&loopard_adapter);
	loopard_cyphal.setNodeID(cyphal_node_id);

	using CanardCyphal = Cyphal<CanardAdapter>;
	canard_adapter.ins = canardInit(LocalHeap::canardMemoryAllocate, LocalHeap::canardMemoryDeallocate);
	canard_adapter.que = canardTxInit(512, CANARD_MTU_CAN_CLASSIC);
	CanardCyphal canard_cyphal(&canard_adapter);
	canard_cyphal.setNodeID(cyphal_node_id);


	//	Logger::setCyphalCanardAdapter(&canard_cyphal);
	std::tuple<CanardCyphal> canard_adapters = { canard_cyphal };
	std::tuple<> empty_adapters = {} ;

	constexpr auto CAMERA_POWER = CIRCUITS::CIRCUIT_0;
	constexpr auto IMAGER_POWER = CIRCUITS::CIRCUIT_1;
	constexpr auto CAMERA_FLASH = CIRCUITS::CIRCUIT_2;
	constexpr auto IMAGER_MRAM = CIRCUITS::CIRCUIT_3;

	constexpr uint8_t GPIO_EXPANDER = 32;
	using PowerSwitchConfig = I2C_Register_Config<hi2c4, GPIO_EXPANDER>;
	using PowerSwitchTransport = I2CRegisterTransport<PowerSwitchConfig>;
	PowerSwitchTransport ps_transport;
	PowerSwitch<PowerSwitchTransport> power_switch(ps_transport, GPIOB, POWER_RST_Pin);
	power_switch.on(CAMERA_POWER);
	power_switch.on(IMAGER_POWER);
	power_switch.on(CAMERA_FLASH);
	power_switch.on(IMAGER_MRAM);

	constexpr uint8_t INA226 = 64;
	using PowerMonitorConfig = I2C_Register_Config<hi2c4, INA226>;
	using PowerMonitorTransport = I2CRegisterTransport<PowerMonitorConfig>;
	PowerMonitorTransport pm_transport;
	PowerMonitor<PowerMonitorTransport> power_monitor(pm_transport);

	using Rail1V8 = GpioPin<GPIOB_BASE, ENABLE_1V8_Pin>;
	using Rail2V8 = GpioPin<GPIOB_BASE, ENABLE_2V8_Pin>;
	CameraPowerConverters<Rail1V8, Rail2V8> camera_power_converters;
	camera_power_converters.enable();

	using RailA = GpioPin<GPIOD_BASE, ENABLE_A_Pin>;
	using RailB = GpioPin<GPIOD_BASE, ENABLE_B_Pin>;
	using RailC = GpioPin<GPIOD_BASE, ENABLE_C_Pin>;
	CameraPowerRails<RailA, RailB, RailC> camera_power_rails;
	camera_power_rails.disable(Rail::A);
	camera_power_rails.enable(Rail::B);
	camera_power_rails.enable(Rail::C);

	using CamClk   = GpioPin<GPIOC_BASE, CAMERA_HW_CLK_Pin>;
	using CamPwdn  = GpioPin<GPIOC_BASE, CAMERA_PWR_DN_Pin>;
	using CamReset = GpioPin<GPIOB_BASE, CAMERA_RST_Pin>;
	CameraControls<CamClk, CamReset, CamPwdn> camera_control;
	camera_control.clock_on();
	camera_control.powerdown_off();
	camera_control.reset_release();

	HAL_Delay(5);
	using I2CSwitchConfig = I2C_Stream_Config<hi2c1, TCA9546A_ADDRESS>;
	using I2CSwitchTransport = I2CStreamTransport<I2CSwitchConfig>;
	using I2CSwitchReset = GpioPin<GPIOB_BASE, I2C1_RST_Pin>;
	I2CSwitchTransport i2c_switch_transport;
	I2CSwitch<I2CSwitchTransport, I2CSwitchReset> camera_switch(i2c_switch_transport);
	camera_switch.releaseReset();

//	using SCL0 = GpioPin<GPIOB_BASE, GPIO_PIN_8>;
//	using SDA0 = GpioPin<GPIOB_BASE, GPIO_PIN_9>;
//
//	using SCCB1Bus = STM32_SCCB_Bus<SCL0, SDA0, 200>;
//	SCCB1Bus sccb1{};
//	using OV5640Config = SCCBRegisterConfig<SCCB1Bus, OV5640_ADDRESS, SCCBAddressWidth::Bits16>;
//	using Camera1Transport = SCCB_Register_Transport<OV5640Config, SCCB1Bus>;
//	Camera1Transport cam1_transport{sccb1};
//	OV5640<Camera1Transport> cam1(cam1_transport);
//
//	using SCCB2Bus = STM32_SCCB_Bus<SCL0, SDA0, 200>;
//	SCCB2Bus sccb2{};
//	using OV2640Config = SCCBRegisterConfig< SCCB2Bus, OV2640_ADDRESS, SCCBAddressWidth::Bits8 >;
//	using Camera2Transport = SCCB_Register_Transport<OV2640Config,  SCCB2Bus>;
//	Camera2Transport cam2_transport{sccb2};
//	OV2640<Camera2Transport> cam2(cam2_transport);

	using OV5640Config = I2C_Register_Config<hi2c1, OV5640_ADDRESS, I2CAddressWidth::Bits16>;
	using Camera1Transport = I2CRegisterTransport<OV5640Config>;
	Camera1Transport cam1_transport{};
	OV5640<Camera1Transport> cam1(cam1_transport);

	using OV2640Config = I2C_Register_Config<hi2c1, OV2640_ADDRESS, I2CAddressWidth::Bits8 >;
	using Camera2Transport = I2CRegisterTransport<OV2640Config>;
	Camera2Transport cam2_transport{};
	OV2640<Camera2Transport> cam2(cam2_transport);


	//	constexpr uint8_t THERMO_I2C_ADR = 0x33;
	//    using MLX90640Config = I2C_Register_Config<hi2c2, THERMO_I2C_ADR, I2CAddressWidth::Bits16>;
	//    using MLX90640Transport = I2CRegisterTransport<MLX90640Config>;
	//    MLX90640Transport mlx90640_transport;
	//	MLX90640<MLX90640Transport> mlx90640(mlx90640_transport);

	RegistrationManager registration_manager;
	SubscriptionManager subscription_manager;
	registration_manager.subscribe(uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_);
	registration_manager.subscribe(uavcan_node_port_List_1_0_FIXED_PORT_ID_);
	registration_manager.subscribe(uavcan_diagnostic_Record_1_1_FIXED_PORT_ID_);
	registration_manager.publish(uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_);
	registration_manager.publish(uavcan_node_port_List_1_0_FIXED_PORT_ID_);
	registration_manager.publish(uavcan_diagnostic_Record_1_1_FIXED_PORT_ID_);

	HAL_Delay(3000);
	static SafeAllocator<CyphalTransfer, LocalHeap> allocator;
	LoopManager loop_manager(allocator);

	constexpr uint8_t uuid[] = {0x1a, 0xb7, 0x9f, 0x23, 0x7c, 0x51, 0x4e, 0x0b, 0x8d, 0x69, 0x32, 0xfa, 0x15, 0x0c, 0x6e, 0x41};
	constexpr char node_name[50] = "SCIL496_CSAT";

	using TSHeart = TaskSendHeartBeat<CanardCyphal>;
	register_task_with_heap<TSHeart>(registration_manager, 2000, 100, 0, canard_adapters);

	using TPHeart = TaskProcessHeartBeat<CanardCyphal>;
	register_task_with_heap<TPHeart>(registration_manager, 2000, 100, canard_adapters);

	using TSendNodeList = TaskSendNodePortList<CanardCyphal>;
	register_task_with_heap<TSendNodeList>(registration_manager, &registration_manager, 10000, 100, 0, canard_adapters);

	using TSubscribeNodeList = TaskSubscribeNodePortList<CanardCyphal>;
	register_task_with_heap<TSubscribeNodeList>(registration_manager, &subscription_manager, 10000, 100, canard_adapters);

	using TRespondInfo = TaskRespondGetInfo<CanardCyphal>;
	register_task_with_heap<TRespondInfo>(registration_manager, uuid, node_name, 10000, 100, canard_adapters);

	using TRequestInfo = TaskRequestGetInfo<CanardCyphal>;
	register_task_with_heap<TRequestInfo>(registration_manager, 10000, 100, 13, 0, canard_adapters);

	using TBlink = TaskBlinkLED;
	register_task_with_heap<TBlink>(registration_manager, GPIOB, LED1_Pin, 1000, 100);

	using TCheckMem = TaskCheckMemory;
	auto o1heap = LocalHeap::getO1Heap();
	register_task_with_heap<TCheckMem>(registration_manager, o1heap, 1000, 100);

	//	using PowerSwitchType = PowerSwitch<PowerSwitchTransport>;
	//	using MLX90640Type = MLX90640<MLX90640Transport>;
	//	using TMLX = TaskMLX90640<PowerSwitchType, MLX90640Type, NullImageBuffer, PeriodicTrigger>;
	//	NullImageBuffer imgbuf;
	//	PeriodicTrigger trig(30000);
	//	register_task_with_heap<TMLX>(registration_manager, o1heap, power_switch, IMAGER_POWER, mlx90640, imgbuf, trig, MLXMode::Burst, 2, 250, 1);

	subscription_manager.subscribe<SubscriptionManager::MessageTag>(registration_manager.getSubscriptions(), canard_adapters);
	subscription_manager.subscribe<SubscriptionManager::ResponseTag>(registration_manager.getServers(), canard_adapters);
	subscription_manager.subscribe<SubscriptionManager::RequestTag>(registration_manager.getClients(), canard_adapters);

	ServiceManager service_manager(registration_manager.getHandlers());
	service_manager.initializeServices(HAL_GetTick());

	uint32_t counter = 0;
	while(1)
	{
		log(LOG_LEVEL_TRACE, "while loop: %d\r\n", HAL_GetTick());
		log(LOG_LEVEL_TRACE, "RegistrationManager: (%d %d) (%d %d) \r\n",
				registration_manager.getHandlers().capacity(), registration_manager.getHandlers().size(),
				registration_manager.getSubscriptions().capacity(), registration_manager.getSubscriptions().size());
		log(LOG_LEVEL_TRACE, "ServiceManager: (%d %d) \r\n",
				service_manager.getHandlers().capacity(), service_manager.getHandlers().size());
		loop_manager.CanProcessTxQueue(&canard_adapter, &hcan1);
		loop_manager.CanProcessRxQueue(&canard_cyphal, &service_manager, empty_adapters, can_rx_buffer);
		loop_manager.LoopProcessRxQueue(&loopard_cyphal, &service_manager, empty_adapters);
		service_manager.handleServices();

		uint8_t data = 0;
		camera_switch.status(data);

		uint8_t camera2_id_h, camera2_id_l;
		camera_switch.select(I2CSwitchChannel::Channel2);
		HAL_Delay(10);
//		sccb2.reconfigure_pins_to_sccb();
		cam2_transport.read_reg(0x0A, &camera2_id_h, 1);
		cam2_transport.read_reg(0x0B, &camera2_id_l, 1);
//		sccb2.reconfigure_pins_to_i2c();

		uint8_t camera1_id_h, camera1_id_l;
		camera_switch.select(I2CSwitchChannel::Channel1);
		HAL_Delay(10);
//		sccb1.reconfigure_pins_to_sccb();
		cam1_transport.read_reg(0x300A, &camera1_id_h, 1);
		cam1_transport.read_reg(0x300B, &camera1_id_l, 1);
//		sccb1.reconfigure_pins_to_i2c();

//		char buffer[256];
//		sprintf(buffer, "Channel: %02x OV2640: (%02x %02x) OV5640: (%02x %02x)\r\n", data, camera2_id_h, camera2_id_l, camera1_id_h, camera1_id_l);
//		CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));

		if ((counter % 300) == 0)
		{
			HAL_GPIO_TogglePin(GPIOA, LED5_Pin);
		}

		HAL_Delay(5);
		++counter;
	}
}
