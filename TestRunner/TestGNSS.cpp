#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN // Only define in one cpp file
#include "doctest.h"
#include "mock_hal.h"
#include "GNSS.hpp"
#include <iostream>
#include <vector>
#include <cstring>

TEST_CASE("GNSS GetNavPosECEF Test") {
    UART_HandleTypeDef huart;
    init_uart_handle(&huart);
    clear_uart_rx_buffer();
    GNSS gnss(&huart);

    SUBCASE("Valid Data") {
        uint8_t test_data[] = {0xB5, 0x62, 0x01, 0x01, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xe7};
        static_assert(sizeof(test_data) == 28);
        inject_uart_rx_data(test_data, sizeof(test_data));

        std::optional<PositionECEF> navPosECEF = gnss.GetNavPosECEF();

        REQUIRE(navPosECEF.has_value());
        CHECK(navPosECEF.value().ecefX == 0);
        CHECK(navPosECEF.value().ecefY == 0);
        CHECK(navPosECEF.value().ecefZ == 0);
        CHECK(navPosECEF.value().pAcc == 0);
    }

    SUBCASE("Invalid Checksum") {
        uint8_t test_data[] = {0xB5, 0x62, 0x01, 0x01, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff};
        inject_uart_rx_data(test_data, sizeof(test_data));
        std::optional<PositionECEF> navPosECEF = gnss.GetNavPosECEF();
        CHECK(!navPosECEF.has_value());
    }

    SUBCASE("Wrong ClassID") {
        uint8_t test_data[] = {0xB5, 0x62, 0x02, 0x01, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Wrong ClassID
        inject_uart_rx_data(test_data, sizeof(test_data));
        std::optional<PositionECEF> navPosECEF = gnss.GetNavPosECEF();
        CHECK(!navPosECEF.has_value());
    }

     SUBCASE("Truncated Data") {
        uint8_t test_data[] = {0xB5, 0x62, 0x01, 0x01, 0x04, 0x00, 0x00, 0x00}; // Truncated Data
        inject_uart_rx_data(test_data, sizeof(test_data));
        std::optional<PositionECEF> navPosECEF = gnss.GetNavPosECEF();
        CHECK(!navPosECEF.has_value());
    }
}

TEST_CASE("GNSS GetNavVelNED Test") {
    UART_HandleTypeDef huart;
    init_uart_handle(&huart);
    clear_uart_rx_buffer();
    GNSS gnss(&huart);

    SUBCASE("Valid Data") {
        uint8_t test_data[] = {0xB5, 0x62, 0x01, 0x12, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x3e};
        static_assert(sizeof(test_data) == 44);
        inject_uart_rx_data(test_data, sizeof(test_data));

        std::optional<VelocityNED> navVelNED = gnss.GetNavVelNED();

        REQUIRE(navVelNED.has_value());
        CHECK(navVelNED.value().velN == 0);
        CHECK(navVelNED.value().velE == 0);
        CHECK(navVelNED.value().velD == 0);
        CHECK(navVelNED.value().headMot == 0);
        CHECK(navVelNED.value().gSpeed == 0);
        CHECK(navVelNED.value().sAcc == 0);
        CHECK(navVelNED.value().headAcc == 0);
    }
}

TEST_CASE("GNSS GetNavVelECEF Test") {
    UART_HandleTypeDef huart;
    init_uart_handle(&huart);
    clear_uart_rx_buffer();
    GNSS gnss(&huart);

    SUBCASE("Valid Data") {
        uint8_t test_data[] = {0xB5, 0x62, 0x01, 0x11, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x57};
        static_assert(sizeof(test_data) == 28);
        inject_uart_rx_data(test_data, sizeof(test_data));

        std::optional<VelocityECEF> navVelECEF = gnss.GetNavVelECEF();

        REQUIRE(navVelECEF.has_value());
        CHECK(navVelECEF.value().ecefVX == 0);
        CHECK(navVelECEF.value().ecefVY == 0);
        CHECK(navVelECEF.value().ecefVZ == 0);
        CHECK(navVelECEF.value().sAcc == 0);
    }
}