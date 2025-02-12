#ifdef __x86_64__
#include "mock_hal.h"
#include <string.h>
#include <stdlib.h>

// Mocked Data Buffers
#define CAN_TX_BUFFER_SIZE 20
CAN_TxMessage_t can_tx_buffer[CAN_TX_BUFFER_SIZE];
int can_tx_buffer_count = 0;

#define CAN_RX_BUFFER_SIZE 20
CAN_RxMessage_t can_rx_buffer[CAN_RX_BUFFER_SIZE];
int can_rx_buffer_count = 0;

#define UART_TX_BUFFER_SIZE 256
uint8_t uart_tx_buffer[UART_TX_BUFFER_SIZE];
int uart_tx_buffer_count = 0;

#define UART_RX_BUFFER_SIZE 256
uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
int uart_rx_buffer_count = 0;
int uart_rx_buffer_read_pos = 0;


#define I2C_MEM_BUFFER_SIZE 256
uint8_t i2c_mem_buffer[I2C_MEM_BUFFER_SIZE];
uint16_t i2c_mem_buffer_dev_address;
uint16_t i2c_mem_buffer_mem_address;
uint16_t i2c_mem_buffer_size = 0;

// Mock Variables
uint32_t current_tick = 0;
int current_free_mailboxes = 3;
int current_rx_fifo_fill_level = 0;


// ----- CAN Mock Functions -----

uint32_t HAL_CAN_AddTxMessage(void *hcan, CAN_TxHeaderTypeDef *pHeader, uint8_t aData[], uint32_t *pTxMailbox)
{
    if (can_tx_buffer_count < CAN_TX_BUFFER_SIZE) {
        can_tx_buffer[can_tx_buffer_count].TxHeader = *pHeader;
        
        // Copy data if the buffer pointer is not null
        if (aData != NULL) {
             memcpy(&can_tx_buffer[can_tx_buffer_count].pData[0], aData, pHeader->DLC);
        } else {
             memset(&can_tx_buffer[can_tx_buffer_count].pData[0], 0, pHeader->DLC);
        }
       
        can_tx_buffer[can_tx_buffer_count].Mailbox = 0; // Mock mailbox
        *pTxMailbox = 0;
        can_tx_buffer_count++;
        return 0; // HAL_OK
    }
    return 1; // HAL_ERROR, buffer full
}


uint32_t HAL_CAN_GetRxMessage(void *hcan, uint32_t Fifo, CAN_RxHeaderTypeDef *pHeader, uint8_t aData[]) {
    if(current_rx_fifo_fill_level == 0) {
        return 1; // HAL_ERROR
    }
    if (can_rx_buffer_count > 0) {
       *pHeader = can_rx_buffer[0].RxHeader;
       // Copy data to given location if aData pointer is not null
       if (aData != NULL) {
           memcpy(aData, &can_rx_buffer[0].pData[0], pHeader->DLC);
       }
        // Shift buffer
        for(int i=0; i<can_rx_buffer_count-1; i++) {
          can_rx_buffer[i] = can_rx_buffer[i+1];
        }
        can_rx_buffer_count--;
        current_rx_fifo_fill_level--;
        return 0; // HAL_OK
    }

    return 1; // HAL_ERROR, No data available
}


uint32_t HAL_CAN_GetTxMailboxesFreeLevel(void *hcan) {
    return current_free_mailboxes;
}

uint32_t HAL_CAN_ConfigFilter(void *hcan, CAN_FilterTypeDef *sFilterConfig) {
    // Mock implementation - for now, just return HAL_OK
    return 0; // HAL_OK
}

uint32_t HAL_CAN_GetRxFifoFillLevel(void *hcan, uint32_t Fifo) {
    return current_rx_fifo_fill_level;
}



// ---- UART Mock Functions -----

uint32_t HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    if (uart_tx_buffer_count + Size <= UART_TX_BUFFER_SIZE) {
        memcpy(uart_tx_buffer + uart_tx_buffer_count, pData, Size);
        uart_tx_buffer_count += Size;
        return 0; // HAL_OK
    }
    return 1; // HAL_ERROR, buffer overflow
}


uint32_t HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size)
{
    if (uart_tx_buffer_count + Size <= UART_TX_BUFFER_SIZE) {
        memcpy(uart_tx_buffer + uart_tx_buffer_count, pData, Size);
        uart_tx_buffer_count += Size;
        return 0; // HAL_OK
    }
    return 1; // HAL_ERROR, buffer overflow
}

uint32_t HAL_UART_Receive(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
  uint32_t start_tick = current_tick;
  uint16_t bytes_received = 0;

  while(bytes_received < Size) {
     if(uart_rx_buffer_read_pos >= uart_rx_buffer_count) {
       if( (current_tick-start_tick) >= Timeout) {
            break; //Timeout
        } else {
            HAL_Delay(1);
            continue;
        }
     }
     pData[bytes_received] = uart_rx_buffer[uart_rx_buffer_read_pos];
     uart_rx_buffer_read_pos++;
     bytes_received++;
    }

  return bytes_received == Size ? 0 : 1; //HAL_OK if all bytes are received
}

uint32_t HAL_UART_Receive_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size) {
    uint32_t start_tick = current_tick;
    uint16_t bytes_received = 0;

  while(bytes_received < Size) {
     if(uart_rx_buffer_read_pos >= uart_rx_buffer_count) {
          break; // No Timeout in DMA version, just return the available data
     }
      pData[bytes_received] = uart_rx_buffer[uart_rx_buffer_read_pos];
      uart_rx_buffer_read_pos++;
      bytes_received++;
  }


  return bytes_received == Size ? 0 : 1; //HAL_OK if all bytes are received
}


