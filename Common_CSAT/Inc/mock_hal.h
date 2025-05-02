#ifdef __x86_64__
#ifndef MOCK_HAL_H
#define MOCK_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
//  INCLUDES
//------------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

//------------------------------------------------------------------------------
//  CONDITIONAL DEFINITION OF __IO
//------------------------------------------------------------------------------

#ifndef __IO
  #ifdef MOCK_HAL_VOLATILE
    #define __IO volatile
  #else
    #define __IO
  #endif
#endif

//------------------------------------------------------------------------------
//  DEFINES - Configuration and Constants
//------------------------------------------------------------------------------

//--- General HAL Defines ---
#define HAL_MAX_DELAY           0xFFFFFFFFU  // Maximum delay value for HAL_Delay
#define HAL_OK                  0x00         // HAL Status: Operation successful
#define HAL_ERROR               0x01         // HAL Status: Operation failed
#define HAL_BUSY                0x02         // HAL Status: Resource busy
#define HAL_TIMEOUT             0x03         // HAL Status: Operation timed out

//--- I2C Defines ---
#define I2C_MEMADD_SIZE_8BIT    (0x00000001U) // I2C Memory address size: 8-bit
#define I2C_MEMADD_SIZE_16BIT   (0x00000002U) // I2C Memory address size: 16-bit

//--- DCMI Defines ---
#define DCMI_MODE_CONTINUOUS      0x00000000U  // Example: Continuous capture mode
#define DCMI_SYNCHRO_HARDWARE     0x00000001U  // Example: Hardware synchronization

//--- Buffer Size Defines ---
#define CAN_TX_BUFFER_SIZE    50   // Size of the CAN transmit buffer
#define CAN_RX_BUFFER_SIZE    50   // Size of the CAN receive buffer
#define UART_TX_BUFFER_SIZE   256  // Size of the UART transmit buffer
#define UART_RX_BUFFER_SIZE   256  // Size of the UART receive buffer
#define I2C_MEM_BUFFER_SIZE   256  // Size of the I2C memory buffer
#define USB_TX_BUFFER_SIZE    256  // Size of the USB transmit buffer
#define SPI_TX_BUFFER_SIZE    256  // Size of the SPI transmit buffer
#define SPI_RX_BUFFER_SIZE    256  // Size of the SPI receive buffer
#define DCMI_IMAGE_BUFFER_SIZE  (640*480)  //Example VGA resolution. Adjust for your use case

//--- GPIO Defines ---
#define GPIO_PIN_0  ((uint16_t)0x0001)  // Pin 0 selected
#define GPIO_PIN_1  ((uint16_t)0x0002)  // Pin 1 selected
#define GPIO_PIN_2  ((uint16_t)0x0004)  // Pin 2 selected
#define GPIO_PIN_3  ((uint16_t)0x0008)  // Pin 3 selected
#define GPIO_PIN_4  ((uint16_t)0x0010)  // Pin 4 selected
#define GPIO_PIN_5  ((uint16_t)0x0020)  // Pin 5 selected
#define GPIO_PIN_6  ((uint16_t)0x0040)  // Pin 6 selected
#define GPIO_PIN_7  ((uint16_t)0x0080)  // Pin 7 selected
#define GPIO_PIN_8  ((uint16_t)0x0100)  // Pin 8 selected
#define GPIO_PIN_9  ((uint16_t)0x0200)  // Pin 9 selected
#define GPIO_PIN_10 ((uint16_t)0x0400)  // Pin 10 selected
#define GPIO_PIN_11 ((uint16_t)0x0800)  // Pin 11 selected
#define GPIO_PIN_12 ((uint16_t)0x1000)  // Pin 12 selected
#define GPIO_PIN_13 ((uint16_t)0x2000)  // Pin 13 selected
#define GPIO_PIN_14 ((uint16_t)0x4000)  // Pin 14 selected
#define GPIO_PIN_15 ((uint16_t)0x8000)  // Pin 15 selected
#define GPIO_PIN_All ((uint16_t)0xFFFF)  // All pins selected
#define GPIO_PIN_MASK ((uint32_t)0x0000FFFF) // PIN mask for assert test

