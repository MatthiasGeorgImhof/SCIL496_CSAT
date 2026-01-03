#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <cstring>

#include "mock_hal/mock_hal_i2c.h"
#include "Transport.hpp"
#include "MLX90640.hpp"

// ─────────────────────────────────────────────
// Global mock I2C handle
// ─────────────────────────────────────────────
I2C_HandleTypeDef mock_i2c_handle = {
    .Instance =
        {
            .ClockSpeed      = 100000,
            .DutyCycle       = 0,
            .OwnAddress1     = 0,
            .AddressingMode  = 0,
            .DualAddressMode = 0,
            .OwnAddress2     = 0,
            .GeneralCallMode = 0,
            .NoStretchMode   = 0,
            .Master          = 1,
            .Init            = 1,
        }};

// ─────────────────────────────────────────────
// Transport typedefs
// ─────────────────────────────────────────────
using MLX_I2C_Config = I2C_Config<mock_i2c_handle, 0x33>;
using MLX_I2C        = I2CTransport<MLX_I2C_Config>;

// ─────────────────────────────────────────────
// Instantiate transport + driver
// ─────────────────────────────────────────────
MLX_I2C transport;
MLX90640<MLX_I2C> mlx(transport);

// Little‑endian helper (matches reinterpret_cast<uint16_t*> on x86)
static uint16_t le16(uint8_t lsb, uint8_t msb)
{
    return static_cast<uint16_t>(
        static_cast<uint16_t>(lsb) |
        (static_cast<uint16_t>(msb) << 8));
}

// ─────────────────────────────────────────────
// TEST: readEEPROM()
// ─────────────────────────────────────────────
TEST_CASE("MLX90640 readEEPROM returns data consistent with injected bytes")
{
    clear_i2c_mem_data();
    clear_i2c_rx_data();

    // 832 words = 1664 bytes
    uint8_t fake_eeprom[1664];
    for (int i = 0; i < 1664; ++i)
        fake_eeprom[i] = static_cast<uint8_t>(i & 0xFF);

    inject_i2c_rx_data(MLX_I2C_Config::address, fake_eeprom, sizeof(fake_eeprom));

    uint16_t buffer[832] = {};
    bool ok = mlx.readEEPROM(buffer);

    CHECK(ok == true);

    CHECK(buffer[0] == le16(fake_eeprom[0],  fake_eeprom[1]));
    CHECK(buffer[1] == le16(fake_eeprom[2],  fake_eeprom[3]));
    CHECK(buffer[2] == le16(fake_eeprom[4],  fake_eeprom[5]));
    CHECK(buffer[10]== le16(fake_eeprom[20], fake_eeprom[21]));
}

// ─────────────────────────────────────────────
// TEST: isReady()
// ─────────────────────────────────────────────
TEST_CASE("MLX90640 isReady detects NEW_DATA bit")
{
    clear_i2c_rx_data();

    uint8_t ready_status[2] = {0x00, 0x08}; // NEW_DATA bit set
    inject_i2c_rx_data(MLX_I2C_Config::address, ready_status, 2);

    CHECK(mlx.isReady() == true);
}

// ─────────────────────────────────────────────
// TEST: readSubpage() – non‑blocking version
// ─────────────────────────────────────────────
TEST_CASE("MLX90640 readSubpage reads RAM block")
{
    clear_i2c_rx_data();
    clear_i2c_mem_data();

    // Fake subpage data: 834 words = 1668 bytes
    uint8_t fake_subpage[1668];
    for (int i = 0; i < 1668; ++i)
        fake_subpage[i] = static_cast<uint8_t>(i & 0xFF);

    // Inject only the RAM block (non‑blocking readSubpage does NOT read status)
    inject_i2c_rx_data(MLX_I2C_Config::address, fake_subpage, sizeof(fake_subpage));

    uint16_t frame[834] = {};
    bool ok = mlx.readSubpage(frame);

    CHECK(ok == true);

    // Validate a few words
    CHECK(frame[0]   == le16(fake_subpage[0],   fake_subpage[1]));
    CHECK(frame[1]   == le16(fake_subpage[2],   fake_subpage[3]));
    CHECK(frame[10]  == le16(fake_subpage[20],  fake_subpage[21]));
    CHECK(frame[833] == le16(fake_subpage[1666],fake_subpage[1667]));

    // Verify clearStatus() wrote 4 bytes (writeReg16)
    CHECK(get_i2c_buffer_count() == 4);
}

