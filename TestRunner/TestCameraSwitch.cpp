#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "CameraSwitch.hpp"
#include "mock_hal.h"
#include "Transport.hpp"

TEST_CASE("CameraSwitch channel selection and GPIO activation")
{
    static I2C_HandleTypeDef hi2c;
    GPIO_TypeDef mock_reset_port = {};
    GPIO_TypeDef mock_channel_port = {};
    constexpr uint8_t address = 0x70;
    constexpr uint16_t reset_pin = GPIO_PIN_15;

    std::array<uint16_t, 4> channel_pins = {
        GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3
    };

    using SwitchConfig = I2C_Config<hi2c, address>;
    using SwitchTransport = I2CTransport<SwitchConfig>;
    SwitchTransport transport;

    CameraSwitch<SwitchTransport> switcher(
        transport,
        &mock_reset_port, reset_pin,
        &mock_channel_port, channel_pins
    );

    clear_i2c_mem_data();
    reset_gpio_port_state(&mock_reset_port);
    reset_gpio_port_state(&mock_channel_port);

    SUBCASE("Select Channel 0")
    {
        CHECK(switcher.select(I2CSwitchChannel::Channel0) == true);
        CHECK(get_i2c_buffer()[0] == 0x01);
        CHECK(get_gpio_pin_state(&mock_channel_port, GPIO_PIN_0) == GPIO_PIN_SET);
        CHECK(get_gpio_pin_state(&mock_channel_port, GPIO_PIN_1) == GPIO_PIN_RESET);
    }

    SUBCASE("Select Channel 1")
    {
        CHECK(switcher.select(I2CSwitchChannel::Channel1) == true);
        CHECK(get_i2c_buffer()[0] == 0x02);
        CHECK(get_gpio_pin_state(&mock_channel_port, GPIO_PIN_1) == GPIO_PIN_SET);
        CHECK(get_gpio_pin_state(&mock_channel_port, GPIO_PIN_0) == GPIO_PIN_RESET);
    }

    SUBCASE("Select Channel 2")
    {
        CHECK(switcher.select(I2CSwitchChannel::Channel2) == true);
        CHECK(get_i2c_buffer()[0] == 0x04);
        CHECK(get_gpio_pin_state(&mock_channel_port, GPIO_PIN_2) == GPIO_PIN_SET);
    }

    SUBCASE("Select Channel 3")
    {
        CHECK(switcher.select(I2CSwitchChannel::Channel3) == true);
        CHECK(get_i2c_buffer()[0] == 0x08);
        CHECK(get_gpio_pin_state(&mock_channel_port, GPIO_PIN_3) == GPIO_PIN_SET);
    }

    SUBCASE("Disable all channels")
    {
        switcher.disableAll();
        CHECK(get_i2c_buffer()[0] == 0x00);
        for (auto pin : channel_pins)
        {
            CHECK(get_gpio_pin_state(&mock_channel_port, pin) == GPIO_PIN_RESET);
        }
    }

    SUBCASE("Reset pin behavior")
    {
        switcher.holdReset();
        CHECK(get_gpio_pin_state(&mock_reset_port, reset_pin) == GPIO_PIN_RESET);

        switcher.releaseReset();
        CHECK(get_gpio_pin_state(&mock_reset_port, reset_pin) == GPIO_PIN_SET);
    }
}
