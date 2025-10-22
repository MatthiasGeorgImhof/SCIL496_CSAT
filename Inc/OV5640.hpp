#ifndef _OV5640_HPP_
#define _OV5640_HPP_

#include "Transport.hpp"
#include "OV5640_Registers.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

template <typename Transport>
    requires RegisterModeTransport<Transport>
class OV5640
{
public:
    explicit OV5640(Transport& transport)
        : transport_(transport)
    {}

    // Write a single byte (no endian conversion needed)
    bool writeRegister(OV5640_Register reg, uint8_t value)
    {
        std::array<uint8_t, 3> tx = {
            highByte(reg), lowByte(reg), value
        };
        return transport_.write(tx.data(), tx.size());
    }

    // Write multi-byte little-endian payload as big-endian
    bool writeRegister(OV5640_Register reg, const uint8_t* data, size_t size)
    {
        constexpr size_t MaxSize = 32;
        if (size > MaxSize - 2 || size % 2 != 0) return false;

        std::array<uint8_t, MaxSize> tx;
        tx[0] = highByte(reg);
        tx[1] = lowByte(reg);

        // Swap each 16-bit word from little to big endian
        for (size_t i = 0; i < size; i += 2)
        {
            tx[2 + i]     = data[i + 1]; // high byte
            tx[2 + i + 1] = data[i];     // low byte
        }

        return transport_.write(tx.data(), size + 2);
    }

    // Read a single byte (no endian conversion needed)
    uint8_t readRegister(OV5640_Register reg)
    {
        std::array<uint8_t, 2> tx = { highByte(reg), lowByte(reg) };
        uint8_t rx = 0;
        transport_.write_then_read(tx.data(), tx.size(), &rx, 1);
        return rx;
    }

    // Read multi-byte big-endian payload and convert to little-endian
    bool readRegister(OV5640_Register reg, uint8_t* buffer, size_t size)
    {
        std::array<uint8_t, 2> tx = { highByte(reg), lowByte(reg) };
        if (!transport_.write_then_read(tx.data(), tx.size(), buffer, size))
            return false;

        if (size % 2 != 0) return false;

        // Swap each 16-bit word from big to little endian
        for (size_t i = 0; i < size; i += 2)
        {
            std::swap(buffer[i], buffer[i + 1]);
        }

        return true;
    }

private:
    Transport& transport_;

    static constexpr uint8_t highByte(OV5640_Register reg)
    {
        return static_cast<uint8_t>(static_cast<uint16_t>(reg) >> 8);
    }

    static constexpr uint8_t lowByte(OV5640_Register reg)
    {
        return static_cast<uint8_t>(static_cast<uint16_t>(reg) & 0xFF);
    }
};

#endif // _OV5640_HPP_
