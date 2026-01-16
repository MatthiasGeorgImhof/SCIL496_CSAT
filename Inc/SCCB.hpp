#ifndef __SCCB_HPP__
#define __SCCB_HPP__

#include <cstdint>
#include "GpioPin.hpp"
#include "stm32l4xx_hal.h"

// SCCB bit-bang engine with injected pins and full mode switching
template <typename SCLPin, typename SDAPin, uint32_t DelayCycles = 200>
class SCCB
{
public:
    SCCB(SCLPin scl_pin, SDAPin sda_pin) : scl_(scl_pin), sda_(sda_pin) {}

    void SDA_AsInput() const
    {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin  = SDAPin::pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(SDAPin::port(), &GPIO_InitStruct);
    }

    void SDA_AsOutputOD() const
    {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin   = SDAPin::pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(SDAPin::port(), &GPIO_InitStruct);
    }

    // Switch both pins to GPIO bit-bang mode
    void ReconfigurePinsToSCCB() const
    {
        GPIO_InitTypeDef GPIO_InitStruct = {0};

        __HAL_RCC_GPIOB_CLK_ENABLE(); // adjust if needed

        // SCL
        GPIO_InitStruct.Pin   = SCLPin::pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(SCLPin::port(), &GPIO_InitStruct);

        // SDA
        GPIO_InitStruct.Pin   = SDAPin::pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(SDAPin::port(), &GPIO_InitStruct);

        // Idle state
        scl_high();
        sda_high();
    }

    // Switch both pins back to I2C peripheral mode
    void ReconfigurePinsToI2C() const
    {
        GPIO_InitTypeDef GPIO_InitStruct = {0};

        __HAL_RCC_GPIOB_CLK_ENABLE(); // adjust if needed

        // SCL
        GPIO_InitStruct.Pin       = SCLPin::pin;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull      = GPIO_PULLUP;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(SCLPin::port(), &GPIO_InitStruct);

        // SDA
        GPIO_InitStruct.Pin       = SDAPin::pin;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull      = GPIO_PULLUP;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(SDAPin::port(), &GPIO_InitStruct);
    }

    // ─────────────────────────────────────────────
    // SCCB Protocol Primitives
    // ─────────────────────────────────────────────

    void start()
    {
        SDA_AsOutputOD();
        sda_high();
        scl_high();
        delay();
        sda_low();
        delay();
        scl_low();
    }

    void stop()
    {
        SDA_AsOutputOD();
        sda_low();
        delay();
        scl_high();
        delay();
        sda_high();
        delay();
    }

    void write_byte(uint8_t b)
    {
        SDA_AsOutputOD();

        for (int i = 0; i < 8; ++i)
        {
            (b & 0x80) ? sda_high() : sda_low();
            delay();
            scl_high(); delay();
            scl_low();  delay();
            b <<= 1;
        }

        // ACK ignored
        sda_high();
        delay();
        scl_high();
        delay();
        scl_low();
    }

    uint8_t read_byte()
    {
        uint8_t v = 0;

        SDA_AsInput(); // release SDA

        for (int i = 0; i < 8; ++i)
        {
            v <<= 1;
            scl_high(); delay();
            if (sda_read()) v |= 1;
            scl_low(); delay();
        }

        // NACK
        SDA_AsOutputOD();
        sda_high();
        delay();
        scl_high();
        delay();
        scl_low();

        return v;
    }

private:
    SCLPin scl_;
    SDAPin sda_;

    static inline void delay()
    {
        for (volatile uint32_t i = 0; i < DelayCycles; ++i);
    }

    inline void scl_high() const { scl_.high();}
    inline void scl_low()  const { scl_.low();}
    inline void sda_high() const { sda_.high();}
    inline void sda_low()  const { sda_.low();}
    inline bool sda_read() const { return sda_.read(); }
};

#endif // __SCCB_HPP__
