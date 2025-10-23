// GPIOPin.hpp

#pragma once

#ifdef __linux__
#include "mock_hal.h"
#endif
#ifdef __arm__
#include "stm32f4xx_hal.h"
#endif

enum class PinState : uint8_t {
    LOW = 0,
    HIGH = 1
};

template<GPIO_TypeDef* Port, uint16_t Pin>
struct GpioPin {
    void set(PinState state) const {
        HAL_GPIO_WritePin(Port, Pin, state == PinState::HIGH ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    void high() const {
        HAL_GPIO_WritePin(Port, Pin, GPIO_PIN_SET);
    }

    void low() const {
        HAL_GPIO_WritePin(Port, Pin, GPIO_PIN_RESET);
    }

    bool read() const {
        return HAL_GPIO_ReadPin(Port, Pin) == GPIO_PIN_SET;
    }
};

// GPIOPin.hpp