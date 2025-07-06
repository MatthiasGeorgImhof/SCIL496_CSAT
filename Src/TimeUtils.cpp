#include "TimeUtils.hpp"
#include <iostream> //debugging
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime> // Required for std::tm and std::mktime

namespace TimeUtils
{

    // Custom epoch
    constexpr auto epoch = std::chrono::sys_days{std::chrono::year{EPOCH_YEAR} / std::chrono::month(EPOCH_MONTH) / EPOCH_DAY}; // 2000-01-01 00:00:00 GMT

    // Convert system_clock::time_point to epoch_duration
    epoch_duration to_epoch_duration(const std::chrono::system_clock::time_point &tp)
    {
        return std::chrono::duration_cast<epoch_duration>(tp - epoch);
    }

    // Convert epoch_duration to system_clock::time_point
    std::chrono::system_clock::time_point to_timepoint(const epoch_duration &d)
    {
        return epoch + d;
    }

    // date and time to duration
    std::chrono::system_clock::time_point to_timepoint(const DateTimeComponents &components)
    {
        std::chrono::year year_chrono(components.year);
        std::chrono::month month_chrono(components.month);
        std::chrono::day day_chrono(components.day);

        std::chrono::sys_days date_chrono = year_chrono / month_chrono / day_chrono;
        std::chrono::hours hours_chrono(components.hour);
        std::chrono::minutes minutes_chrono(components.minute);
        std::chrono::seconds seconds_chrono(components.second);
        std::chrono::milliseconds milliseconds_chrono(components.millisecond);

        return date_chrono + hours_chrono + minutes_chrono + seconds_chrono + milliseconds_chrono;
    }

    epoch_duration to_epoch_duration(const DateTimeComponents &components)
    {
        return to_epoch_duration(to_timepoint(components));
    }

    std::chrono::system_clock::time_point  to_timepoint(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, int32_t nanosecond)
    {
        std::chrono::year year_chrono(year);
        std::chrono::month month_chrono(month);
        std::chrono::day day_chrono(day);

        std::chrono::sys_days date_chrono = year_chrono / month_chrono / day_chrono;
        std::chrono::hours hours_chrono(hour);
        std::chrono::minutes minutes_chrono(minute);
        std::chrono::seconds seconds_chrono(second);
        std::chrono::nanoseconds nanoseconds_chrono(nanosecond);

        return date_chrono + hours_chrono + minutes_chrono + seconds_chrono + nanoseconds_chrono;
    }

    epoch_duration to_epoch_duration(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, int32_t nanosecond)
    {
        return to_epoch_duration(to_timepoint(year, month, day, hour, minute, second, nanosecond));
    }

