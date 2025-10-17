#ifndef _POWER_SWITCH_H_
#define _POWER_SWITCH_H_

#include "Transport.hpp"
#include <cstdint>
#include <array>

// MCP23008 Register Definitions
enum class MCP23008_REGISTERS : uint8_t
{
    MCP23008_IODIR = 0x00,
    MCP23008_IPOL = 0x01,
    MCP23008_GPINTEN  = 0x02,
    MCP23008_DEFVAL = 0x03,
    MCP23008_INTCON = 0x04,
    MCP23008_IOCON = 0x05,
    MCP23008_GPPU = 0x06,
    MCP23008_INTF = 0x07,
    MCP23008_INTCAP = 0x08,
    MCP23008_GPIO = 0x09,
    MCP23008_OLAT = 0x0A,
};

template <typename Transport>
    requires RegisterModeTransport<Transport>
class PowerSwitch
{
public:
    PowerSwitch() = delete;

    explicit PowerSwitch(const Transport &transport, GPIO_TypeDef *resetPort, uint16_t resetPin)
        : transport_(transport), register_value_(0),  reset_port_(resetPort), reset_pin_(resetPin)
    {
        releaseReset();
    }

    bool on(uint8_t slot)
    {
        if (invalidSlot(slot))
            return false;
        register_value_ |= static_cast<uint8_t>(1 << slot);
        return writeRegister(MCP23008_REGISTERS::MCP23008_OLAT, &register_value_, 1);
    }

    bool off(uint8_t slot)
    {
        if (invalidSlot(slot))
            return false;
        register_value_ &= static_cast<uint8_t>(~(1 << slot));
        return writeRegister(MCP23008_REGISTERS::MCP23008_OLAT, &register_value_, 1);
    }

    bool status(uint8_t slot) const
    {
        if (invalidSlot(slot))
            return false;
        return (register_value_ & (1 << slot)) != 0;
    }

    bool setState(uint8_t mask)
    {
        register_value_ = mask;
        return writeRegister(MCP23008_REGISTERS::MCP23008_OLAT, &register_value_, 1);
    }

    uint8_t getState()
    {
        register_value_ = readRegister(MCP23008_REGISTERS::MCP23008_OLAT);
        return register_value_;
    }

    void holdReset()
    {
        // Set *Reset pin low
        HAL_GPIO_WritePin(reset_port_, reset_pin_, GPIO_PIN_RESET);
    }

    void releaseReset()
    {
        // Set *Reset pin high
        HAL_GPIO_WritePin(reset_port_, reset_pin_, GPIO_PIN_SET);

        uint8_t reset[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        writeRegister(MCP23008_REGISTERS::MCP23008_IODIR, reset, sizeof(reset));
    }

private:
    bool writeRegister(MCP23008_REGISTERS reg, uint8_t *data, uint16_t size)
    {
        constexpr size_t MaxSize = 16;
        static_assert(MaxSize >= 1, "MaxSize must accommodate register + data");

        if (size > MaxSize - 1)
        {
            return false; // prevent overflow
        }

        std::array<uint8_t, MaxSize> tx{};
        tx[0] = static_cast<uint8_t>(reg);
        std::memcpy(&tx[1], data, size);

        return transport_.write(tx.data(), size + 1);
    }

    uint8_t readRegister(MCP23008_REGISTERS reg) const
    {
        uint8_t tx = static_cast<uint8_t>(reg);
        uint8_t rx;
        transport_.write_then_read(&tx, 1, &rx, 1);
        return rx;
    }

    static constexpr bool invalidSlot(uint8_t slot)
    {
        return slot >= 8;
    }

private:
    const Transport &transport_;
    uint8_t register_value_;
    GPIO_TypeDef *reset_port_;
    uint16_t reset_pin_;
};

#endif /* _POWER_SWITCH_H_ */
