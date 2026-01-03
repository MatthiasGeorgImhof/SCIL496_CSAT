// MLX90640.hpp

#pragma once
#include <cstdint>
#include <cstring>
#include "Transport.hpp"

template <RegisterModeTransport Transport>
class MLX90640
{
public:
    explicit MLX90640(const Transport &t) : transport(t) {}

    // ─────────────────────────────────────────────
    // Read EEPROM (832 words)
    // ─────────────────────────────────────────────
    bool readEEPROM(uint16_t *eeprom)
    {
        // EEPROM is 832 words starting at 0x2400
        return readBlock(0x2400, reinterpret_cast<uint8_t *>(eeprom), 832u * 2u);
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
        return false; // timeout
    }

    // ─────────────────────────────────────────────
    // Read a single subpage (blocking)
    // frameData must point to an array of 834 uint16_t
    // ─────────────────────────────────────────────
    bool readSubpage(uint16_t *frameData)
    {
        if (frameData == nullptr)
        {
            return false;
        }

        // Directly read the RAM block (834 words = 1668 bytes)
        if (!readBlock(0x0400,
                       reinterpret_cast<uint8_t *>(frameData),
                       834u * sizeof(uint16_t)))
        {
            return false;
        }

        // Clear NEW_DATA and other status bits
        if (!clearStatus())
        {
            return false;
        }

        return true;
    }

    // ─────────────────────────────────────────────
    // Merge two subpages into a "frame" buffer
    //
    // NOTE:
    // - We do NOT interleave pixels here.
    // - We simply store subpages back-to-back:
    //   fullFrame[0..833]   = first subpage
    //   fullFrame[834..1667]= second subpage
    //
    // Caller must ensure fullFrame has space for 2*834 uint16_t.
    // Ground side will interpret them using EEPROM + pixel pattern.
    // ─────────────────────────────────────────────
    void createFrame(const uint16_t *sub0,
                     const uint16_t *sub1,
                     uint16_t *fullFrame)
    {
        if (!sub0 || !sub1 || !fullFrame)
            return;

        // First subpage
        std::memcpy(fullFrame, sub0, 834u * sizeof(uint16_t));
        // Second subpage immediately after
        std::memcpy(fullFrame + 834u, sub1, 834u * sizeof(uint16_t));
    }

    // ─────────────────────────────────────────────
    // Read a full "frame" = two subpages back-to-back
    //
    // frameData must point to an array of 2*834 uint16_t.
    // Layout on return:
    //   frameData[0..833]      = first subpage (as read)
    //   frameData[834..1667]   = second subpage (as read)
    //
    // We enforce that the two subpages are different (0 and 1).
    // ─────────────────────────────────────────────
    bool readFrame(uint16_t *frameData)
    {
        if (frameData == nullptr)
            return false;

        uint16_t subA[834];
        uint16_t subB[834];

        // ---- First subpage ----
        if (!waitUntilReady())
            return false;
        if (!readSubpage(subA))
            return false;
        int spA = int(subA[833] & 0x0001u);

        // ---- Second subpage ----
        if (!waitUntilReady())
            return false;
        if (!readSubpage(subB))
            return false;
        int spB = int(subB[833] & 0x0001u);

        if (spA == spB)
            return false;

        createFrame(subA, subB, frameData);
        return true;
    }

    // ─────────────────────────────────────────────
    // Status register helpers
    // ─────────────────────────────────────────────
    bool readStatus(uint16_t &status)
    {
        return readReg16(0x8000u, status);
    }

    bool clearStatus()
    {
        // Writing 0 clears latched flags according to MLX90640 protocol
        uint16_t zero = 0;
        return writeReg16(0x8000u, zero);
    }

private:
    const Transport &transport;

    // ─────────────────────────────────────────────
    // Low-level register access
    // ─────────────────────────────────────────────
    bool readReg16(uint16_t reg, uint16_t &out)
    {
        uint8_t addr[2] = {
            static_cast<uint8_t>(reg >> 8),
            static_cast<uint8_t>(reg & 0xFF)};
        uint8_t buf[2] = {0, 0};

        if (!transport.write_then_read(addr, 2u, buf, 2u))
            return false;

        out = static_cast<uint16_t>((buf[0] << 8) | buf[1]);
        return true;
    }

    bool writeReg16(uint16_t reg, uint16_t value)
    {
        uint8_t buf[4] = {
            static_cast<uint8_t>(reg >> 8),
            static_cast<uint8_t>(reg & 0xFF),
            static_cast<uint8_t>(value >> 8),
            static_cast<uint8_t>(value & 0xFF)};
        return transport.write(buf, 4u);
    }

    bool readBlock(uint16_t startReg, uint8_t *dest, std::size_t bytes)
    {
        if (dest == nullptr || bytes == 0)
            return false;

        uint8_t addr[2] = {
            static_cast<uint8_t>(startReg >> 8),
            static_cast<uint8_t>(startReg & 0xFF)};
        return transport.write_then_read(addr, 2u, dest, static_cast<uint16_t>(bytes));
    }
};
