#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "I2CSwitch.hpp"
#include "mock_hal.h"
#include "Transport.hpp"

TEST_CASE("I2CSwitch channel selection and disable")
{
    static I2C_HandleTypeDef hi2c;
    constexpr uint8_t address = 0x70; // TCA9546A default address

    GPIO_TypeDef mock_gpio_port = {};
    constexpr uint16_t mock_gpio_pin = GPIO_PIN_0;

    using SwitchConfig = I2C_Config<hi2c, address>;
    using SwitchTransport = I2CTransport<SwitchConfig>;
    SwitchTransport transport;
    I2CSwitch<SwitchTransport> switcher(transport, &mock_gpio_port, mock_gpio_pin);

    clear_i2c_mem_data();

    SUBCASE("Select Channel 0")
    {
        CHECK(switcher.select(I2CSwitchChannel::Channel0) == true);
        CHECK(get_i2c_buffer_count() == 1);
        CHECK(get_i2c_buffer()[0] == 0x01);
    }

    SUBCASE("Select Channel 1")
    {
        CHECK(switcher.select(I2CSwitchChannel::Channel1) == true);
        CHECK(get_i2c_buffer_count() == 1);
        CHECK(get_i2c_buffer()[0] == 0x02);
    }

    SUBCASE("Select Channel 2")
    {
        CHECK(switcher.select(I2CSwitchChannel::Channel2) == true);
        CHECK(get_i2c_buffer_count() == 1);
        CHECK(get_i2c_buffer()[0] == 0x04);
    }

    SUBCASE("Select Channel 3")
    {
        CHECK(switcher.select(I2CSwitchChannel::Channel3) == true);
        CHECK(get_i2c_buffer_count() == 1);
        CHECK(get_i2c_buffer()[0] == 0x08);
    }

    SUBCASE("Disable all channels")
    {
        CHECK(switcher.disableAll() == true);
        CHECK(get_i2c_buffer_count() == 1);
        CHECK(get_i2c_buffer()[0] == 0x00);
    }

    SUBCASE("Select None explicitly")
    {
        CHECK(switcher.select(I2CSwitchChannel::None) == true);
        CHECK(get_i2c_buffer_count() == 1);
        CHECK(get_i2c_buffer()[0] == 0x00);
    }

    SUBCASE("Reset pin is set high on releaseReset")
    {
        switcher.holdReset(); // simulate prior state
        switcher.releaseReset();
        CHECK(get_gpio_pin_state(&mock_gpio_port, mock_gpio_pin) == GPIO_PIN_SET);
    }

    SUBCASE("Reset pin is set low on holdReset")
    {
        switcher.releaseReset(); // simulate prior state
        switcher.holdReset();
        CHECK(get_gpio_pin_state(&mock_gpio_port, mock_gpio_pin) == GPIO_PIN_RESET);
    }
}