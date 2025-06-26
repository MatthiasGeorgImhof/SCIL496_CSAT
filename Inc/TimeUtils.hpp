#ifndef TIMEUTILS_HPP
#define TIMEUTILS_HPP

#include <chrono>
#include <ctime>
#include <cstdint>

#ifdef __arm__
#include "usbd_cdc_if.h"
#endif
#ifdef __x86_64__
#include "mock_hal.h"
#endif


namespace TimeUtils {

    constexpr uint16_t EPOCH_YEAR = 2000;
    constexpr uint8_t EPOCH_MONTH = 1;
    constexpr uint8_t EPOCH_DAY = 1;

    // Define a struct for date and time components
    struct DateTimeComponents {
        uint16_t year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        uint16_t millisecond;
    };

    struct RTCDateTimeSubseconds {
        RTC_DateTypeDef date;       // Date components
        RTC_TimeTypeDef time;       // Time components
    };
    
    // Choose your duration type
    using epoch_duration = std::chrono::milliseconds; // or std::chrono::microseconds, or std::chrono::nanoseconds
    using epoch_time_point = std::chrono::time_point<std::chrono::system_clock, epoch_duration>;

    // Function declarations
    epoch_duration to_epoch_duration(const std::chrono::system_clock::time_point& tp);
    epoch_duration to_epoch_duration(const DateTimeComponents& components);
    epoch_duration to_epoch_duration(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minure, uint8_t second, int32_t nanosecond);

    std::chrono::system_clock::time_point from_epoch_duration(const epoch_duration& d);

    DateTimeComponents extract_date_time(const epoch_duration& d); // Return the struct

    // Function to get epoch_duration as uint64_t
    uint64_t to_uint64(const epoch_duration& d);

    // Function to convert uint64_t back to epoch_duration
    epoch_duration from_uint64(uint64_t value);

    // Convert from STM32 RTC to epoch_duration
    epoch_duration from_rtc(const RTCDateTimeSubseconds &rtcdatetimesubseconds, uint32_t secondFraction);

    RTCDateTimeSubseconds to_rtc(const epoch_duration& d, uint32_t secondFraction);
    RTCDateTimeSubseconds to_rtc(const DateTimeComponents& components, uint32_t secondFraction);

} // namespace TimeUtils

#endif // TimeUtils_HPP