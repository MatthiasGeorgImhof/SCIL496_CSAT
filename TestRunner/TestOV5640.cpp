#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "OV5640.hpp"
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>

// Simple GPIO stub that records calls
struct GpioStub
{
    std::vector<std::string> calls;

    void high() { calls.push_back("high"); }
    void low()  { calls.push_back("low");  }
};

// Dummy config for RegisterModeTransport concept
struct DummyConfig
{
    using mode_tag = register_mode_tag;
};

// Mock register‑mode transport
class MockTransport
{
public:
    using config_type = DummyConfig;

    bool write_reg(uint16_t reg, const uint8_t* data, size_t size)
    {
        last_reg  = reg;
        last_write.assign(data, data + size);
        return write_ok;
    }

    bool read_reg(uint16_t reg, uint8_t* data, size_t size)
    {
        last_reg = reg;
        last_read.resize(size);

        // Fill from mock_response; if shorter, pad with zeros
        for (size_t i = 0; i < size; ++i) {
            last_read[i] = (i < mock_response.size()) ? mock_response[i] : uint8_t{0};
        }

        std::memcpy(data, last_read.data(), size);
        return read_ok;
    }

    void setMockResponse(std::initializer_list<uint8_t> bytes)
    {
        mock_response.assign(bytes.begin(), bytes.end());
    }

    uint16_t              last_reg{0};
    std::vector<uint8_t>  last_write;
    std::vector<uint8_t>  last_read;
    std::vector<uint8_t>  mock_response;
    bool                  write_ok{true};
    bool                  read_ok{true};
};

TEST_CASE("writeRegister: single byte")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;

    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    cam.writeRegister(OV5640_Register::CHIP_ID, 0xAB);

    CHECK(transport.last_reg ==
          static_cast<uint16_t>(OV5640_Register::CHIP_ID));
    CHECK(transport.last_write == std::vector<uint8_t>({0xAB}));
}

TEST_CASE("writeRegister: multi-byte little-endian payload")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;

    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    uint16_t value = 0x1234; // little-endian in memory: 34 12
    cam.writeRegister(OV5640_Register::CHIP_ID,
                      reinterpret_cast<uint8_t*>(&value),
                      2);

    // Expect big-endian on the wire: 12 34
    CHECK(transport.last_reg ==
          static_cast<uint16_t>(OV5640_Register::CHIP_ID));
    CHECK(transport.last_write == std::vector<uint8_t>({0x12, 0x34}));
}

TEST_CASE("readRegister: single byte")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;

    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    transport.setMockResponse({0xAB});
    uint8_t result = cam.readRegister(OV5640_Register::CHIP_ID);

    CHECK(result == 0xAB);
    CHECK(transport.last_reg ==
          static_cast<uint16_t>(OV5640_Register::CHIP_ID));
}

TEST_CASE("readRegister: multi-byte big-endian to little-endian")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;

    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    // On the bus: 30 0A (big-endian)
    transport.setMockResponse({0x30, 0x0A});

    uint16_t result = 0;
    bool ok = cam.readRegister(OV5640_Register::CHIP_ID,
                               reinterpret_cast<uint8_t*>(&result),
                               2);

    CHECK(ok);
    // After swapping, memory holds 0A 30 -> value 0x300A (little-endian)
    CHECK(result == 0x300A);
    CHECK(transport.last_reg ==
          static_cast<uint16_t>(OV5640_Register::CHIP_ID));
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

    transport.setMockResponse({0x30, 0x0A});
    uint8_t buffer[3];
    bool ok = cam.readRegister(OV5640_Register::CHIP_ID, buffer, 3);
    CHECK_FALSE(ok);
}

// ─────────────────────────────────────────────
// Additional Tests
// ─────────────────────────────────────────────

TEST_CASE("powerUp() performs correct GPIO sequencing")
{
    MockTransport transport;
    GpioStub clk, pwdn, rst;

    OV5640<MockTransport, GpioStub, GpioStub, GpioStub> cam(transport, clk, pwdn, rst);

    cam.powerUp();

    CHECK(rst.calls.size() == 2);
    CHECK(clk.calls.size() == 1);
    CHECK(pwdn.calls.size() == 1);

    CHECK(rst.calls[0] == "low");   // reset_.low()
    CHECK(clk.calls[0] == "high");  // clockOE_.high()
    CHECK(pwdn.calls[0] == "low");  // powerDn_.low()
    CHECK(rst.calls[1] == "high");  // reset_.high()
}

TEST_CASE("GpioOutput concept is satisfied by GpioStub")
{
    static_assert(GpioOutput<GpioStub>);
    CHECK(true); // just to keep doctest happy
}

TEST_CASE("MockTransport satisfies RegisterModeTransport")
{
    static_assert(RegisterModeTransport<MockTransport>);
    CHECK(true);
}
