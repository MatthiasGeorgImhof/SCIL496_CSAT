#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "OV5640.hpp"
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>

struct GpioStub
{
    void high() {}
    void low() {}
};

struct DummyConfig
{
    using mode_tag = register_mode_tag;
};

// Mock transport that captures write and write_then_read calls
class MockTransport
{
public:
    using config_type = DummyConfig;

    bool write(const uint8_t *data, size_t size)
    {
        last_write.assign(data, data + size);
        return true;
    }

    bool write_then_read(const uint8_t *tx, size_t tx_size, uint8_t *rx, size_t rx_size)
    {
        last_write.assign(tx, tx + tx_size);
        last_read.resize(rx_size);
        for (size_t i = 0; i < rx_size; ++i)
            last_read[i] = mock_response[i];
        std::memcpy(rx, last_read.data(), rx_size);
        return true;
    }

    void setMockResponse(std::initializer_list<uint8_t> bytes)
    {
        mock_response.assign(bytes.begin(), bytes.end());
    }

    std::vector<uint8_t> last_write;
    std::vector<uint8_t> last_read;
    std::vector<uint8_t> mock_response;
};

TEST_CASE("writeRegister: single byte")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;
    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    cam.writeRegister(OV5640_Register::CHIP_ID, 0xAB);
    CHECK(transport.last_write == std::vector<uint8_t>({0x30, 0x0a, 0xAB}));
}

TEST_CASE("writeRegister: multi-byte little-endian payload")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;
    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    uint16_t value = 0x1234;
    cam.writeRegister(OV5640_Register::CHIP_ID, reinterpret_cast<uint8_t *>(&value), 2);
    CHECK(transport.last_write == std::vector<uint8_t>({0x30, 0x0a, 0x12, 0x34}));
}

TEST_CASE("readRegister: single byte")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;
    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    transport.setMockResponse({0xAB});
    uint8_t result = cam.readRegister(OV5640_Register::CHIP_ID);
    CHECK(result == 0xAB);
    CHECK(transport.last_write == std::vector<uint8_t>({0x30, 0x0a}));
}

TEST_CASE("readRegister: multi-byte big-endian to little-endian")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;
    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    transport.setMockResponse({0x30, 0x0a});
    uint16_t result = 0;
    cam.readRegister(OV5640_Register::CHIP_ID, reinterpret_cast<uint8_t *>(&result), 2);
    CHECK(result == 0x300a);
}

TEST_CASE("writeRegister: reject odd-sized payload")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;
    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    uint8_t data[3] = {0x01, 0x02, 0x03};
    bool ok = cam.writeRegister(OV5640_Register::CHIP_ID, data, 3);
    CHECK_FALSE(ok);
}

TEST_CASE("readRegister: reject odd-sized buffer")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;
    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    transport.setMockResponse({0x30, 0x0a});
    uint8_t buffer[3];
    bool ok = cam.readRegister(OV5640_Register::CHIP_ID, buffer, 3);
    CHECK_FALSE(ok);
}