//--- CAN Defines ---
#define CAN_FILTERMODE_IDMASK       (0x00000000U)  // Identifier mask mode
#define CAN_FILTERMODE_IDLIST       (0x00000001U)  // Identifier list mode
#define CAN_FILTERSCALE_16BIT       (0x00000000U)  // Two 16-bit filters
#define CAN_FILTERSCALE_32BIT       (0x00000001U)  // One 32-bit filter
#define CAN_FILTER_DISABLE          (0x00000000U)  // Disable filter
#define CAN_FILTER_ENABLE           (0x00000001U)  // Enable filter
#define CAN_FILTER_FIFO0            (0x00000000U)  // Filter FIFO 0 assignment
#define CAN_FILTER_FIFO1            (0x00000001U)  // Filter FIFO 1 assignment
#define CAN_ID_STD                  (0x00000000U)  // Standard Id
#define CAN_ID_EXT                  (0x00000004U)  // Extended Id
#define CAN_RTR_DATA                (0x00000000U)  // Data frame
#define CAN_RTR_REMOTE              (0x00000002U)  // Remote frame
#define CAN_RX_FIFO0                (0x00000000U)  // CAN receive FIFO 0
#define CAN_RX_FIFO1                (0x00000001U)  // CAN receive FIFO 1
#define CAN_TX_MAILBOX0             (0x00000001U)  // Tx Mailbox 0
#define CAN_TX_MAILBOX1             (0x00000002U)  // Tx Mailbox 1
#define CAN_TX_MAILBOX2             (0x00000004U)  // Tx Mailbox 2

//--- UARTEx Defines ---
#define HAL_UART_RXEVENT_HT   0  // UART Rx Event: Half Transfer
#define HAL_UART_RXEVENT_IDLE 1  // UART Rx Event: Idle Line

//------------------------------------------------------------------------------
//  ENUMS - Enumerated Types
//------------------------------------------------------------------------------

//--- HAL Status ---
typedef uint32_t HAL_StatusTypeDef;

//--- GPIO Pin State ---
typedef enum {
    GPIO_PIN_RESET = 0,  // GPIO Pin is reset (low)
    GPIO_PIN_SET = 1      // GPIO Pin is set (high)
} GPIO_PinState;

typedef enum
{
  HAL_DCMI_STATE_RESET             = 0x00U,  /*!< DCMI not yet initialized or disabled                     */
  HAL_DCMI_STATE_READY             = 0x01U,  /*!< DCMI initialized and ready for use                        */
  HAL_DCMI_STATE_BUSY              = 0x02U,  /*!< DCMI transfer is ongoing                                  */
  HAL_DCMI_STATE_TIMEOUT           = 0x03U,  /*!< DCMI timeout state                                        */
  HAL_DCMI_STATE_ERROR             = 0x04U,  /*!< DCMI error state                                          */
} HAL_DCMI_StateTypeDef;

typedef enum
{
  HAL_UNLOCKED = 0x00U,                       /*!< Object is not locked                                      */
  HAL_LOCKED   = 0x01U                        /*!< Object is locked                                          */
} HAL_LockTypeDef;

//------------------------------------------------------------------------------
//  STRUCTURE DEFINITIONS - Data Structures
//------------------------------------------------------------------------------

//--- GPIO Structures ---
typedef struct {
    uint32_t Pin;         // Specifies the GPIO pins to be configured
    uint32_t Mode;        // Specifies the operating mode for the selected pins
    uint32_t Pull;        // Specifies the Pull-up or Pull-down resistance for the selected pins
    uint32_t Speed;       // Specifies the speed for the selected pins
    uint32_t Alternate;   // Specifies the alternate function to be used for the selected pins
} GPIO_InitTypeDef;

typedef struct {
    void *Instance;      // GPIO port instance (GPIOA, GPIOB, etc.) - void* for mocking
    GPIO_InitTypeDef Init; // GPIO initialization structure
} GPIO_TypeDef;

