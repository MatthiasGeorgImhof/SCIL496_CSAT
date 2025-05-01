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
	uint16_t manufacturer_id;
	uint16_t die_id;
};

class PowerMonitor
{
public:
	PowerMonitor() = delete;
	PowerMonitor(I2C_HandleTypeDef *hi2c, uint8_t i2c_address) : hi2c_(hi2c), i2c_address_(i2c_address)
	{
		reset();
	};

	bool reset()
	{
		bool res1 = setRegister(INA226_REGISTERS::INA226_CONFIGURATION, reset_value);
		delay();
		bool res2 = setRegister(INA226_REGISTERS::INA226_CALIBRATION, calibration_value_);
		delay();
		return res1 & res2;
	}

	bool setConfig(uint16_t config)
	{
		bool result = setRegister(INA226_REGISTERS::INA226_CONFIGURATION, config);
		delay();
		return result;
	}

	bool operator()(PowerMonitorData &data) const
	{
		int16_t voltage_shunt_uV;
		uint16_t voltage_bus_mV, power_mW, current_uA, manufacturer_id, die_id;
		bool result = true;
		result &= getRegister(INA226_REGISTERS::INA226_SHUNT_VOLTAGE, reinterpret_cast<uint16_t*>(&voltage_shunt_uV));
		result &= getRegister(INA226_REGISTERS::INA226_BUS_VOLTAGE, &voltage_bus_mV);
		result &= getRegister(INA226_REGISTERS::INA226_POWER, &power_mW);
		result &= getRegister(INA226_REGISTERS::INA226_CURRENT, &current_uA);
		result &= getRegister(INA226_REGISTERS::INA226_MANUFACTURER, &manufacturer_id);
		result &= getRegister(INA226_REGISTERS::INA226_DIE_ID, &die_id);

		data.voltage_shunt_uV = 5 * abs(voltage_shunt_uV) / 2;
		data.voltage_bus_mV = 5 * voltage_bus_mV / 4;
		data.power_mW = power_mW * lsb_power_W_;
		data.current_uA = current_uA * lsb_current_uA_;
		data.manufacturer_id = manufacturer_id;
		data.die_id = die_id;

		return result;
	}


private:
	inline uint16_t byteswap(uint16_t value) const { return (value << 8) | (value >> 8); }
	inline int16_t byteswap(int16_t value) const { return (value << 8) | (value >> 8); }

	uint16_t checkAndCast(uint32_t value) const
	{
		if (value > std::numeric_limits<uint16_t>::max())
		{
			return std::numeric_limits<uint16_t>::max();
		}
		return static_cast<uint16_t>(value);
	}

	bool setRegister(INA226_REGISTERS reg, uint16_t value) const
	{
		uint16_t value_ = byteswap(value);
		return (HAL_I2C_Mem_Write(hi2c_, i2c_address_<<1, static_cast<uint8_t>(reg), I2C_MEMADD_SIZE_8BIT, reinterpret_cast<uint8_t*>(&value_), sizeof(value_), HAL_MAX_DELAY) == HAL_OK);
	}

	bool getRegister(INA226_REGISTERS reg, uint16_t *value) const
	{
		uint16_t value_;
		bool result = (HAL_I2C_Mem_Read(hi2c_, i2c_address_<<1, static_cast<uint8_t>(reg), I2C_MEMADD_SIZE_8BIT, reinterpret_cast<uint8_t*>(&value_), sizeof(value_), HAL_MAX_DELAY) == HAL_OK);
		*value = byteswap(value_);
		return result;
	}

	void delay() { HAL_Delay(1); };

private:
	I2C_HandleTypeDef *hi2c_;
	uint8_t i2c_address_;

	// equation 1: calibration = 0.00512 * (Vshunt * LSBcurrent) in A and ohm

	static constexpr uint16_t lsb_current_uA_ = 25;
	static constexpr uint16_t lsb_power_W_ = 25 * lsb_current_uA_;
	static constexpr uint16_t shunt_resistor_mohms_ = 10;
	static constexpr uint16_t reset_value = 0x8000;
	static constexpr uint16_t configuration_value = 0x4327;
	static constexpr uint16_t calibration_value_ = 5120000 / (shunt_resistor_mohms_ * lsb_current_uA_);
};

#endif /* _POWER_MONITOR_H_ */
