#ifndef _OV2640_HPP_
#define _OV2640_HPP_

#include "Transport.hpp"
#include "OV2640_Registers.hpp"
#include "CameraDriver.hpp"
#include <array>
#include <cstdint>
#include <concepts>

template <typename Transport>
    requires RegisterModeTransport<Transport>
class OV2640
{
public:
    explicit OV2640(Transport &transport)
        : transport_(transport)
    {
    }

    //
    // ────────────────────────────────────────────────────────────────
    //  High‑level CameraDriver API
    // ────────────────────────────────────────────────────────────────
    //

    bool init()
    {
        // OV2640 requires bank switching and a soft reset.
        if (!writeRegister(OV2640_Register::BANK_SEL, static_cast<uint8_t>(OV2640_Register::BANK_SENSOR)))
            return false;

        // Soft reset via COM7
        if (!writeRegister(OV2640_Register::REG_COM7, OV2640_COM7_SRST))
            return false;

        // TODO: full init table
        return true;
    }

    bool setResolution(uint16_t width, uint16_t height)
    {
        // OV2640 uses DSP ZMOW/ZMOH/ZMHH for output size
        if (!writeRegister(OV2640_Register::BANK_SEL, static_cast<uint8_t>(OV2640_Register::BANK_DSP)))
            return false;

        if (!writeRegister(OV2640_Register::DSP_ZMOW, static_cast<uint8_t>(width >> 2)))
            return false;
        if (!writeRegister(OV2640_Register::DSP_ZMOH, static_cast<uint8_t>(height >> 2)))
            return false;

        // High bits
        uint8_t high = ((width & 0x03) << 2) | ((height & 0x03));
        return writeRegister(OV2640_Register::DSP_ZMHH, high);
    }

    bool setFormat(PixelFormat fmt)
    {
        if (!writeRegister(OV2640_Register::BANK_SEL, static_cast<uint8_t>(OV2640_Register::BANK_DSP)))
            return false;

        switch (fmt)
        {
        case PixelFormat::YUV422:
            return writeRegister(OV2640_Register::DSP_FORMAT_CTRL, static_cast<uint8_t>(OV2640_FormatValue::YUV422));
        case PixelFormat::RGB565:
            return writeRegister(OV2640_Register::DSP_FORMAT_CTRL, static_cast<uint8_t>(OV2640_FormatValue::RGB565));
        case PixelFormat::JPEG:
            return writeRegister(OV2640_Register::DSP_FORMAT_CTRL, static_cast<uint8_t>(OV2640_FormatValue::JPEG));
        }
        return false;
    }

    bool setExposure(uint32_t exposure_us)
    {
        // OV2640 exposure is coarse and sensor‑bank specific.
        // This is a placeholder until you add the full table.
        if (!writeRegister(OV2640_Register::BANK_SEL, static_cast<uint8_t>(OV2640_Register::BANK_SENSOR)))
            return false;

        uint8_t exp = static_cast<uint8_t>(exposure_us >> OV2640_EXPOSURE_SHIFT);
        return writeRegister(OV2640_Register::REG_AEC, exp);
    }

    bool setGain(float gain)
    {
        // OV2640 gain is in COM9 (upper bits) and sensor gain registers.
        // Placeholder implementation.
        if (!writeRegister(OV2640_Register::BANK_SEL, OV2640_Register::BANK_SENSOR))
            return false;

        uint8_t g = static_cast<uint8_t>(gain * OV2640_GAIN_SCALE);
        return writeRegister(OV2640_Register::REG_COM9, g);
    }

    bool enableTestPattern(bool enable)
    {
        if (!writeRegister(OV2640_Register::BANK_SEL, static_cast<uint8_t>(OV2640_Register::BANK_DSP)))
            return false;

        return writeRegister(OV2640_Register::DSP_IMAGE_MODE, enable ? static_cast<uint8_t>(OV2640_TestPattern::ColorBar) : static_cast<uint8_t>(OV2640_TestPattern::Disabled));
    }

    //
    // ────────────────────────────────────────────────────────────────
    //  Raw register access
    // ────────────────────────────────────────────────────────────────
    //

    bool writeRegister(OV2640_Register reg, uint8_t value)
    {
        return transport_.write_reg(static_cast<uint8_t>(reg), &value, 1);
    }

    bool writeRegister(OV2640_Register reg, const uint8_t *data, size_t size)
    {
        // OV2640 uses 8‑bit registers, no endian swapping required.
        if (size % 2 != 0) return false;
        return transport_.write_reg(static_cast<uint8_t>(reg), data, size);
    }

    uint8_t readRegister(OV2640_Register reg)
    {
        uint8_t rx{};
        transport_.read_reg(static_cast<uint8_t>(reg), &rx, 1);
        return rx;
    }

    bool readRegister(OV2640_Register reg, uint8_t *buffer, size_t size)
    {
        if (size % 2 != 0) return false;
        return transport_.read_reg(static_cast<uint8_t>(reg), buffer, size);
    }

private:
    Transport &transport_;
    constexpr static float OV2640_GAIN_SCALE = 8.0f;
    constexpr static uint8_t OV2640_EXPOSURE_SHIFT = 8;
};

#endif // _OV2640_HPP_