//--- CAN Structures ---
typedef struct {
    uint32_t StdId;      // Standard Identifier
    uint32_t ExtId;      // Extended Identifier
    uint8_t IDE;         // Identifier Extension
    uint8_t RTR;         // Remote Transmission Request
    uint8_t DLC;         // Data Length Code
    uint8_t Data[8];     // Data bytes (up to 8 bytes)
} CAN_TxHeaderTypeDef;

typedef struct {
    uint32_t StdId;      // Standard Identifier
    uint32_t ExtId;      // Extended Identifier
    uint8_t IDE;         // Identifier Extension
    uint8_t RTR;         // Remote Transmission Request
    uint8_t DLC;         // Data Length Code
    uint8_t Data[8];     // Data bytes (up to 8 bytes)
    uint8_t FIFONumber;  // FIFO Number (0 or 1)
} CAN_RxHeaderTypeDef;

typedef struct {
  uint8_t  FilterBank;            // Filter bank number
  uint32_t FilterMode;            // Filter mode (Id Mask or Id List)
  uint32_t FilterScale;           // Filter scale (16-bit or 32-bit)
  uint32_t FilterIdHigh;          // Filter ID (upper 16 bits)
  uint32_t FilterIdLow;           // Filter ID (lower 16 bits)
  uint32_t FilterMaskIdHigh;      // Filter Mask ID (upper 16 bits)
  uint32_t FilterMaskIdLow;       // Filter Mask ID (lower 16 bits)
  uint32_t FilterFIFOAssignment;   // FIFO Assignment (0 or 1)
  uint32_t FilterActivation;       // Filter Activation (Enable or Disable)
  uint32_t SlaveStartFilterBank; //Can be used to determine start filter bank on slave
} CAN_FilterTypeDef;

typedef struct {
    CAN_TxHeaderTypeDef TxHeader; // CAN Tx Header
    uint32_t  pData[2];           // Pointer to data (assuming 64-bit architecture)
    uint32_t  Mailbox;           // Mailbox number
} CAN_TxMessage_t;

typedef struct {
    CAN_RxHeaderTypeDef RxHeader; // CAN Rx Header
    uint32_t  pData[2];           // Pointer to data (assuming 64-bit architecture)
} CAN_RxMessage_t;

typedef struct {
    int dummy;                   // Minimal definition for mocking
} CAN_HandleTypeDef;

//--- UART Structures ---
typedef struct {
    uint32_t BaudRate;       // Specifies the baud rate
    uint32_t WordLength;     // Specifies the word length
    uint32_t StopBits;       // Specifies the stop bits
    uint32_t Parity;         // Specifies the parity
    uint32_t Mode;           // Specifies the mode (Tx, Rx, or TxRx)
    uint32_t HwFlowCtl;      // Specifies the hardware flow control
    uint32_t OverSampling;    // Specifies the oversampling mode
    uint32_t OneBitSampling;  // Specifies one-bit sampling
    uint32_t ADVFEATURE;      // Specifies advanced features
} UART_InitTypeDef;

typedef struct {
   UART_InitTypeDef Init;   // UART initialization structure
} UART_HandleTypeDef;

typedef uint32_t HAL_UART_RxEventTypeTypeDef; // Type for UART Rx Event

//--- I2C Structures ---
typedef struct {
    uint32_t ClockSpeed;     // Specifies the clock speed
    uint32_t DutyCycle;      // Specifies the duty cycle
    uint32_t OwnAddress1;    // Specifies the own address 1
    uint32_t AddressingMode; // Specifies the addressing mode
    uint32_t DualAddressMode;// Specifies the dual addressing mode
    uint32_t OwnAddress2;    // Specifies the own address 2
    uint32_t GeneralCallMode;// Specifies the general call mode
    uint32_t NoStretchMode;  // Specifies the no stretch mode
    uint32_t Master;         // Master flag
    uint32_t Init;          // Init flag
} I2C_InitTypeDef;

typedef struct {
   I2C_InitTypeDef Instance; // I2C initialization structure
} I2C_HandleTypeDef;

