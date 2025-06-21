// TestTaskSetRTC.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "TaskSetRTC.hpp"
#include "mock_hal.h"
#include <memory>

// Function to calculate the UBX checksum (fletcher algorithm)
uint16_t calculate_checksum(const std::vector<uint8_t> &data)
{
    uint16_t ck_a = 0;
    uint16_t ck_b = 0;
    for (uint8_t byte : data)
    {
        ck_a += byte;
        ck_b += ck_a;
    }
    return (ck_b << 8) | ck_a;
}

struct NavTimeUTCPayload
{
    uint32_t iTOW;       // GPS Time of week in milliseconds
    uint32_t tAcc;       // Time accuracy estimate in nanoseconds (UTC)
    int32_t nano;        // Fraction of a second in nanoseconds (UTC)
    int16_t year;        // UTC year
    int8_t month;        // UTC month
    int8_t day;          // UTC day
    int8_t hour;         // UTC hour
    int8_t min;          // UTC minute
    int8_t sec;          // UTC second
    uint8_t valid;       // Validity flags (bitfield)
    uint8_t utcStandard; // UTC standard identifier
    uint8_t res1;        // Reserved
};

// Function to generate the UBX-NAV-TIMEUTC response message
std::vector<uint8_t> generate_ubx_nav_timeutc_response(
    uint32_t itow,
    uint32_t t_acc,
    int32_t nano,
    int16_t year,
    int8_t month,
    int8_t day,
    int8_t hour,
    int8_t min,
    int8_t sec,
    uint8_t valid,  // Validity flags
    uint8_t utc_std // UTC standard identifier
)
{
    std::vector<uint8_t> message;

    // Header
    message.push_back(0xB5); // Sync Char 1
    message.push_back(0x62); // Sync Char 2
    message.push_back(0x01); // Message Class: NAV
    message.push_back(0x21); // Message ID: TIMEUTC

    // Payload (Construct from input data)
    NavTimeUTCPayload payload;
    payload.iTOW = itow;
    payload.tAcc = t_acc;
    payload.nano = nano;
    payload.year = year;
    payload.month = month;
    payload.day = day;
    payload.hour = hour;
    payload.min = min;
    payload.sec = sec;
    payload.valid = valid;
    payload.utcStandard = utc_std;
    payload.res1 = 0; // Reserved byte

    // Calculate payload length
    uint16_t payload_len = sizeof(NavTimeUTCPayload);
    message.push_back(payload_len & 0xFF);        // Length (LSB)
    message.push_back((payload_len >> 8) & 0xFF); // Length (MSB)

    // Add payload bytes to message
    const uint8_t *payload_bytes = reinterpret_cast<const uint8_t *>(&payload);
    for (size_t i = 0; i < payload_len; ++i)
    {
        message.push_back(payload_bytes[i]);
    }

    // Calculate and add checksum
    std::vector<uint8_t> checksum_data(message.begin() + 2, message.end()); // Exclude sync bytes

    // Now cast the checksum data to uint8_t*
    const uint8_t *checksum_bytes = checksum_data.data();
    uint8_t cka = 0, ckb = 0;
    for (size_t i = 0; i < checksum_data.size(); ++i)
    {
        cka += checksum_bytes[i];
        ckb += cka;
    }

    message.push_back(cka); // Checksum A
    message.push_back(ckb); // Checksum B

    return message;
}

