#ifndef __SCCB__HPP
#define __SCCB__HPP

#include "stm32l4xx_hal.h"   // adjust to your MCU family

#define SCCB_SCL_PORT GPIOB
#define SCCB_SCL_PIN  GPIO_PIN_8

#define SCCB_SDA_PORT GPIOB
#define SCCB_SDA_PIN  GPIO_PIN_9

static inline void SCCB_SCL_H(void) { HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_SET); }
static inline void SCCB_SCL_L(void) { HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_RESET); }

static inline void SCCB_SDA_H(void) { HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_SET); }
static inline void SCCB_SDA_L(void) { HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_RESET); }

static inline uint8_t SCCB_SDA_Read(void)
{
    return HAL_GPIO_ReadPin(SCCB_SDA_PORT, SCCB_SDA_PIN);
}

static inline void SCCB_Delay(void)
{
    for (volatile int i = 0; i < 200; i++);   // tune as needed
}

static void SCCB_SDA_AsInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = SCCB_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SCCB_SDA_PORT, &GPIO_InitStruct);
}

static void SCCB_SDA_AsOutputOD(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = SCCB_SDA_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SCCB_SDA_PORT, &GPIO_InitStruct);
}

void SCCB_ReconfigurePinsToGPIO(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();   // adjust for your port

    // SCL (PB8)
    GPIO_InitStruct.Pin   = SCCB_SCL_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SCCB_SCL_PORT, &GPIO_InitStruct);

    // SDA (PB9)
    GPIO_InitStruct.Pin   = SCCB_SDA_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SCCB_SDA_PORT, &GPIO_InitStruct);

    // Idle state
    SCCB_SCL_H();
    SCCB_SDA_H();
}

void SCCB_ReconfigurePinsToI2C(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();   // adjust for your port

    // SCL (PB8)
    GPIO_InitStruct.Pin       = SCCB_SCL_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(SCCB_SCL_PORT, &GPIO_InitStruct);

    // SDA (PB9)
    GPIO_InitStruct.Pin       = SCCB_SDA_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(SCCB_SDA_PORT, &GPIO_InitStruct);
}

void SCCB_Start(void)
{
    SCCB_SDA_AsOutputOD();
    SCCB_SDA_H();
    SCCB_SCL_H();
    SCCB_Delay();
    SCCB_SDA_L();
    SCCB_Delay();
    SCCB_SCL_L();
}

void SCCB_Stop(void)
{
    SCCB_SDA_AsOutputOD();
    SCCB_SDA_L();
    SCCB_Delay();
    SCCB_SCL_H();
    SCCB_Delay();
    SCCB_SDA_H();
    SCCB_Delay();
}

void SCCB_WriteByte(uint8_t byte)
{
    SCCB_SDA_AsOutputOD();

    for (int i = 0; i < 8; i++)
    {
        if (byte & 0x80) SCCB_SDA_H();
        else             SCCB_SDA_L();

        SCCB_Delay();
        SCCB_SCL_H();
        SCCB_Delay();
        SCCB_SCL_L();
        byte <<= 1;
    }

    // 9th clock: ignore ACK, release line
    SCCB_SDA_H();
    SCCB_Delay();
    SCCB_SCL_H();
    SCCB_Delay();
    SCCB_SCL_L();
}

uint8_t SCCB_ReadByte(void)
{
    uint8_t byte = 0;

    SCCB_SDA_AsInput();   // release line

    for (int i = 0; i < 8; i++)
    {
        byte <<= 1;

        SCCB_SCL_H();
        SCCB_Delay();

        if (SCCB_SDA_Read())
            byte |= 1;

        SCCB_SCL_L();
        SCCB_Delay();
    }

    // 9th clock: NACK (master releases line)
    SCCB_SDA_AsOutputOD();
    SCCB_SDA_H();
    SCCB_Delay();
    SCCB_SCL_H();
    SCCB_Delay();
    SCCB_SCL_L();

    return byte;
}

void SCCB_WriteReg16(uint8_t dev, uint16_t reg, uint8_t value)
{
    SCCB_SDA_AsOutputOD();

    SCCB_Start();
    SCCB_WriteByte(dev << 1);          // write address
    SCCB_WriteByte(reg >> 8);          // high byte
    SCCB_WriteByte(reg & 0xFF);        // low byte
    SCCB_WriteByte(value);
    SCCB_Stop();
}

uint8_t SCCB_ReadReg16(uint8_t dev, uint16_t reg)
{
    uint8_t value;

    // Write register address
    SCCB_SDA_AsOutputOD();
    SCCB_Start();
    SCCB_WriteByte(dev << 1);          // write address
    SCCB_WriteByte(reg >> 8);
    SCCB_WriteByte(reg & 0xFF);
    SCCB_Stop();

    // Read register value
    SCCB_Start();
    SCCB_WriteByte((dev << 1) | 1);    // read address
    value = SCCB_ReadByte();
    SCCB_Stop();

    return value;
}

#endif // __SCCB__HPP