//--- SPI Structures ---
typedef struct {
  uint32_t Mode;                      // Specifies the operation mode for the SPI.
  uint32_t Direction;                 // Specifies the data transfer direction for the SPI.
  uint32_t DataSize;                  // Specifies the data size for the SPI.
  uint32_t ClockPolarity;             // Specifies the serial clock steady state.
  uint32_t ClockPhase;                // Specifies the clock active edge for the SPI serial clock.
  uint32_t NSS;                       // Specifies the NSS signal management mode.
  uint32_t BaudRatePrescaler;         // Specifies the Baud Rate prescaler value
  uint32_t FirstBit;                  // Specifies whether data transfers start from MSB or LSB bit.
  uint32_t TIMode;                    // Specifies if the TI mode is enabled or disabled.
  uint32_t CRCCalculation;            // Specifies if the CRC calculation is enabled or disabled.
  uint32_t CRCPolynomial;            // Specifies the polynomial for the CRC calculation
  uint32_t DataAlign;                 // Specifies the data alignment MSB/LSB.
  uint32_t FifoThreshold;             // Specifies the FIFO threshold.
  uint32_t TxCRCInitializationPattern;// SPI transmit CRC initial value
  uint32_t RxCRCInitializationPattern;// SPI receive CRC initial value
  uint32_t MasterSSIdleness;          // Specifies the Idle Time between two data frame transmission using the NSS management by software.
  uint32_t MasterKeepIOState;        // Specifies if the SPI Master Keep IO State is enabled or disabled.
  uint32_t SuspendState;              // Specifies the SPI suspend mode.
} SPI_InitTypeDef;

typedef struct
{
  void *Instance;  // SPI registers base address.
  SPI_InitTypeDef Init;        // SPI communication parameters.
} SPI_HandleTypeDef;

//--- DCMI Structures ---
typedef struct {
    uint32_t SynchroMode;        // Example: Synchronization mode
    uint32_t VSyncPolarity;      // Example: VSync polarity
    uint32_t HSyncPolarity;      // Example: HSync polarity
    uint32_t DataEnablePolarity; // Example: Data Enable Polarity
    uint32_t PCKPolarity;         // Example: Pixel Clock Polarity
    uint32_t CaptureRate;         // Example: Capture Rate
    uint32_t ExtendedDataMode;    // Example: Extended Data Mode
} DCMI_InitTypeDef;

typedef struct {
    void *Instance;               // DCMI peripheral instance (e.g., DCMI)
    DCMI_InitTypeDef Init;          // DCMI initialization structure
    HAL_LockTypeDef Lock;           // If you need to mock locking
    __IO HAL_DCMI_StateTypeDef State; // HAL state of peripheral
    uint32_t ErrorCode;             // Error code
    uint8_t *pFrameBuffer;          // Pointer to frame buffer (added for testing)
    uint32_t FrameWidth;            // Frame width (added for testing)
    uint32_t FrameHeight;           // Frame height (added for testing)
} DCMI_HandleTypeDef;

//------------------------------------------------------------------------------
//  MOCK FUNCTION PROTOTYPES - HAL Functions
//------------------------------------------------------------------------------

//--- CAN Mock Function Prototypes ---
HAL_StatusTypeDef HAL_CAN_AddTxMessage(void *hcan, CAN_TxHeaderTypeDef *pHeader, uint8_t aData[], uint32_t *pTxMailbox);
HAL_StatusTypeDef HAL_CAN_GetRxMessage(void *hcan, uint32_t Fifo, CAN_RxHeaderTypeDef *pHeader, uint8_t aData[]);
HAL_StatusTypeDef HAL_CAN_GetTxMailboxesFreeLevel(void *hcan);
HAL_StatusTypeDef HAL_CAN_ConfigFilter(void *hcan, CAN_FilterTypeDef *sFilterConfig);
HAL_StatusTypeDef HAL_CAN_GetRxFifoFillLevel(void *hcan, uint32_t Fifo);

//--- UART Mock Function Prototypes ---
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);
HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);

//--- Time Mock Function Prototypes ---
void HAL_Delay(uint32_t Delay);
uint32_t HAL_GetTick(void);
void     HAL_SetTick(uint32_t tick);

//--- I2C Mock Function Prototypes ---
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);

//--- USB CDC Mock Function Prototypes ---
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