// ---- Time Mock Functions -----

void HAL_Delay(uint32_t Delay)
{
    current_tick += Delay;
}

uint32_t HAL_GetTick(void)
{
    return current_tick;
}

void HAL_SetTick(uint32_t tick)
{
    current_tick = tick;
}

// ---- I2C Mock Functions -----

uint32_t HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    // Basic mock, just return HAL_OK
    return 0; // HAL_OK
}

uint32_t HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
   if(i2c_mem_buffer_dev_address != DevAddress || i2c_mem_buffer_mem_address != MemAddress || i2c_mem_buffer_size == 0) {
       return 1; //HAL_ERROR, invalid address
   }
   if(Size > i2c_mem_buffer_size) {
     return 1; //HAL_ERROR, invalid size
   }
    memcpy(pData, i2c_mem_buffer, Size);
   return 0; // HAL_OK
}

uint32_t HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
  // Basic Mock, just returns HAL_OK
    return 0; //HAL_OK
}


// ---- USB CDC Mock Functions -----
#define USB_TX_BUFFER_SIZE 256
uint8_t usb_tx_buffer[USB_TX_BUFFER_SIZE];
int usb_tx_buffer_count = 0;

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len) {
  if (usb_tx_buffer_count + Len <= USB_TX_BUFFER_SIZE) {
        memcpy(usb_tx_buffer + usb_tx_buffer_count, Buf, Len);
        usb_tx_buffer_count += Len;
        return 0; // Success
    }
    return 1;  // Error (Buffer overflow or something similar)
}

// ----- Injector and Deleter Functions ------

// CAN Injectors
void inject_can_rx_message(CAN_RxHeaderTypeDef header, uint8_t data[]) {
    if (can_rx_buffer_count < CAN_RX_BUFFER_SIZE) {
        can_rx_buffer[can_rx_buffer_count].RxHeader = header;
        memcpy(&can_rx_buffer[can_rx_buffer_count].pData[0], data, header.DLC);
         can_rx_buffer_count++;
         current_rx_fifo_fill_level++;
    }
}

// CAN Deleters
void clear_can_tx_buffer() {
    memset(can_tx_buffer, 0, sizeof(can_tx_buffer));  // Set all elements to 0
    can_tx_buffer_count = 0;
}

// UART Injectors
void inject_uart_rx_data(uint8_t *data, int size) {
  if (uart_rx_buffer_count + size <= UART_RX_BUFFER_SIZE) {
        memcpy(uart_rx_buffer + uart_rx_buffer_count, data, size);
        uart_rx_buffer_count += size;
    }
}

void clear_uart_rx_buffer(){
  memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer)); //Set all elements to 0
  uart_rx_buffer_count = 0;
  uart_rx_buffer_read_pos = 0;
}

// UART Deleters
void clear_uart_tx_buffer() {
    memset(uart_tx_buffer, 0, sizeof(uart_tx_buffer)); //Set all elements to 0
    uart_tx_buffer_count = 0;
}

// I2C Injectors
void inject_i2c_mem_data(uint16_t DevAddress, uint16_t MemAddress,uint8_t *data, uint16_t size){
    i2c_mem_buffer_dev_address = DevAddress;
    i2c_mem_buffer_mem_address = MemAddress;
    memcpy(i2c_mem_buffer, data, size);
    i2c_mem_buffer_size = size;
}

// I2C Deleters
void clear_i2c_mem_data(){
    memset(i2c_mem_buffer, 0, sizeof(i2c_mem_buffer)); //Set all elements to 0
    i2c_mem_buffer_size = 0;
}


void clear_usb_tx_buffer(){
    memset(usb_tx_buffer, 0, sizeof(usb_tx_buffer)); //Set all elements to 0
    usb_tx_buffer_count = 0;
}

// Getters to access buffers

int get_can_tx_buffer_count(){
  return can_tx_buffer_count;
}

CAN_TxMessage_t get_can_tx_message(int pos){
  if(pos < can_tx_buffer_count){
    return can_tx_buffer[pos];
  }
  CAN_TxMessage_t empty;
  memset(&empty,0, sizeof(empty));
  return empty;
}

int get_uart_tx_buffer_count(){
  return uart_tx_buffer_count;
}

uint8_t* get_uart_tx_buffer() {
  return uart_tx_buffer;
}

int get_usb_tx_buffer_count() {
    return usb_tx_buffer_count;
}

uint8_t* get_usb_tx_buffer() {
    return usb_tx_buffer;
}

void set_current_free_mailboxes(int free_mailboxes) {
  current_free_mailboxes = free_mailboxes;
}

void set_current_rx_fifo_fill_level(int rx_fifo_level){
    current_rx_fifo_fill_level = rx_fifo_level;
}

//Setter to set current tick value
void set_current_tick(uint32_t tick){
    current_tick = tick;
}

void init_uart_handle(UART_HandleTypeDef *huart)
{
    huart->Init.BaudRate = 115200;
    huart->Init.WordLength = 8;
    huart->Init.StopBits = 1;
    huart->Init.Parity = 0;
    huart->Init.Mode = 3;
    huart->Init.HwFlowCtl = 0;
    huart->Init.OverSampling = 0;
    huart->Init.OneBitSampling = 0;
    huart->Init.ADVFEATURE = 0;
}

#endif /* __x86_64__ */