TEST_CASE("TaskSetRTC Time Synchronization with UART Injection - Positive Nano")
{
    // Mock HAL RTC
    RTC_HandleTypeDef hrtc;
    RTC_InitTypeDef init;
    init.SynchPrediv = 1023;
    hrtc.Init = init;

    UART_HandleTypeDef huart;
    init_uart_handle(&huart);

    // Create TaskSetRTC instance
    std::shared_ptr<TaskSetRTC> task = std::make_shared<TaskSetRTC>(&huart, &hrtc, 1000, 0);

    // Desired time
    uint16_t year = 2024;
    uint8_t month = 12;
    uint8_t day = 5;
    uint8_t hour = 10;
    uint8_t min = 30;
    uint8_t sec = 0;
    int32_t nano = 250000000; // Positive nano

    // Create UBX-NAV-TIMEUTC message
    std::vector<uint8_t> ubx_message = generate_ubx_nav_timeutc_response(0, 0, nano, year, month, day, hour, min, sec, 3, 0);

    // Inject the UBX message into the UART receive buffer
    inject_uart_rx_data(ubx_message.data(), ubx_message.size());

    // Set initial RTC values (optional) - lets say it is off by a few minutes
    RTC_TimeTypeDef initial_time;
    initial_time.Hours = 10;
    initial_time.Minutes = 20;
    initial_time.Seconds = 0;
    initial_time.SubSeconds = 1000;
    set_mocked_rtc_time(initial_time);

    RTC_DateTypeDef initial_date;
    initial_date.Year = 24;
    initial_date.Month = 12;
    initial_date.Date = 5;
    set_mocked_rtc_date(initial_date);

    // Call handleTaskImpl to execute the time synchronization
    task->handleTaskImpl();

    // Get the mocked RTC time and date after the synchronization
    RTC_TimeTypeDef synced_time = get_mocked_rtc_time();
    RTC_DateTypeDef synced_date = get_mocked_rtc_date();

    // Assert that the RTC time and date are updated correctly
    CHECK(synced_time.Hours == 10);
    CHECK(synced_time.Minutes == 30);
    CHECK(synced_time.Seconds == 0);
    CHECK(synced_date.Year == 24);
    CHECK(synced_date.Month == 12);
    CHECK(synced_date.Date == 5);
    CHECK(synced_time.SubSeconds == 767);

    clear_mocked_rtc();
    clear_uart_rx_buffer();
}

TEST_CASE("TaskSetRTC Time Synchronization with UART Injection - Positive Nano roundover")
{
    // Mock HAL RTC
    RTC_HandleTypeDef hrtc;
    RTC_InitTypeDef init;
    init.SynchPrediv = 1023;
    hrtc.Init = init;

    UART_HandleTypeDef huart;
    init_uart_handle(&huart);

    // Create TaskSetRTC instance
    std::shared_ptr<TaskSetRTC> task = std::make_shared<TaskSetRTC>(&huart, &hrtc, 1000, 0);

    // Desired time
    uint16_t year = 2024;
    uint8_t month = 12;
    uint8_t day = 5;
    uint8_t hour = 10;
    uint8_t min = 30;
    uint8_t sec = 1;
    int32_t nano = 500000000; // Positive nano

    // Create UBX-NAV-TIMEUTC message
    std::vector<uint8_t> ubx_message = generate_ubx_nav_timeutc_response(0, 0, nano, year, month, day, hour, min, sec, 3, 0);

    // Inject the UBX message into the UART receive buffer
    inject_uart_rx_data(ubx_message.data(), ubx_message.size());

    // Set initial RTC values (optional) - lets say it is off by a few minutes
    RTC_TimeTypeDef initial_time;
    initial_time.Hours = 10;
    initial_time.Minutes = 20;
    initial_time.Seconds = 0;
    initial_time.SubSeconds = 256;
    set_mocked_rtc_time(initial_time);

    RTC_DateTypeDef initial_date;
    initial_date.Year = 24;
    initial_date.Month = 12;
    initial_date.Date = 5;
    set_mocked_rtc_date(initial_date);

    // Call handleTaskImpl to execute the time synchronization
    task->handleTaskImpl();

    // Get the mocked RTC time and date after the synchronization
    RTC_TimeTypeDef synced_time = get_mocked_rtc_time();
    RTC_DateTypeDef synced_date = get_mocked_rtc_date();

    // Assert that the RTC time and date are updated correctly
    CHECK(synced_time.Hours == 10);
    CHECK(synced_time.Minutes == 30);
    CHECK(synced_time.Seconds == 1);
    CHECK(synced_date.Year == 24);
    CHECK(synced_date.Month == 12);
    CHECK(synced_date.Date == 5);
    CHECK(synced_time.SubSeconds == 511);

    clear_mocked_rtc();
    clear_uart_rx_buffer();
}

