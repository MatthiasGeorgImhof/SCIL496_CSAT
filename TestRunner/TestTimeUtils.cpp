#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "TimeUtils.hpp"
#include <chrono>
#include <cstdint>

#include "mock_hal.h"

TEST_CASE("RTC <-> epoch_duration Conversions") {
    // Sample RTC values
    constexpr uint32_t secondFraction = 1023;
    constexpr uint32_t subSeconds = 500;
    
    
    RTC_DateTypeDef rtc_date {};
    rtc_date.Year = 24;    // Year 2024 (relative to 2000)
    rtc_date.Month = 10;
    rtc_date.Date = 27;

    RTC_TimeTypeDef rtc_time;
    rtc_time.Hours = 10;
    rtc_time.Minutes = 30;
    rtc_time.Seconds = 15;
    rtc_time.TimeFormat = RTC_HOURFORMAT_24;
    rtc_time.SubSeconds = subSeconds; 
    rtc_time.SecondFraction = secondFraction; 

    TimeUtils::RTCDateTimeSubseconds rtcdatetimeseconds = {
        .date = rtc_date,
        .time = rtc_time,
    };

    // Convert from RTC to epoch_duration
    TimeUtils::epoch_duration time1 = TimeUtils::from_rtc(rtcdatetimeseconds, secondFraction);

    // Convert back to RTC
    TimeUtils::RTCDateTimeSubseconds rtc_date_back_time = TimeUtils::to_rtc(time1, secondFraction);

    // Check if the values are approximately equal
    CHECK(rtc_date_back_time.date.Year == rtc_date.Year);
    CHECK(rtc_date_back_time.date.Month == rtc_date.Month);
    CHECK(rtc_date_back_time.date.Date == rtc_date.Date);
    CHECK(rtc_date_back_time.time.Hours == rtc_time.Hours);
    CHECK(rtc_date_back_time.time.Minutes == rtc_time.Minutes);
    CHECK(rtc_date_back_time.time.Seconds == rtc_time.Seconds);
    // Subsecond approximation, allow a small difference
    CHECK(abs(static_cast<int>(rtc_date_back_time.time.SubSeconds) - static_cast<int>(subSeconds)) < 10); //Adjust tolerance as needed
}

TEST_CASE("Time Conversions and Extraction") {
    // Test case 1: Conversion from date/time components to duration
    TimeUtils::DateTimeComponents components1 = {2024, 10, 27, 10, 30, 15, 500};
    TimeUtils::epoch_duration time1 = TimeUtils::to_epoch_duration(components1);
    CHECK(time1.count() > 0); // Basic sanity check

    // Test case 2: Extraction of components
    TimeUtils::DateTimeComponents extractedComponents = TimeUtils::extract_date_time(time1);

    CHECK(extractedComponents.year == 2024);
    CHECK(extractedComponents.month == 10);
    CHECK(extractedComponents.day == 27);
    CHECK(extractedComponents.hour == 10);
    CHECK(extractedComponents.minute == 30);
    CHECK(extractedComponents.second == 15);
    CHECK(extractedComponents.millisecond == 500);

    // Test case 3: Round trip conversion (date/time -> duration -> date/time)
    TimeUtils::DateTimeComponents components2 = {2023, 1, 1, 0, 0, 0, 0};
    TimeUtils::epoch_duration time2 = TimeUtils::to_epoch_duration(components2);
    TimeUtils::DateTimeComponents extractedComponents2 = TimeUtils::extract_date_time(time2);
    CHECK(extractedComponents2.year == 2023);
    CHECK(extractedComponents2.month == 1);
    CHECK(extractedComponents2.day == 1);
    CHECK(extractedComponents2.hour == 0);
    CHECK(extractedComponents2.minute == 0);
    CHECK(extractedComponents2.second == 0);
    CHECK(extractedComponents2.millisecond == 0);

    // Test case 4: Check the epoch
    TimeUtils::DateTimeComponents epochComponents = {TimeUtils::EPOCH_YEAR, TimeUtils::EPOCH_MONTH, TimeUtils::EPOCH_DAY, 0, 0, 0, 0};
    TimeUtils::epoch_duration epoch_duration_value = TimeUtils::to_epoch_duration(epochComponents);
    CHECK(epoch_duration_value.count() == 0);
}

TEST_CASE("Duration Arithmetic") {
    TimeUtils::DateTimeComponents components1 = {2024, 1, 1, 0, 0, 0, 0};
    TimeUtils::epoch_duration time1 = TimeUtils::to_epoch_duration(components1);
    TimeUtils::epoch_duration time2 = time1 + std::chrono::seconds(60); // Add 60 seconds

    TimeUtils::DateTimeComponents extractedComponents = TimeUtils::extract_date_time(time2);

    CHECK(extractedComponents.year == 2024);
    CHECK(extractedComponents.month == 1);
    CHECK(extractedComponents.day == 1);
    CHECK(extractedComponents.hour == 0);
    CHECK(extractedComponents.minute == 1);
    CHECK(extractedComponents.second == 0);
    CHECK(extractedComponents.millisecond == 0);
}

TEST_CASE("Edge Cases and Error Handling (Illustrative)") {
    // Note:  This is where you'd add checks for boundary conditions and potential
    // errors, like very large durations that might cause overflows, or invalid date values.
    // For demonstration, let's just check a very early date.  Proper error handling
    // would involve throwing exceptions or returning error codes from the functions.

    TimeUtils::DateTimeComponents epochComponents = {TimeUtils::EPOCH_YEAR, TimeUtils::EPOCH_MONTH, TimeUtils::EPOCH_DAY, 0, 0, 0, 0};
    TimeUtils::epoch_duration early_time = TimeUtils::to_epoch_duration(epochComponents); // The epoch
    CHECK(early_time.count() == 0);
}