    // Function to convert year and fractional day to std::chrono::time_point
    std::chrono::system_clock::time_point to_timepoint(uint16_t past_year, float past_fractional_day)
    {
        using namespace std::chrono;

        std::stringstream past_date_stream;
        past_date_stream << past_year << "-01-01 00:00:00.000"; // Year starts on Jan 1
        std::tm past_tm = {};
        past_date_stream >> std::get_time(&past_tm, "%Y-%m-%d %H:%M:%S.%OS"); // parse year
        if (past_date_stream.fail())
        {
            return std::chrono::system_clock::time_point::min(); // Return min time_point on failure
        }
        return std::chrono::system_clock::from_time_t(std::mktime(&past_tm)) + std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double, std::ratio<86400>>(past_fractional_day - 1.0));
    }

    // Function to convert the difference between two time_points to fractional days
    float to_fractional_days(const std::chrono::system_clock::time_point &start, const std::chrono::system_clock::time_point &end)
    {
        using namespace std::chrono;

        auto time_diff = end - start;
        std::chrono::duration<double, std::ratio<86400>> days_diff = time_diff; // 86400 seconds in a day
        return static_cast<float>(days_diff.count());                           // Explicit cast to float
    }

    // Function to calculate the number of fractional days between two dates (using helper functions)
    float fractional_days_between(
        uint16_t past_year, float past_fractional_day,
        uint16_t current_year, uint8_t current_month, uint8_t current_day,
        uint8_t current_hour, uint8_t current_minute, uint8_t current_second, uint16_t current_millisecond)
    {

        DateTimeComponents current_components = {current_year, current_month, current_day, current_hour, current_minute, current_second, current_millisecond};
        auto past_time_point = to_timepoint(past_year, past_fractional_day);
        auto current_time_point = to_timepoint(current_components);

        // Check for invalid time_points (parsing failures)
        if (past_time_point == std::chrono::system_clock::time_point::min() || current_time_point == std::chrono::system_clock::time_point::min())
        {
            return -1.0f; // Indicate an error
        }

        return to_fractional_days(past_time_point, current_time_point);
    }

    // extract year, month, day, hour, minute_v, second_v, millisecond_v from epoch_duration
    DateTimeComponents extract_date_time(const epoch_duration &d)
    {
        using namespace std::chrono;

        // Convert the epoch_duration to a time_point relative to the epoch.
        system_clock::time_point tp = to_timepoint(d);

        // Get the time_point since the standard epoch (1970-01-01).
        auto time_since_epoch = tp.time_since_epoch();

        // Calculate the number of days since the standard epoch.
        auto days_since_epoch = duration_cast<days>(time_since_epoch);

        // Convert the number of days since the standard epoch to a sys_days object.
        sys_days day_point(days_since_epoch);

        // Convert the sys_days object to a year_month_day object.
        year_month_day ymd(day_point);

        // Calculate the time of day, which represents the remaining duration since midnight.
        auto time_of_day = time_since_epoch - days_since_epoch;

        // Declare the duration types *before* using duration_cast
        std::chrono::hours hours;
        std::chrono::minutes minutes;
        std::chrono::seconds seconds;
        std::chrono::milliseconds milliseconds;

        // Extract hours, minutes, seconds, and milliseconds from the time_of_day.
        hours = duration_cast<std::chrono::hours>(time_of_day);
        time_of_day -= hours;
        minutes = duration_cast<std::chrono::minutes>(time_of_day);
        time_of_day -= minutes;
        seconds = duration_cast<std::chrono::seconds>(time_of_day);
        time_of_day -= seconds;
        milliseconds = duration_cast<std::chrono::milliseconds>(time_of_day);

        DateTimeComponents components;
        components.year = static_cast<uint16_t>(static_cast<int>(ymd.year()));
        components.month = static_cast<uint8_t>(static_cast<unsigned int>(ymd.month()));
        components.day = static_cast<uint8_t>(static_cast<unsigned int>(ymd.day()));
        components.hour = static_cast<uint8_t>(hours.count());
        components.minute = static_cast<uint8_t>(minutes.count());
        components.second = static_cast<uint8_t>(seconds.count());
        components.millisecond = static_cast<uint16_t>(milliseconds.count());

        return components;
    }

    // Convert epoch_duration to uint64_t
    uint64_t to_uint64(const epoch_duration &d)
    {
        return static_cast<uint64_t>(d.count());
    }

    // Convert uint64_t back to epoch_duration
    epoch_duration from_uint64(uint64_t value)
    {
        return epoch_duration(static_cast<typename epoch_duration::rep>(value));
    }

    // Convert from STM32 RTC to epoch_duration
    epoch_duration from_rtc(const RTCDateTimeSubseconds &rtcdatetimesubseconds, uint32_t secondFraction)
    {
        DateTimeComponents components;
        components.year = rtcdatetimesubseconds.date.Year + EPOCH_YEAR; // RTC Year is relative to 2000
        components.month = rtcdatetimesubseconds.date.Month;
        components.day = rtcdatetimesubseconds.date.Date;
        components.hour = rtcdatetimesubseconds.time.Hours;
        components.minute = rtcdatetimesubseconds.time.Minutes;
        components.second = rtcdatetimesubseconds.time.Seconds;

        // Calculate milliseconds from subSeconds and secondFraction
        components.millisecond = static_cast<uint16_t>(static_cast<uint64_t>(1000 * (secondFraction - rtcdatetimesubseconds.time.SubSeconds) / (secondFraction + 1)));

        return to_epoch_duration(components);
    }

    // Convert to STM32 RTC
    RTCDateTimeSubseconds to_rtc(const DateTimeComponents &components, uint32_t secondFraction)
    {
        auto y = std::chrono::year(components.year);
        auto m = std::chrono::month(components.month);
        auto day_v = std::chrono::day(components.day);
        std::chrono::year_month_day ymd(y, m, day_v);

        return RTCDateTimeSubseconds{
            RTC_DateTypeDef{
                .WeekDay = static_cast<uint8_t>(std::chrono::weekday(ymd).iso_encoding()),
                .Month = static_cast<uint8_t>(components.month),
                .Date = static_cast<uint8_t>(components.day),
                .Year = static_cast<uint8_t>(components.year - EPOCH_YEAR)},
            RTC_TimeTypeDef{
                .Hours = static_cast<uint8_t>(components.hour),
                .Minutes = static_cast<uint8_t>(components.minute),
                .Seconds = static_cast<uint8_t>(components.second),
                .TimeFormat = RTC_FORMAT_BIN,
                .SubSeconds = secondFraction - static_cast<uint32_t>(static_cast<uint64_t>(components.millisecond) * (secondFraction + 1) / 1000),
                .SecondFraction = secondFraction,
                .DayLightSaving = RTC_DAYLIGHTSAVING_NONE,
                .StoreOperation = RTC_STOREOPERATION_RESET}};
    }

    RTCDateTimeSubseconds to_rtc(const epoch_duration &d, uint32_t secondFraction)
    {
        return to_rtc(extract_date_time(d), secondFraction);
    }

} // namespace TimeUtils