
#ifndef INC_BMI270_H_
#define INC_BMI270_H_

#include <cstdint>
#include <optional>
#include "au.hpp"

#ifdef __arm__
#include "utilities.h"
#elif __x86_64__
#include "mock_hal.h"
#endif

struct SPI_PortDef
{
	UART_HandleTypeDef *hspi;
	GPIO_TypeDef* GPIO;
	uint16_t pin;
};

struct ChipID
{
	uint8_t id; // ID of the BMI270 module
};

struct Accelerometer
{
    au::QuantityF<au::MetersPerSecondSquaredInBodyFrame> x, y, z;
};

struct Gyroscope
{
    au::QuantityF<au::DegreesPerSecondInBodyFrame> x, y, z;
};

struct Magnetometer
{
	au::QuantityF<au::TeslaInBodyFrame> x, y, z;
};

struct Temperature
{
	au::QuantityF<au::Celsius> temperature;
};

class BMI270
{
public:
	BMI270() = delete;
	BMI270(SPI_PortDef spi);

	std::optional<ChipID> GetUniqID();
	std::optional<Accelerometer> GetAccelerometer();
	std::optional<Gyroscope> ReadGyroscope();
	std::optional<Magnetometer> ReadMagnetometer();
	std::optional<Temperature> ReadTemperature();
private:
	SPI_PortDef spi_;
};

#endif /* INC_BMI270_H_ */
