#ifndef _OV5640_HPP_
#define _OV5640_HPP_

#include "Transport.hpp"
#include "OV5640_Registers.hpp"
#include "GpioPin.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <type_traits>

template <typename Gpio>
concept GpioOutput = requires(Gpio gpio) {
    { gpio.low() }  -> std::same_as<void>;
    { gpio.high() } -> std::same_as<void>;
};

template <typename Transport, typename ClockOE, typename PowerDn, typename Reset>
    requires RegisterModeTransport<Transport> &&
             GpioOutput<ClockOE> &&
             GpioOutput<PowerDn> &&
             GpioOutput<Reset>
class OV5640
{
public:
    OV5640(Transport& transport, ClockOE& clk, PowerDn& pwdn, Reset& rst)
        : transport_(transport), clockOE_(clk), powerDn_(pwdn), reset_(rst)
    {}

    void powerUp()
    {
        reset_.low();   // Hold reset
        HAL_Delay(5);   // Wait 5 ms
        clockOE_.high(); // Enable oscillator
        powerDn_.low(); // Exit power-down (active low)
        HAL_Delay(1);   // Wait 1 ms
        reset_.high();  // Release reset
        HAL_Delay(20);  // Wait 20 ms before SCCB access
    }

    // Write a single byte (no endian conversion needed)
    bool writeRegister(OV5640_Register reg, uint8_t value)
    {
        uint8_t payload[1] = { value };
        return transport_.write_reg(
            static_cast<uint16_t>(reg),
            payload,
            sizeof(payload));
    }

    // Write multi-byte little-endian payload as big-endian
    bool writeRegister(OV5640_Register reg, const uint8_t* data, size_t size)
    {
        constexpr size_t MaxSize = 32;
        if (size > MaxSize || size % 2 != 0)
            return false;

        std::array<uint8_t, MaxSize> tx{};

        // Swap each 16-bit word from little to big endian
        for (size_t i = 0; i < size; i += 2) {
            tx[i]     = data[i + 1]; // high byte
            tx[i + 1] = data[i];     // low byte
        }

        return transport_.write_reg(
            static_cast<uint16_t>(reg),
            tx.data(),
            size);
    }

    // Read a single byte (no endian conversion needed)
    uint8_t readRegister(OV5640_Register reg)
    {
        uint8_t rx{};
        transport_.read_reg(
            static_cast<uint16_t>(reg),
            &rx,
            1);
        return rx;
    }

    // Read multi-byte big-endian payload and convert to little-endian
    bool readRegister(OV5640_Register reg, uint8_t* buffer, size_t size)
    {
        if (size % 2 != 0)
            return false;

        if (!transport_.read_reg(
                static_cast<uint16_t>(reg),
                buffer,
                size)) {
            return false;
        }

        // Swap each 16-bit word from big to little endian
        for (size_t i = 0; i < size; i += 2) {
            std::swap(buffer[i], buffer[i + 1]);
        }

        return true;
    }

private:
    Transport& transport_;
    ClockOE&   clockOE_;
    PowerDn&   powerDn_;
    Reset&     reset_;
};

#endif // _OV5640_HPP_
