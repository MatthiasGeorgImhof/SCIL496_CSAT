#ifndef _I2C_SWITCH_HPP_
#define _I2C_SWITCH_HPP_

#include "Transport.hpp"
#include <cstdint>

// TCA9546A channel definitions
enum class I2CSwitchChannel : uint8_t
{
    None     = 0x00, // No channel selected
    Channel0 = 0x01, // SD0/SC0
    Channel1 = 0x02, // SD1/SC1
    Channel2 = 0x04, // SD2/SC2
    Channel3 = 0x08  // SD3/SC3
};

template <typename Transport>
    requires RegisterWriteTransport<Transport>
class I2CSwitch
{
public:
    I2CSwitch() = delete;

    explicit I2CSwitch(const Transport& transport, GPIO_TypeDef* resetPort, uint16_t resetPin)
        : transport_(transport), reset_port_(resetPort), reset_pin_(resetPin)
    {
        releaseReset(); // Ensure switch is active on init
    }

    bool select(I2CSwitchChannel channel)
    {
        uint8_t command = static_cast<uint8_t>(channel);
        return transport_.write(&command, 1);
    }

    bool disableAll()
    {
        uint8_t command = 0x00;
        return transport_.write(&command, 1);
    }

    void holdReset()
    {
        HAL_GPIO_WritePin(reset_port_, reset_pin_, GPIO_PIN_RESET);
    }

    void releaseReset()
    {
        HAL_GPIO_WritePin(reset_port_, reset_pin_, GPIO_PIN_SET);
    }

    uint8_t status()
    {
    	uint8_t reg = 0;
    	transport_.read(&reg, 1);
    	return reg;
    }

private:
    const Transport& transport_;
    GPIO_TypeDef* reset_port_;
    uint16_t reset_pin_;
};

#endif // _I2C_SWITCH_HPP_
