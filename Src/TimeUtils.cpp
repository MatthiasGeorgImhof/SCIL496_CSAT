#include "TimeUtils.hpp"
#include <iostream> //debugging
#include <chrono>

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
    std::chrono::system_clock::time_point from_epoch_duration(const epoch_duration &d)
    {
        return epoch + d;
    }

    // date and time to duration
    epoch_duration to_epoch_duration(const DateTimeComponents &components)
    {
        std::chrono::year year_chrono(components.year);
        std::chrono::month month_chrono(components.month);
        std::chrono::day day_chrono(components.day);

        std::chrono::sys_days date_chrono = year_chrono / month_chrono / day_chrono;
        std::chrono::hours hours_chrono(components.hour);
        std::chrono::minutes minutes_chrono(components.minute);
        std::chrono::seconds seconds_chrono(components.second);
        std::chrono::milliseconds milliseconds_chrono(components.millisecond);

        std::chrono::system_clock::time_point tp = date_chrono + hours_chrono + minutes_chrono + seconds_chrono + milliseconds_chrono;

        return to_epoch_duration(tp);
    }

    epoch_duration to_epoch_duration(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, int32_t nanosecond)
    {
        std::chrono::year year_chrono(year);
        std::chrono::month month_chrono(month);
        std::chrono::day day_chrono(day);

        std::chrono::sys_days date_chrono = year_chrono / month_chrono / day_chrono;
        std::chrono::hours hours_chrono(hour);
        std::chrono::minutes minutes_chrono(minute);
        std::chrono::seconds seconds_chrono(second);
        std::chrono::nanoseconds nanoseconds_chrono(nanosecond);

        std::chrono::system_clock::time_point tp = date_chrono + hours_chrono + minutes_chrono + seconds_chrono + nanoseconds_chrono;

        return to_epoch_duration(tp);
    }


    // extract year, month, day, hour, minute_v, second_v, millisecond_v from epoch_duration
    DateTimeComponents extract_date_time(const epoch_duration &d)
    {
        using namespace std::chrono;

        // Convert the epoch_duration to a time_point relative to the epoch.
        system_clock::time_point tp = from_epoch_duration(d);

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
        components.year = static_cast<int>(static_cast<int>(ymd.year()));                     // Corrected
        components.month = static_cast<unsigned int>(static_cast<unsigned int>(ymd.month())); // Corrected
        components.day = static_cast<unsigned int>(static_cast<unsigned int>(ymd.day()));     // Corrected
        components.hour = static_cast<unsigned int>(hours.count());
        components.minute = static_cast<unsigned int>(minutes.count());
        components.second = static_cast<unsigned int>(seconds.count());
        components.millisecond = static_cast<unsigned int>(milliseconds.count());

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
        components.millisecond = static_cast<int>(static_cast<uint64_t>(1000 * (secondFraction - rtcdatetimesubseconds.time.SubSeconds) / (secondFraction + 1)));

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