// ─────────────────────────────────────────────
// TEST: createFrame()
// ─────────────────────────────────────────────
TEST_CASE("MLX90640 createFrame concatenates subpages back‑to‑back")
{
    uint16_t sub0[834];
    uint16_t sub1[834];
    uint16_t full[1668];

    constexpr uint16_t BASE0 = 0x1000u;
    constexpr uint16_t BASE1 = 0x2000u;

    for (int i = 0; i < 834; ++i)
    {
        sub0[i] = static_cast<uint16_t>(BASE0 + static_cast<uint16_t>(i));
        sub1[i] = static_cast<uint16_t>(BASE1 + static_cast<uint16_t>(i));
    }

    mlx.createFrame(sub0, sub1, full);

    CHECK(full[0]   == sub0[0]);
    CHECK(full[10]  == sub0[10]);
    CHECK(full[833] == sub0[833]);

    CHECK(full[834]     == sub1[0]);
    CHECK(full[834 + 5] == sub1[5]);
    CHECK(full[1667]    == sub1[833]);
}

TEST_CASE("MLX90640 readFrame attempts subpage reads (mock‑compatible)")
{
    clear_i2c_rx_data();
    clear_i2c_mem_data();

    // Inject one subpage (mock cannot simulate two)
    uint8_t fake_subpage[1668];
    memset(fake_subpage, 0xAA, sizeof(fake_subpage));
    inject_i2c_rx_data(MLX_I2C_Config::address, fake_subpage, sizeof(fake_subpage));

    uint16_t frame[1668] = {};
    bool ok = mlx.readFrame(frame);

    // Expected: mock cannot support two RX operations → readFrame() fails
    CHECK(ok == false);

    // Inspect the last transmit buffer
    uint8_t* tx = get_i2c_buffer();
    int count   = get_i2c_buffer_count();

    // The last write should be clearStatus() → writeReg16(0x8000, 0)
    CHECK(count == 4);
    CHECK(tx[0] == 0x80);   // MSB of 0x8000
    CHECK(tx[1] == 0x00);   // LSB of 0x8000
    CHECK(tx[2] == 0x00);   // MSB of value
    CHECK(tx[3] == 0x00);   // LSB of value
}

// ─────────────────────────────────────────────
// TEST: waitUntilReady() – success case
// ─────────────────────────────────────────────
TEST_CASE("MLX90640 waitUntilReady returns true when NEW_DATA appears")
{
    clear_i2c_rx_data();

    // First few calls: NOT ready
    uint8_t not_ready[2] = {0x00, 0x00};
    inject_i2c_rx_data(MLX_I2C_Config::address, not_ready, 2);

    CHECK(mlx.waitUntilReady(1) == false);

    // Now inject a READY status
    clear_i2c_rx_data();
    uint8_t ready[2] = {0x00, 0x08};
    inject_i2c_rx_data(MLX_I2C_Config::address, ready, 2);

    CHECK(mlx.waitUntilReady(1) == true);
}

// ─────────────────────────────────────────────
// TEST: waitUntilReady() – timeout case
// ─────────────────────────────────────────────
TEST_CASE("MLX90640 waitUntilReady returns false when NEW_DATA never appears")
{
    clear_i2c_rx_data();

    // Always NOT ready
    uint8_t not_ready[2] = {0x00, 0x00};
    inject_i2c_rx_data(MLX_I2C_Config::address, not_ready, 2);

    // Only 1 attempt → must fail
    CHECK(mlx.waitUntilReady(1) == false);
}
