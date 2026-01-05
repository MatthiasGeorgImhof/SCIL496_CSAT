// MLX90640.hpp

#pragma once
#include <cstdint>
#include <cstring>
#include "Transport.hpp"

constexpr static uint8_t MLX90640_ID = 0x33;
constexpr static std::size_t MLX90640_EEPROM_WORDS = 832;
constexpr static std::size_t MLX90640_EEPROM_SIZE = MLX90640_EEPROM_WORDS * sizeof(uint16_t);
constexpr static std::size_t MLX90640_SUBPAGE_WORDS = 834;
constexpr static std::size_t MLX90640_SUBPAGE_SIZE = MLX90640_SUBPAGE_WORDS * sizeof(uint16_t);
constexpr static std::size_t MLX90640_FRAME_WORDS = 2 * MLX90640_SUBPAGE_WORDS;
constexpr static std::size_t MLX90640_FRAME_SIZE = MLX90640_FRAME_WORDS * sizeof(uint16_t);

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
        return readBlock(0x2400, reinterpret_cast<uint8_t *>(eeprom), MLX90640_EEPROM_SIZE);
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
    // Read a single MLX90640_SUBPAGE (blocking)
    // MLX90640_FRAMEData must point to an array of 834 uint16_t
    // ─────────────────────────────────────────────
    bool readSubpage(uint16_t *MLX90640_FRAMEData)
    {
        if (MLX90640_FRAMEData == nullptr)
        {
            return false;
        }

        // Directly read the RAM block (834 words = 1668 bytes)
        if (!readBlock(0x0400,
                       reinterpret_cast<uint8_t *>(MLX90640_FRAMEData), MLX90640_SUBPAGE_SIZE))
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
    // Merge two MLX90640_SUBPAGEs into a "MLX90640_FRAME" buffer
    //
    // NOTE:
    // - We do NOT interleave pixels here.
    // - We simply store MLX90640_SUBPAGEs back-to-back:
    //   fullMLX90640_FRAME[0..833]   = first MLX90640_SUBPAGE
    //   fullMLX90640_FRAME[834..1667]= second MLX90640_SUBPAGE
    //
    // Caller must ensure fullMLX90640_FRAME has space for 2*834 uint16_t.
    // Ground side will interpret them using EEPROM + pixel pattern.
    // ─────────────────────────────────────────────
    void createFrame(const uint16_t *sub0,
                     const uint16_t *sub1,
                     uint16_t *fullMLX90640_FRAME)
    {
        if (!sub0 || !sub1 || !fullMLX90640_FRAME)
            return;

        // First MLX90640_SUBPAGE
        std::memcpy(fullMLX90640_FRAME, sub0, MLX90640_SUBPAGE_SIZE);
        // Second MLX90640_SUBPAGE immediately after
        std::memcpy(fullMLX90640_FRAME + MLX90640_SUBPAGE_WORDS, sub1, MLX90640_SUBPAGE_SIZE);
    }

    // ─────────────────────────────────────────────
    // Read a full "MLX90640_FRAME" = two MLX90640_SUBPAGEs back-to-back
    //
    // MLX90640_FRAMEData must point to an array of 2*834 uint16_t.
    // Layout on return:
    //   MLX90640_FRAMEData[0..833]      = first MLX90640_SUBPAGE (as read)
    //   MLX90640_FRAMEData[834..1667]   = second MLX90640_SUBPAGE (as read)
    //
    // We enforce that the two MLX90640_SUBPAGEs are different (0 and 1).
    // ─────────────────────────────────────────────
    bool readFrame(uint16_t *MLX90640_FRAMEData)
    {
        if (MLX90640_FRAMEData == nullptr)
            return false;

        uint16_t subA[834];
        uint16_t subB[834];

        // ---- First MLX90640_SUBPAGE ----
        if (!waitUntilReady())
            return false;
        if (!readSubpage(subA))
            return false;
        int spA = int(subA[833] & 0x0001u);

        // ---- Second MLX90640_SUBPAGE ----
        if (!waitUntilReady())
            return false;
        if (!readSubpage(subB))
            return false;
        int spB = int(subB[833] & 0x0001u);

        if (spA == spB)
            return false;

        createFrame(subA, subB, MLX90640_FRAMEData);
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
        uint8_t buf[2] = {0, 0};
        if (!transport.read_reg(reg, buf, sizeof(buf)))
            return false;

        out = static_cast<uint16_t>((buf[0] << 8) | buf[1]);
        return true;
    }

    bool writeReg16(uint16_t reg, uint16_t value)
    {
        uint8_t buf[2] = {
            static_cast<uint8_t>(value >> 8),
            static_cast<uint8_t>(value & 0xFF)};
        return transport.write_reg(reg, buf, sizeof(buf));
    }

    bool readBlock(uint16_t startReg, uint8_t *dest, std::size_t bytes)
    {
        if (dest == nullptr || bytes == 0)
            return false;

        return transport.read_reg(startReg, dest, static_cast<uint16_t>(bytes));
    }
};