TEST_CASE("TaskSetRTC Time Synchronization with UART Injection - Negative Nano with rollover 750ms")
{
    // Mock HAL RTC
    RTC_HandleTypeDef hrtc;
    RTC_InitTypeDef init;
    init.SynchPrediv = 1023;
    hrtc.Init = init;

    UART_HandleTypeDef huart;
    init_uart_handle(&huart);

    // Create TaskSetRTC instance
    std::shared_ptr<TaskSetRTC> task = std::make_shared<TaskSetRTC>(&huart, &hrtc, 1000, 0);

    // Desired time
    uint16_t year = 2024;
    uint8_t month = 12;
    uint8_t day = 5;
    uint8_t hour = 10;
    uint8_t min = 0;
    uint8_t sec = 0;
    int32_t nano = -750000000; // Negative nano

    // Create UBX-NAV-TIMEUTC message
    std::vector<uint8_t> ubx_message = generate_ubx_nav_timeutc_response(0, 0, nano, year, month, day, hour, min, sec, 3, 0);

    // Inject the UBX message into the UART receive buffer
    inject_uart_rx_data(ubx_message.data(), ubx_message.size());

    // Set initial RTC values (optional) - lets say it is off by a few minutes
    RTC_TimeTypeDef initial_time;
    initial_time.Hours = 10;
    initial_time.Minutes = 14;
    initial_time.Seconds = 34;
    initial_time.SubSeconds = 100;

    set_mocked_rtc_time(initial_time);

    RTC_DateTypeDef initial_date;
    initial_date.Year = 24;
    initial_date.Month = 12;
    initial_date.Date = 5;
    set_mocked_rtc_date(initial_date);

    // Call handleTaskImpl to execute the time synchronization
    task->handleTaskImpl();

    // Get the mocked RTC time and date after the synchronization
    RTC_TimeTypeDef synced_time = get_mocked_rtc_time();
    RTC_DateTypeDef synced_date = get_mocked_rtc_date();

    // Assert that the RTC time and date are updated correctly
    CHECK(synced_time.Hours == 9);
    CHECK(synced_time.Minutes == 59);
    CHECK(synced_time.Seconds == 59);
    CHECK(synced_date.Year == 24);
    CHECK(synced_date.Month == 12);
    CHECK(synced_date.Date == 5);
    CHECK(synced_time.SubSeconds == 767);

    clear_mocked_rtc();
    clear_uart_rx_buffer();
}

TEST_CASE("TaskSetRTC Time Synchronization with UART Injection - Negative Nano with rollover 250ms")
{
    // Mock HAL RTC
    RTC_HandleTypeDef hrtc;
    RTC_InitTypeDef init;
    init.SynchPrediv = 1023;
    hrtc.Init = init;

    UART_HandleTypeDef huart;
    init_uart_handle(&huart);

    // Create TaskSetRTC instance
    std::shared_ptr<TaskSetRTC> task = std::make_shared<TaskSetRTC>(&huart, &hrtc, 1000, 0);

    // Desired time
    uint16_t year = 2024;
    uint8_t month = 12;
    uint8_t day = 5;
    uint8_t hour = 10;
    uint8_t min = 30;
    uint8_t sec = 0;
    int32_t nano = -250000000; // Negative nano

    // Create UBX-NAV-TIMEUTC message
    std::vector<uint8_t> ubx_message = generate_ubx_nav_timeutc_response(0, 0, nano, year, month, day, hour, min, sec, 3, 0);

    // Inject the UBX message into the UART receive buffer
    inject_uart_rx_data(ubx_message.data(), ubx_message.size());

    // Set initial RTC values (optional) - lets say it is off by a few minutes
    RTC_TimeTypeDef initial_time;
    initial_time.Hours = 10;
    initial_time.Minutes = 20;
    initial_time.Seconds = 0;
    initial_time.SubSeconds = 100;

    set_mocked_rtc_time(initial_time);

    RTC_DateTypeDef initial_date;
    initial_date.Year = 24;
    initial_date.Month = 12;
    initial_date.Date = 5;
    set_mocked_rtc_date(initial_date);

    // Call handleTaskImpl to execute the time synchronization
    task->handleTaskImpl();

    // Get the mocked RTC time and date after the synchronization
    RTC_TimeTypeDef synced_time = get_mocked_rtc_time();
    RTC_DateTypeDef synced_date = get_mocked_rtc_date();

    // Assert that the RTC time and date are updated correctly
    CHECK(synced_time.Hours == 10);
    CHECK(synced_time.Minutes == 29);
    CHECK(synced_time.Seconds == 59);
    CHECK(synced_date.Year == 24);
    CHECK(synced_date.Month == 12);
    CHECK(synced_date.Date == 5);
    CHECK(synced_time.SubSeconds == 255);

    clear_mocked_rtc();
    clear_uart_rx_buffer();
}