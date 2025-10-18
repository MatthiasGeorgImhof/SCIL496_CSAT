#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "PowerSwitch.hpp"
#include "mock_hal.h"
#include "Transport.hpp"

#include <iostream> // For debugging output (remove in production tests)
#include <bitset>   // For visualizing bit patterns

GPIO_TypeDef mock_gpio_port = {};
constexpr uint16_t mock_gpio_pin = GPIO_PIN_0;

TEST_CASE("PowerSwitch On/Off/Status")
{
    static I2C_HandleTypeDef hi2c;
    constexpr uint8_t address = 0x40;
    constexpr uint8_t ps_register = 0x0a;

    using SwitchConfig = I2C_Config<hi2c, address>;
    using SwitchTransport = I2CTransport<SwitchConfig>;
    SwitchTransport transport;

    PowerSwitch<SwitchTransport> pm(transport, &mock_gpio_port, mock_gpio_pin);

    // Reset the I2C buffer before each test case
    clear_i2c_mem_data();

    SUBCASE("Turn on slot 0")
    {
        pm.on(CIRCUITS::CIRCUIT_0);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer()[1] == 0b00000001); // Bit 0 should be set
        CHECK(pm.status(CIRCUITS::CIRCUIT_0) == true);
    }

    SUBCASE("Turn off slot 0")
    {
        // First, turn it on
        pm.on(CIRCUITS::CIRCUIT_0);
        pm.off(CIRCUITS::CIRCUIT_0);

        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer()[1] == 0b00000000); // Bit 0 should be cleared
        CHECK(pm.status(CIRCUITS::CIRCUIT_0) == false);
    }

    SUBCASE("Turn on slot 1")
    {
        pm.on(CIRCUITS::CIRCUIT_1);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer()[1] == 0b00000010); // Bit 1 should be set
        CHECK(pm.status(CIRCUITS::CIRCUIT_1) == true);
    }

    SUBCASE("Turn on slot 2")
    {
        pm.on(CIRCUITS::CIRCUIT_2);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer()[1] == 0b00000100); // Bit 2 should be set
        CHECK(pm.status(CIRCUITS::CIRCUIT_2) == true);
    }

    SUBCASE("Turn on slot 3")
    {
        pm.on(CIRCUITS::CIRCUIT_3);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer()[1] == 0b00001000); // Bit 3 should be set
        CHECK(pm.status(CIRCUITS::CIRCUIT_3) == true);
    }

    SUBCASE("Turn off slot 4")
    {
        pm.on(CIRCUITS::CIRCUIT_4);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer()[1] == 0b00010000); // Bit 4 should be cleared
        CHECK(pm.status(CIRCUITS::CIRCUIT_4) == true);
    }

    SUBCASE("Turn on slot 5")
    {
        pm.on(CIRCUITS::CIRCUIT_5);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer()[1] == 0b00100000); // Bit 5 should be set
        CHECK(pm.status(CIRCUITS::CIRCUIT_5) == true);
    }

    SUBCASE("Turn on slot 6")
    {
        pm.on(CIRCUITS::CIRCUIT_6);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer()[1] == 0b01000000); // Bit 6 should be set
        CHECK(pm.status(CIRCUITS::CIRCUIT_6) == true);
    }

    SUBCASE("Turn on slot 7")
    {
        pm.on(CIRCUITS::CIRCUIT_7);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer()[1] == 0b10000000); // Bit 7 should be set
        CHECK(pm.status(CIRCUITS::CIRCUIT_7) == true);
    }
    SUBCASE("Turn on and off multiple slots")
    {
        pm.on(CIRCUITS::CIRCUIT_0);
        pm.on(CIRCUITS::CIRCUIT_2);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer()[1] == 0b00000101);
        CHECK(pm.status(CIRCUITS::CIRCUIT_0) == true);
        CHECK(pm.status(CIRCUITS::CIRCUIT_2) == true);

        pm.off(CIRCUITS::CIRCUIT_0);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[1] == 0b00000100);
        CHECK(pm.status(CIRCUITS::CIRCUIT_0) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_2) == true);

        pm.off(CIRCUITS::CIRCUIT_2);
        CHECK(get_i2c_buffer()[0] == ps_register);
        CHECK(get_i2c_buffer_count() == 2);
        CHECK(get_i2c_buffer()[1] == 0b00000000);
        CHECK(pm.status(CIRCUITS::CIRCUIT_0) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_2) == false);
    }

    SUBCASE("Initial status is off")
    {
        CHECK(pm.status(CIRCUITS::CIRCUIT_0) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_1) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_2) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_3) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_4) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_5) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_6) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_7) == false);
    }

    SUBCASE("Set state with bitmask")
    {
        pm.setState(0b10101010);
        CHECK(pm.status(CIRCUITS::CIRCUIT_0) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_1) == true);
        CHECK(pm.status(CIRCUITS::CIRCUIT_2) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_3) == true);
        CHECK(pm.status(CIRCUITS::CIRCUIT_4) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_5) == true);
        CHECK(pm.status(CIRCUITS::CIRCUIT_6) == false);
        CHECK(pm.status(CIRCUITS::CIRCUIT_7) == true);
    }

    SUBCASE("Get state reads from OLAT")
    {
        constexpr uint16_t dev_addr = 0x40;
        uint8_t response = 0b11001100;

        inject_i2c_rx_data(dev_addr, &response, 1); // Correct injection

        auto state = pm.getState();
        CHECK(state == 0b11001100);
    }

    SUBCASE("Reset pin is set high on releaseReset")
    {
        // Set pin low first to simulate prior state
        set_gpio_pin_state(&mock_gpio_port, mock_gpio_pin, GPIO_PIN_RESET);
        CHECK(get_gpio_pin_state(&mock_gpio_port, mock_gpio_pin) == GPIO_PIN_RESET);

        pm.releaseReset();
        CHECK(get_gpio_pin_state(&mock_gpio_port, mock_gpio_pin) == GPIO_PIN_SET);
    }

    SUBCASE("Reset pin is set low on holdReset")
    {
        // Set pin high first to simulate prior state
        set_gpio_pin_state(&mock_gpio_port, mock_gpio_pin, GPIO_PIN_SET);
        CHECK(get_gpio_pin_state(&mock_gpio_port, mock_gpio_pin) == GPIO_PIN_SET);

        pm.holdReset();
        CHECK(get_gpio_pin_state(&mock_gpio_port, mock_gpio_pin) == GPIO_PIN_RESET);
    }

}