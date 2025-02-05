#ifdef __x86_64__
#ifndef MOCK_HAL_H
#define MOCK_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

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

// ---- I2C Mock Functions -----
uint32_t HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout);
uint32_t HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);
uint32_t HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);

// ---- USB CDC Mock Functions -----
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);
void clear_usb_tx_buffer();
int get_usb_tx_buffer_count();
uint8_t* get_usb_tx_buffer();


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

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* MOCK_HAL_H */
#endif /* __x86_64__ */