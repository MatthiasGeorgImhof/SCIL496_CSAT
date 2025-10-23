#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "GpioPin.hpp"
#include "mock_hal.h"

// Mock GPIO port
GPIO_TypeDef mock_port;

TEST_CASE("GpioPin set HIGH and LOW updates pin state") {
    GpioPin<&mock_port, GPIO_PIN_1> pin;

    pin.set(PinState::HIGH);
    CHECK(get_gpio_pin_state(&mock_port, GPIO_PIN_1) == GPIO_PIN_SET);

    pin.set(PinState::LOW);
    CHECK(get_gpio_pin_state(&mock_port, GPIO_PIN_1) == GPIO_PIN_RESET);
}

TEST_CASE("GpioPin high() and low() update pin state") {
    GpioPin<&mock_port, GPIO_PIN_2> pin;

    pin.high();
    CHECK(get_gpio_pin_state(&mock_port, GPIO_PIN_2) == GPIO_PIN_SET);

    pin.low();
    CHECK(get_gpio_pin_state(&mock_port, GPIO_PIN_2) == GPIO_PIN_RESET);
}

TEST_CASE("GpioPin read() reflects mock HAL state") {
    set_gpio_pin_state(&mock_port, GPIO_PIN_3, GPIO_PIN_SET);
    GpioPin<&mock_port, GPIO_PIN_3> pin;
    CHECK(pin.read() == true);

    set_gpio_pin_state(&mock_port, GPIO_PIN_3, GPIO_PIN_RESET);
    CHECK(pin.read() == false);
}
