#pragma once
#include <cstdint>
#include <cstring>
#include "Transport.hpp"

// ─────────────────────────────────────────────
// MLX90640 Constants
// ─────────────────────────────────────────────
constexpr static uint8_t  MLX90640_ID = 0x33;
constexpr static std::size_t MLX90640_EEPROM_WORDS = 832;
constexpr static std::size_t MLX90640_EEPROM_SIZE  = MLX90640_EEPROM_WORDS * sizeof(uint16_t);
constexpr static std::size_t MLX90640_SUBPAGE_WORDS = 834;
constexpr static std::size_t MLX90640_SUBPAGE_SIZE  = MLX90640_SUBPAGE_WORDS * sizeof(uint16_t);
constexpr static std::size_t MLX90640_FRAME_WORDS   = 2 * MLX90640_SUBPAGE_WORDS;
constexpr static std::size_t MLX90640_FRAME_SIZE    = MLX90640_FRAME_WORDS * sizeof(uint16_t);

// ─────────────────────────────────────────────
// MLX90640 Register Map
// ─────────────────────────────────────────────
enum class MLX90640_REGISTERS : uint16_t
{
    STATUS        = 0x8000,   // Status register (NEW_DATA, flags)
    CONTROL1      = 0x800D,   // Control register 1 (mode, refresh rate, power)
    RAM_START     = 0x0400,   // Start of RAM subpage data
    EEPROM_START  = 0x2400    // Start of EEPROM
};

// ─────────────────────────────────────────────
// MLX90640 Driver
// ─────────────────────────────────────────────
template <RegisterModeTransport Transport>
class MLX90640
{
    // Enforce 16‑bit register addressing for MLX90640
    static_assert(
        Transport::config_type::address_width == I2CAddressWidth::Bits16,
        "MLX90640 requires I2CAddressWidth::Bits16 (16‑bit register addressing)."
    );

public:
    explicit MLX90640(const Transport& t) : transport(t) {}

    // ─────────────────────────────────────────────
    // Initialization: wake device, chess mode, refresh rate
    // ─────────────────────────────────────────────
    bool wakeUp()
    {
        uint16_t ctrl;

        // Read current control register
        if (!readReg16(static_cast<uint16_t>(MLX90640_REGISTERS::CONTROL1), ctrl))
            return false;

        // Build new control register value in one expression:
        //
        // Bit 0   = 1      → wake device
        // Bit 12  = 1      → chess mode
        // Bits 7:5 = 0b011 → refresh rate = 4 Hz
        //
        uint16_t newCtrl =
              (ctrl | 0x0001)      // wake device
            | 0x1000               // chess mode
            | (0x03 << 5);         // refresh rate = 4 Hz

        if (!writeReg16(static_cast<uint16_t>(MLX90640_REGISTERS::CONTROL1), newCtrl))
            return false;

        return clearStatus();
    }

    // ─────────────────────────────────────────────
    // Put device into sleep mode
    // ─────────────────────────────────────────────
    bool sleep()
    {
        uint16_t ctrl;

        if (!readReg16(static_cast<uint16_t>(MLX90640_REGISTERS::CONTROL1), ctrl))
            return false;

        // Bit 0 = 0 → sleep mode
        uint16_t newCtrl = static_cast<uint16_t>(ctrl & static_cast<uint16_t>(~uint16_t(0x0001)));

        return writeReg16(static_cast<uint16_t>(MLX90640_REGISTERS::CONTROL1), newCtrl);
    }

    // ─────────────────────────────────────────────
    // Optional soft reset
    // ─────────────────────────────────────────────
    bool reset()
    {
        if (!writeReg16(static_cast<uint16_t>(MLX90640_REGISTERS::STATUS),   0x0000)) return false;
        if (!writeReg16(static_cast<uint16_t>(MLX90640_REGISTERS::CONTROL1), 0x0000)) return false;
        return true;
    }

