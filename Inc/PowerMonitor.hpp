#ifndef _POWER_MONITOR_H_
#define _POWER_MONITOR_H_

#include <cstdint>
#include <limits>
#include <cmath>

#include "mock_hal.h"

enum class INA226_REGISTERS : uint8_t
{
    INA226_CONFIGURATION = 0x00,
    INA226_SHUNT_VOLTAGE = 0x01,
    INA226_BUS_VOLTAGE = 0x02,
    INA226_POWER = 0x03,
    INA226_CURRENT = 0x04,
    INA226_CALIBRATION = 0x05,
    INA226_MASK_ENABLE = 0x06,
    INA226_ALERT_LIMIT = 0x07,
    INA226_MANUFACTURER = 0xFE,
    INA226_DIE_ID = 0xFF,
};

struct PowerMonitorData
{
    uint16_t voltage_shunt_uV;
    uint16_t voltage_bus_mV;
    uint16_t power_mW;
    uint16_t current_uA;
};

class PowerMonitor
{
public:
    PowerMonitor() = delete;
    PowerMonitor(I2C_HandleTypeDef *hi2c, uint8_t i2c_address) : hi2c_(hi2c), i2c_address_(i2c_address) { setRegister(static_cast<uint8_t>(INA226_REGISTERS::INA226_CALIBRATION), calibration_value_); };

    PowerMonitorData operator()() const
    {
        uint16_t values[4];
        HAL_I2C_Mem_Read(hi2c_, i2c_address_<<1, static_cast<uint8_t>(INA226_REGISTERS::INA226_SHUNT_VOLTAGE), I2C_MEMADD_SIZE_8BIT, reinterpret_cast<uint8_t*>(values), sizeof(values), HAL_MAX_DELAY);
        PowerMonitorData data =
        {
            .voltage_shunt_uV = checkAndCast(static_cast<uint32_t>(std::abs(static_cast<int16_t>(byteswap(values[0])))) * 5 / 2),
            .voltage_bus_mV = checkAndCast(static_cast<uint32_t>(byteswap(values[1])) * 5 / 4),
            .power_mW = checkAndCast(static_cast<uint32_t>(byteswap(values[2])) * lsb_power_W_),
            .current_uA = checkAndCast(static_cast<uint32_t>(byteswap(values[3])) * lsb_current_uA_)
        };
        return data;
    }

    void setConfig(uint16_t config)
    {
        setRegister(static_cast<uint8_t>(INA226_REGISTERS::INA226_CONFIGURATION), config);
    }

private:
    inline uint16_t byteswap(uint16_t value) const { return (value << 8) | (value >> 8); }

    uint16_t checkAndCast(uint32_t value) const {
        if (value > std::numeric_limits<uint16_t>::max())
        {
          return std::numeric_limits<uint16_t>::max();
        }
        return static_cast<uint16_t>(value);
      }

    void setRegister(uint8_t reg, uint16_t value) const
    {
        uint16_t swapped = byteswap(value);
        HAL_I2C_Mem_Write(hi2c_, i2c_address_<<1, reg, I2C_MEMADD_SIZE_8BIT, reinterpret_cast<uint8_t*>(&swapped), sizeof(value), HAL_MAX_DELAY);
    }

    uint16_t getRegister(uint8_t reg) const
    {
        uint16_t value;
        HAL_I2C_Mem_Read(hi2c_, i2c_address_<<1, reg, I2C_MEMADD_SIZE_8BIT, reinterpret_cast<uint8_t*>(&value), sizeof(value), HAL_MAX_DELAY);
        return byteswap(value);
    }

private:
    I2C_HandleTypeDef *hi2c_;
    uint8_t i2c_address_;

    // equation 1: calibration = 0.00512 * (Vshunt * LSBcurrent) in A and ohm

    static constexpr uint16_t lsb_current_uA_ = 25;
    static constexpr uint16_t lsb_power_W_ = 25 * lsb_current_uA_;
    static constexpr uint16_t shunt_resistor_mohms_ = 10;
    static constexpr uint16_t calibration_value_ = 5120000 / (shunt_resistor_mohms_ * lsb_current_uA_);
};

#endif /* _POWER_MONITOR_H_ */
