#ifndef MOCK_HAL_H
#define MOCK_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Error Codes
#define HAL_OK  0
#define HAL_ERROR 1
#define HAL_BUSY  2
#define HAL_TIMEOUT 3

// Buffer sizes
#define CAN_TX_BUFFER_SIZE 20
#define CAN_RX_BUFFER_SIZE 20
#define UART_TX_BUFFER_SIZE 256
#define UART_RX_BUFFER_SIZE 256
#define I2C_MEM_BUFFER_SIZE 256
#define USB_TX_BUFFER_SIZE 256

// GPIO Definitions (minimal)
typedef struct {
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
    uint32_t Alternate;
} GPIO_InitTypeDef;

typedef struct {
    void *Instance; // e.g., GPIOA, GPIOB, etc.  Use void* for mocking
    GPIO_InitTypeDef Init;
} GPIO_TypeDef;

// GPIO Pin State Definition
typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET = 1
} GPIO_PinState;

// Mock CAN Structures
typedef struct {
    uint32_t StdId;
    uint32_t ExtId;
    uint8_t IDE;
    uint8_t RTR;
    uint8_t DLC;
    uint8_t Data[8];
} CAN_TxHeaderTypeDef;

typedef struct {
    uint32_t StdId;
    uint32_t ExtId;
    uint8_t IDE;
    uint8_t RTR;
    uint8_t DLC;
    uint8_t Data[8];
    uint8_t FIFONumber;
} CAN_RxHeaderTypeDef;

typedef struct {
  uint8_t  FilterBank;
  uint32_t FilterMode;
  uint32_t FilterScale;
  uint32_t FilterIdHigh;
  uint32_t FilterIdLow;
  uint32_t FilterMaskIdHigh;
  uint32_t FilterMaskIdLow;
  uint32_t FilterFIFOAssignment;
  uint32_t FilterActivation;
  uint32_t SlaveStartFilterBank;
} CAN_FilterTypeDef;

typedef struct {
    CAN_TxHeaderTypeDef TxHeader;
    uint32_t  pData[2];
    uint32_t  Mailbox;
} CAN_TxMessage_t;

typedef struct {
    CAN_RxHeaderTypeDef RxHeader;
    uint32_t  pData[2];
} CAN_RxMessage_t;

// Mock UART Structures
typedef struct {
    uint32_t BaudRate;
    uint32_t WordLength;
    uint32_t StopBits;
    uint32_t Parity;
    uint32_t Mode;
    uint32_t HwFlowCtl;
    uint32_t OverSampling;
    uint32_t OneBitSampling;
    uint32_t ADVFEATURE;
} UART_InitTypeDef;

typedef struct {
   UART_InitTypeDef Init;
} UART_HandleTypeDef;

// Mock I2C Structures
typedef struct {
    uint32_t ClockSpeed;
    uint32_t DutyCycle;
    uint32_t OwnAddress1;
    uint32_t AddressingMode;
    uint32_t DualAddressMode;
    uint32_t OwnAddress2;
    uint32_t GeneralCallMode;
    uint32_t NoStretchMode;
    uint32_t Master;
    uint32_t Init;
} I2C_InitTypeDef;

typedef struct {
   I2C_InitTypeDef Instance;
} I2C_HandleTypeDef;

// --- Additions for UARTEx ---

typedef uint32_t HAL_UART_RxEventTypeTypeDef; // Or whatever type is appropriate

#define HAL_UART_RXEVENT_HT 0  // Choose a representative value.  Could be an enum.

// Control the return value of HAL_UARTEx_GetRxEventType
extern HAL_UART_RxEventTypeTypeDef mocked_uart_rx_event_type;
void set_mocked_uart_rx_event_type(HAL_UART_RxEventTypeTypeDef event_type);

// --- Additions for CAN ---

typedef struct {
    int dummy; // Minimal definition
} CAN_HandleTypeDef;

#define CAN_RX_FIFO0 0

// ----- CAN Mock Functions -----
uint32_t HAL_CAN_AddTxMessage(void *hcan, CAN_TxHeaderTypeDef *pHeader, uint8_t aData[], uint32_t *pTxMailbox);
uint32_t HAL_CAN_GetRxMessage(void *hcan, uint32_t Fifo, CAN_RxHeaderTypeDef *pHeader, uint8_t aData[]);
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(void *hcan);
uint32_t HAL_CAN_ConfigFilter(void *hcan, CAN_FilterTypeDef *sFilterConfig);
uint32_t HAL_CAN_GetRxFifoFillLevel(void *hcan, uint32_t Fifo);

// ---- UART Mock Functions -----
uint32_t HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout);
uint32_t HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);
uint32_t HAL_UART_Receive(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout);
uint32_t HAL_UART_Receive_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);

// ---- Time Mock Functions -----
void HAL_Delay(uint32_t Delay);
uint32_t HAL_GetTick(void);
void     HAL_SetTick(uint32_t tick);

// ---- I2C Mock Functions -----
uint32_t HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout);
uint32_t HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);
uint32_t HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);

// ---- USB CDC Mock Functions -----
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);
void clear_usb_tx_buffer();
int get_usb_tx_buffer_count();
uint8_t* get_usb_tx_buffer();

// ---- GPIO Mock Functions -----
void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
void HAL_GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

// ----- Injector and Deleter Functions ------
void inject_can_rx_message(CAN_RxHeaderTypeDef header, uint8_t data[]);
void clear_can_tx_buffer();

void inject_uart_rx_data(uint8_t *data, int size);
void clear_uart_rx_buffer();
void clear_uart_tx_buffer();
void inject_i2c_mem_data(uint16_t DevAddress, uint16_t MemAddress,uint8_t *data, uint16_t size);
void clear_i2c_mem_data();

// Getters to access buffers
int get_can_tx_buffer_count();
CAN_TxMessage_t get_can_tx_message(int pos);
int get_uart_tx_buffer_count();
uint8_t* get_uart_tx_buffer();
void set_current_free_mailboxes(int free_mailboxes);
void set_current_rx_fifo_fill_level(int rx_fifo_level);
void set_current_tick(uint32_t tick);
void init_uart_handle(UART_HandleTypeDef *huart);

// UARTEx
uint32_t HAL_UARTEx_GetRxEventType(UART_HandleTypeDef *huart);
uint32_t HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);

// Add a way to access the injected I2C parameters for assertions
extern uint16_t mocked_i2c_dev_address;
extern uint16_t mocked_i2c_mem_address;
extern uint16_t mocked_i2c_mem_size;

// GPIO Access Functions
GPIO_PinState get_gpio_pin_state(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void set_gpio_pin_state(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* MOCK_HAL_H */