    // ─────────────────────────────────────────────
    // Read EEPROM (832 words)
    // ─────────────────────────────────────────────
    bool readEEPROM(uint16_t* eeprom)
    {
        return readBlock(
            static_cast<uint16_t>(MLX90640_REGISTERS::EEPROM_START),
            reinterpret_cast<uint8_t*>(eeprom),
            MLX90640_EEPROM_SIZE
        );
    }

    // ─────────────────────────────────────────────
    // Non-blocking readiness check (NEW_DATA bit)
    // ─────────────────────────────────────────────
    bool isReady()
    {
        uint16_t status;
        if (!readStatus(status))
            return false;

        return (status & 0x0008u) != 0u; // NEW_DATA bit
    }

    bool waitUntilReady(unsigned maxAttempts = 100)
    {
        for (unsigned i = 0; i < maxAttempts; ++i)
        {
            if (isReady())
                return true;
        }
        return false;
    }

    // ─────────────────────────────────────────────
    // Read a single MLX90640_SUBPAGE (834 words)
    // ─────────────────────────────────────────────
    bool readSubpage(uint16_t* MLX90640_FRAMEData)
    {
        if (!MLX90640_FRAMEData)
            return false;

        if (!readBlock(
                static_cast<uint16_t>(MLX90640_REGISTERS::RAM_START),
                reinterpret_cast<uint8_t*>(MLX90640_FRAMEData),
                MLX90640_SUBPAGE_SIZE))
        {
            return false;
        }

        return clearStatus();
    }

    // ─────────────────────────────────────────────
    // Merge two subpages into a full frame
    // ─────────────────────────────────────────────
    void createFrame(const uint16_t* sub0,
                     const uint16_t* sub1,
                     uint16_t* fullFrame)
    {
        if (!sub0 || !sub1 || !fullFrame)
            return;

        std::memcpy(fullFrame, sub0, MLX90640_SUBPAGE_SIZE);
        std::memcpy(fullFrame + MLX90640_SUBPAGE_WORDS, sub1, MLX90640_SUBPAGE_SIZE);
    }

    // ─────────────────────────────────────────────
    // Read a full frame (two subpages)
    // ─────────────────────────────────────────────
    bool readFrame(uint16_t* frame)
    {
        if (!frame)
            return false;

        uint16_t subA[MLX90640_SUBPAGE_WORDS];
        uint16_t subB[MLX90640_SUBPAGE_WORDS];

        if (!waitUntilReady()) return false;
        if (!readSubpage(subA)) return false;
        int spA = int(subA[833] & 0x0001u);

        if (!waitUntilReady()) return false;
        if (!readSubpage(subB)) return false;
        int spB = int(subB[833] & 0x0001u);

        if (spA == spB)
            return false;

        createFrame(subA, subB, frame);
        return true;
    }

    // ─────────────────────────────────────────────
    // Status helpers
    // ─────────────────────────────────────────────
    bool readStatus(uint16_t& status)
    {
        return readReg16(static_cast<uint16_t>(MLX90640_REGISTERS::STATUS), status);
    }

    bool clearStatus()
    {
        uint16_t zero = 0;
        return writeReg16(static_cast<uint16_t>(MLX90640_REGISTERS::STATUS), zero);
    }

private:
    const Transport& transport;

    // ─────────────────────────────────────────────
    // Low-level register access
    // ─────────────────────────────────────────────
    bool readReg16(uint16_t reg, uint16_t& out)
    {
        uint8_t buf[2] = {0, 0};
        if (!transport.read_reg(reg, buf, sizeof(buf)))
            return false;

        out = (uint16_t(buf[0]) << 8) | buf[1];
        return true;
    }

    bool writeReg16(uint16_t reg, uint16_t value)
    {
        uint8_t buf[2] = {
            uint8_t(value >> 8),
            uint8_t(value & 0xFF)
        };
        return transport.write_reg(reg, buf, sizeof(buf));
    }

    bool readBlock(uint16_t startReg, uint8_t* dest, std::size_t bytes)
    {
        if (!dest || bytes == 0)
            return false;

        return transport.read_reg(startReg, dest, uint16_t(bytes));
    }
};