//--- GPIO Mock Function Prototypes ---
void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
void HAL_GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

//--- UARTEx Mock Function Prototypes ---
HAL_StatusTypeDef HAL_UARTEx_GetRxEventType(UART_HandleTypeDef *huart);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);

//--- SPI Mock Function Prototypes ---
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *hspi); // Add SPI init function

//--- DCMI Mock Function Prototypes ---
HAL_StatusTypeDef HAL_DCMI_Init(DCMI_HandleTypeDef *hdcmi);
HAL_StatusTypeDef HAL_DCMI_DeInit(DCMI_HandleTypeDef *hdcmi);
HAL_StatusTypeDef HAL_DCMI_Start(DCMI_HandleTypeDef *hdcmi, uint32_t Mode, uint32_t DMA_InitStruct);
HAL_StatusTypeDef HAL_DCMI_Stop(DCMI_HandleTypeDef *hdcmi);
HAL_DCMI_StateTypeDef HAL_DCMI_GetState(DCMI_HandleTypeDef *hdcmi);
uint32_t HAL_DCMI_GetError(DCMI_HandleTypeDef *hdcmi); //return ErrorCode

//------------------------------------------------------------------------------
//  MOCK FUNCTION PROTOTYPES - Helper functions
//------------------------------------------------------------------------------

//--- CAN Helper Function Prototypes ---
void inject_can_rx_message(CAN_RxHeaderTypeDef header, uint8_t data[]);
void clear_can_tx_buffer();
void clear_can_rx_buffer();
void move_can_tx_to_rx();

//--- UART Helper Function Prototypes ---
void inject_uart_rx_data(uint8_t *data, int size);
void clear_uart_rx_buffer();
void clear_uart_tx_buffer();

//--- I2C Helper Function Prototypes ---
void inject_i2c_mem_data(uint16_t DevAddress, uint16_t MemAddress,uint8_t *data, uint16_t size);
void clear_i2c_mem_data();

//--- USB Helper Function Prototypes ---
void clear_usb_tx_buffer();
int get_usb_tx_buffer_count();
uint8_t* get_usb_tx_buffer();

//--- SPI Helper Function Prototypes ---
void clear_spi_tx_buffer();
void clear_spi_rx_buffer();
void inject_spi_rx_data(uint8_t *data, int size);
void copy_spi_tx_to_rx();

//--- Getter Function Prototypes ---
int get_can_tx_buffer_count();
CAN_TxMessage_t get_can_tx_message(int pos);
int get_uart_tx_buffer_count();
uint8_t* get_uart_tx_buffer();
void set_current_free_mailboxes(int free_mailboxes);
void set_current_rx_fifo_fill_level(int rx_fifo_level);
void set_current_tick(uint32_t tick);
void init_uart_handle(UART_HandleTypeDef *huart);
int get_spi_tx_buffer_count();
uint8_t* get_spi_tx_buffer();
int get_spi_rx_buffer_count();
uint8_t* get_spi_rx_buffer();
void init_spi_handle(SPI_HandleTypeDef *hspi);

// I2C Getters
uint16_t get_i2c_mem_buffer_dev_address();
uint16_t get_i2c_mem_buffer_mem_address();
uint16_t get_i2c_mem_buffer_count();
int get_i2c_buffer_count();
uint8_t* get_i2c_buffer();

//--- GPIO Access Function Prototypes ---
GPIO_PinState get_gpio_pin_state(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void set_gpio_pin_state(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);

//--- DCMI Access/Helper Function Prototypes ---
void set_dcmi_frame_buffer(DCMI_HandleTypeDef *hdcmi, uint8_t *buffer, uint32_t width, uint32_t height);
uint8_t* get_dcmi_frame_buffer(DCMI_HandleTypeDef *hdcmi); // Added getter

//------------------------------------------------------------------------------
//  EXTERNAL VARIABLES
//------------------------------------------------------------------------------

//--- UARTEx External Variables ---
extern uint32_t mocked_uart_rx_event_type;
void set_mocked_uart_rx_event_type(uint32_t event_type);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* MOCK_HAL_H */
#endif /* __x86_64__ */