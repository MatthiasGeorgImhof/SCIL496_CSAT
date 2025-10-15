#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "Transport.hpp"
#include "mock_hal.h"

// ─────────────────────────────────────────────
// I2C Tests (Register Mode)
// ─────────────────────────────────────────────

I2C_HandleTypeDef mock_i2c;
using TestI2CConfig = I2C_Config<mock_i2c, 0x42>;
using TestI2CTransport = I2CTransport<TestI2CConfig>;

TEST_CASE("I2CTransport write() sends correct register and payload")
{
    clear_i2c_mem_data();

    TestI2CTransport transport;
    uint8_t tx[] = {0x05, 0xAA, 0xBB}; // e.g. register + 2-byte payload
    CHECK(transport.write(tx, sizeof(tx)) == true);

    CHECK(get_i2c_buffer_count() == sizeof(tx));
    CHECK(get_i2c_buffer()[0] == 0x05); // register
    CHECK(get_i2c_buffer()[1] == 0xAA);
    CHECK(get_i2c_buffer()[2] == 0xBB);
}

TEST_CASE("I2CTransport write_then_read() performs atomic transaction")
{
    clear_i2c_rx_data(); // optional, for test hygiene

    uint8_t tx[] = {0x10}; // e.g. register address
    uint8_t injected[] = {0xAA, 0xBB};
    inject_i2c_rx_data(0x42, injected, sizeof(injected)); // ← use rx buffer

    TestI2CTransport transport;
    uint8_t rx[2] = {};
    CHECK(transport.write_then_read(tx, sizeof(tx), rx, sizeof(rx)) == true);

    CHECK(rx[0] == 0xAA);
    CHECK(rx[1] == 0xBB);
}

// ─────────────────────────────────────────────
// SPI Tests (Register Mode)
// ─────────────────────────────────────────────

SPI_HandleTypeDef mock_spi;
GPIO_TypeDef GPIOA;
using TestSPIConfig = SPI_Config<mock_spi, GPIO_PIN_5, 128>;
using TestSPITransport = SPITransport<TestSPIConfig>;

TEST_CASE("SPITransport write() transmits payload with CS toggled")
{
    clear_spi_tx_buffer();

    TestSPIConfig config(&GPIOA);  // Pass mock GPIO port
    TestSPITransport transport(config);
    uint8_t tx[] = {0x7E, 0x01}; // e.g. command + argument
    CHECK(transport.write(tx, sizeof(tx)) == true);

    CHECK(get_spi_tx_buffer_count() == sizeof(tx));
    CHECK(get_spi_tx_buffer()[0] == 0x7E);
    CHECK(get_spi_tx_buffer()[1] == 0x01);
}

TEST_CASE("SPITransport write_then_read() performs atomic transaction with CS held low")
{
    clear_spi_tx_buffer();
    clear_spi_rx_buffer();

    uint8_t tx[] = {0x0F}; // e.g. command byte
    uint8_t injected[] = {0x55, 0x66};
    inject_spi_rx_data(injected, sizeof(injected));

    TestSPIConfig config(&GPIOA);  // Pass mock GPIO port
    TestSPITransport transport(config);
    uint8_t rx[2] = {};
    CHECK(transport.write_then_read(tx, sizeof(tx), rx, sizeof(rx)) == true);

    CHECK(get_spi_tx_buffer()[0] == 0x0F);  // command
    CHECK(get_spi_tx_buffer()[1] == 0x00);  // dummy
    CHECK(get_spi_tx_buffer()[2] == 0x00);  // dummy
    CHECK(get_spi_tx_buffer_count() == 1+2);

    CHECK(rx[0] == 0x55);
    CHECK(rx[1] == 0x66);
}

// ─────────────────────────────────────────────
// UART Tests (Stream Mode)
// ─────────────────────────────────────────────

UART_HandleTypeDef mock_uart;
using TestUARTConfig = UART_Config<mock_uart>;
using TestUARTTransport = UARTTransport<TestUARTConfig>;

TEST_CASE("UARTTransport send() transmits correct data")
{
    clear_uart_tx_buffer();

    TestUARTTransport transport;
    uint8_t msg[] = "Hello";
    CHECK(transport.send(msg, sizeof(msg)) == true);

    CHECK(get_uart_tx_buffer_count() == sizeof(msg));
    CHECK(get_uart_tx_buffer()[0] == 'H');
    CHECK(get_uart_tx_buffer()[4] == 'o');
}

TEST_CASE("UARTTransport receive() receives injected data")
{
    clear_uart_rx_buffer();

    uint8_t injected[] = {'A', 'B', 'C'};
    inject_uart_rx_data(injected, sizeof(injected));

    TestUARTTransport transport;
    uint8_t buf[3] = {};
    CHECK(transport.receive(buf, sizeof(buf)) == true);

    CHECK(buf[0] == 'A');
    CHECK(buf[2] == 'C');
}
