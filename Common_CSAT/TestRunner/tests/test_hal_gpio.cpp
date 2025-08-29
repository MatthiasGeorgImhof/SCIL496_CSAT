//#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "mock_hal.h"
#include <string.h>
#include <stdio.h>

//--- GPIO Tests ---
TEST_CASE("HAL_GPIO_Init Test")
{
    GPIO_TypeDef GPIOx;
    GPIO_InitTypeDef GPIO_Init;
    GPIO_Init.Pin = 1;
    GPIO_Init.Mode = 1;
    GPIO_Init.Pull = 1;
    GPIO_Init.Speed = 1;
    GPIO_Init.Alternate = 1;

    HAL_GPIO_Init(&GPIOx, &GPIO_Init);

    CHECK(GPIOx.Init.Pin == 1);
    CHECK(GPIOx.Init.Mode == 1);
    CHECK(GPIOx.Init.Pull == 1);
    CHECK(GPIOx.Init.Speed == 1);
    CHECK(GPIOx.Init.Alternate == 1);
}

TEST_CASE("HAL_GPIO_WritePin and HAL_GPIO_ReadPin Test")
{
    GPIO_TypeDef GPIOx;
    uint16_t GPIO_Pin = 1;

    // Write a pin high, and check if we can read it back as high.
    HAL_GPIO_WritePin(&GPIOx, GPIO_Pin, GPIO_PIN_SET);
    CHECK(HAL_GPIO_ReadPin(&GPIOx, GPIO_Pin) == GPIO_PIN_SET);

    // Write a pin low, and check if we can read it back as low.
    HAL_GPIO_WritePin(&GPIOx, GPIO_Pin, GPIO_PIN_RESET);
    CHECK(HAL_GPIO_ReadPin(&GPIOx, GPIO_Pin) == GPIO_PIN_RESET);

    GPIO_Pin = 1 << 5;
    HAL_GPIO_WritePin(&GPIOx, GPIO_Pin, GPIO_PIN_SET);
    CHECK(HAL_GPIO_ReadPin(&GPIOx, GPIO_Pin) == GPIO_PIN_SET);

    GPIO_Pin = 1 << 5;
    HAL_GPIO_WritePin(&GPIOx, GPIO_Pin, GPIO_PIN_RESET);
    CHECK(HAL_GPIO_ReadPin(&GPIOx, GPIO_Pin) == GPIO_PIN_RESET);
}

TEST_CASE("HAL_GPIO_TogglePin Test")
{
    GPIO_TypeDef GPIOx;
    uint16_t GPIO_Pin = 1;

    // Start with the pin low.
    HAL_GPIO_WritePin(&GPIOx, GPIO_Pin, GPIO_PIN_RESET);
    CHECK(HAL_GPIO_ReadPin(&GPIOx, GPIO_Pin) == GPIO_PIN_RESET);

    // Toggle it high.
    HAL_GPIO_TogglePin(&GPIOx, GPIO_Pin);
    CHECK(HAL_GPIO_ReadPin(&GPIOx, GPIO_Pin) == GPIO_PIN_SET);

    // Toggle it back low.
    HAL_GPIO_TogglePin(&GPIOx, GPIO_Pin);
    CHECK(HAL_GPIO_ReadPin(&GPIOx, GPIO_Pin) == GPIO_PIN_RESET);
}

TEST_CASE("get_gpio_pin_state and set_gpio_pin_state")
{
    GPIO_TypeDef GPIOx;
    uint16_t GPIO_Pin = 1 << 2; // Pin 2

    // Initially Reset
    CHECK(get_gpio_pin_state(&GPIOx, GPIO_Pin) == GPIO_PIN_RESET);

    // Set it to high using set_gpio_pin_state
    set_gpio_pin_state(&GPIOx, GPIO_Pin, GPIO_PIN_SET);
    CHECK(get_gpio_pin_state(&GPIOx, GPIO_Pin) == GPIO_PIN_SET);

    // Set it to low using set_gpio_pin_state
    set_gpio_pin_state(&GPIOx, GPIO_Pin, GPIO_PIN_RESET);
    CHECK(get_gpio_pin_state(&GPIOx, GPIO_Pin) == GPIO_PIN_RESET);
}

TEST_CASE("get_gpio_pin_state and set_gpio_pin_reset")
{
    GPIO_TypeDef GPIOx;
    uint16_t GPIO_Pin = 1 << 2; // Pin 2

    // Initially Reset
    CHECK(get_gpio_pin_state(&GPIOx, GPIO_Pin) == GPIO_PIN_RESET);

    // Set it to high using set_gpio_pin_state
    set_gpio_pin_state(&GPIOx, GPIO_Pin, GPIO_PIN_SET);
    CHECK(get_gpio_pin_state(&GPIOx, GPIO_Pin) == GPIO_PIN_SET);

    // Set it to low using set_gpio_pin_state
    reset_gpio_port_state(&GPIOx);
    for(uint16_t i=0; i < MAX_GPIO_PINS; i++) {
        CHECK(get_gpio_pin_state(&GPIOx, i) == GPIO_PIN_RESET);
    }
}

