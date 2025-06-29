#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "TaskSGP4.hpp"
#include "cyphal.hpp"

#include "TimeUtils.hpp"
#include <chrono>
#include <cstdint>
#include <cmath>  // For std::abs
#include <limits> // For max and min values

#include "mock_hal.h"

#include "loopard_adapter.hpp"
#include "RegistrationManager.hpp"
#include "ServiceManager.hpp"
#include "Allocator.hpp"

TEST_CASE("duration_in_fractional_days - Basic")
{
    TimeUtils::DateTimeComponents start = {2024, 1, 1, 0, 0, 0, 0};
    TimeUtils::DateTimeComponents end = {2024, 1, 2, 0, 0, 0, 0};
    CHECK(duration_in_fractional_days(start, end) == doctest::Approx(1.0f));

    start = {2024, 1, 1, 0, 0, 0, 0};
    end = {2024, 1, 1, 12, 0, 0, 0};
    CHECK(duration_in_fractional_days(start, end) == doctest::Approx(0.5f));

    start = {2024, 1, 1, 0, 0, 0, 0};
    end = {2024, 1, 1, 0, 30, 0, 0};
    CHECK(duration_in_fractional_days(start, end) == doctest::Approx(30.0f / (24.0f * 60.0f)));

    start = {2024, 1, 1, 0, 0, 0, 0};
    end = {2024, 1, 1, 0, 0, 30, 0};
    CHECK(duration_in_fractional_days(start, end) == doctest::Approx(30.0f / (24.0f * 3600.0f)));
}

TEST_CASE("duration_in_fractional_days - Same day")
{
    TimeUtils::DateTimeComponents start = {2024, 5, 15, 10, 30, 0, 0};
    TimeUtils::DateTimeComponents end = {2024, 5, 15, 12, 0, 0, 0};
    CHECK(duration_in_fractional_days(start, end) == doctest::Approx(1.5f / 24.0f));
}

TEST_CASE("duration_in_fractional_days - Different months")
{
    TimeUtils::DateTimeComponents start = {2024, 1, 31, 0, 0, 0, 0};
    TimeUtils::DateTimeComponents end = {2024, 2, 1, 0, 0, 0, 0};
    CHECK(duration_in_fractional_days(start, end) == doctest::Approx(1.0f));
}

TEST_CASE("duration_in_fractional_days - Different years")
{
    TimeUtils::DateTimeComponents start = {2023, 12, 31, 0, 0, 0, 0};
    TimeUtils::DateTimeComponents end = {2024, 1, 1, 0, 0, 0, 0};
    CHECK(duration_in_fractional_days(start, end) == doctest::Approx(1.0f));
}

TEST_CASE("duration_in_fractional_days - Leap year test")
{
    TimeUtils::DateTimeComponents start = {2024, 2, 28, 0, 0, 0, 0};
    TimeUtils::DateTimeComponents end = {2024, 3, 1, 0, 0, 0, 0};
    CHECK(duration_in_fractional_days(start, end) == doctest::Approx(2.0f)); // 2 days because of Feb 29
}

TEST_CASE("duration_in_fractional_days - End before start (should be negative)")
{
    TimeUtils::DateTimeComponents start = {2024, 1, 2, 0, 0, 0, 0};
    TimeUtils::DateTimeComponents end = {2024, 1, 1, 0, 0, 0, 0};
    CHECK(duration_in_fractional_days(start, end) == doctest::Approx(-1.0f));
}

TEST_CASE("duration_in_fractional_days - Large duration")
{
    TimeUtils::DateTimeComponents start = {2000, 1, 1, 0, 0, 0, 0};
    TimeUtils::DateTimeComponents end = {2050, 1, 1, 0, 0, 0, 0};
    CHECK(duration_in_fractional_days(start, end) == 365.0f * 50 + 13);
}

TEST_CASE("duration_in_fractional_days - Millisecond Precision")
{
    TimeUtils::DateTimeComponents start = {2024, 1, 1, 0, 0, 0, 0};
    TimeUtils::DateTimeComponents end = {2024, 1, 1, 0, 0, 0, 500};
    CHECK(duration_in_fractional_days(start, end) == doctest::Approx(500.0f / (24.0f * 3600.0f * 1000.0f)));
}

TEST_CASE("year_day_to_date_time - Basic")
{
    TimeUtils::DateTimeComponents components = year_day_to_date_time(2024u, 1u);
    CHECK(components.year == 2024);
    CHECK(components.month == 1);
    CHECK(components.day == 1);
    CHECK(components.hour == 0);
    CHECK(components.minute == 0);
    CHECK(components.second == 0);
    CHECK(components.millisecond == 0);
}

TEST_CASE("year_day_to_date_time - Different day of year")
{
    TimeUtils::DateTimeComponents components = year_day_to_date_time(2024, 32);
    CHECK(components.year == 2024);
    CHECK(components.month == 2);
    CHECK(components.day == 1);
    CHECK(components.hour == 0);
    CHECK(components.minute == 0);
    CHECK(components.second == 0);
    CHECK(components.millisecond == 0);
}

TEST_CASE("year_day_to_date_time - Leap year day")
{
    TimeUtils::DateTimeComponents components = year_day_to_date_time(2024, 60); // Feb 29
    CHECK(components.year == 2024);
    CHECK(components.month == 2);
    CHECK(components.day == 29);
}

TEST_CASE("year_day_to_date_time - End of year")
{
    TimeUtils::DateTimeComponents components = year_day_to_date_time(2023, 365);
    CHECK(components.year == 2023);
    CHECK(components.month == 12);
    CHECK(components.day == 31);
}

TEST_CASE("year_day_to_date_time - Edge of year")
{
    TimeUtils::DateTimeComponents components = year_day_to_date_time(2024, 366); // Leap year
    CHECK(components.year == 2024);
    CHECK(components.month == 12);
    CHECK(components.day == 31);
}

TEST_CASE("year_day_to_date_time - Specific time components")
{
    TimeUtils::DateTimeComponents components = year_day_to_date_time(2024, 1, 12, 30, 45, 500);
    CHECK(components.year == 2024);
    CHECK(components.month == 1);
    CHECK(components.day == 1);
    CHECK(components.hour == 12);
    CHECK(components.minute == 30);
    CHECK(components.second == 45);
    CHECK(components.millisecond == 500);
}

TEST_CASE("year_day_to_date_time - Beginning of leap year")
{
    TimeUtils::DateTimeComponents components = year_day_to_date_time(2024, 1);
    CHECK(components.year == 2024);
    CHECK(components.month == 1);
    CHECK(components.day == 1);
}

TEST_CASE("year_day_to_date_time - Large day number with year roll over")
{
    TimeUtils::DateTimeComponents components1 = year_day_to_date_time(2023, 365); // Not leap year.
    CHECK(components1.year == 2023);
    CHECK(components1.month == 12);
    CHECK(components1.day == 31);

    TimeUtils::DateTimeComponents components2 = year_day_to_date_time(2023, 366); // Not leap year.
    CHECK(components2.year == 2024);
    CHECK(components2.month == 1);
    CHECK(components2.day == 1);
}

TEST_CASE("year_day_to_date_time - Minimal Year Day")
{
    TimeUtils::DateTimeComponents components = year_day_to_date_time(2000, 1);
    CHECK(components.year == 2000);
    CHECK(components.month == 1);
    CHECK(components.day == 1);
}

TEST_CASE("year_day_to_date_time - Year close to max uint16_t")
{
    TimeUtils::DateTimeComponents components = year_day_to_date_time(65535, 1);
    CHECK(components.year == 65535);
    CHECK(components.month == 1);
    CHECK(components.day == 1);
}

void *loopardMemoryAllocate(size_t amount) { return static_cast<void *>(malloc(amount)); };
void loopardMemoryFree(void *pointer) { free(pointer); };

TEST_CASE("receive one TLE")
{
    RTC_HandleTypeDef hrtc;
    set_current_tick(1001);

    constexpr CyphalNodeID id = 11;

    // Create adapters
    LoopardAdapter loopard;
    loopard.memory_allocate = loopardMemoryAllocate;
    loopard.memory_free = loopardMemoryFree;
    Cyphal<LoopardAdapter> loopard_cyphal(&loopard);
    loopard_cyphal.setNodeID(id);
    std::tuple<Cyphal<LoopardAdapter>> adapters(loopard_cyphal);

    auto task = std::make_shared<TaskSGP4<Cyphal<LoopardAdapter>>>(&hrtc, 1000, 0, static_cast<CyphalTransferID>(0), adapters);

    // Create a uavcan_time_Synchronization_1_0 message
    auto transfer = std::make_shared<CyphalTransfer>();
    _4111spyglass_sat_data_SPG4TLE_0_1 data{
        .satelliteNumber = 25544,
        .elementNumber = 999,
        .ephemerisType = 0,
        .epochYear = 25,
        .epochDay = 173.704,
        .meanMotionDerivative1 = 0.00010306f,
        .meanMotionDerivative2 = 0.0f,
        .bStarDrag = 0.00018707f,
        .inclination = 51.6391f,
        .rightAscensionAscendingNode = 279.729f,
        .eccentricity = 0.0002026f,
        .argumentOfPerigee = 272.772f,
        .meanAnomaly = 232.5f,
        .meanMotion = 15.5019f,
        .revolutionNumberAtEpoch = 51601};

    uint8_t payload[_4111spyglass_sat_data_SPG4TLE_0_1_SERIALIZATION_BUFFER_SIZE_BYTES_];
    size_t payload_size = sizeof(payload);
    _4111spyglass_sat_data_SPG4TLE_0_1_serialize_(&data, payload, &payload_size);

    transfer->payload = payload;
    transfer->payload_size = payload_size;

    SGP4TwoLineElement tle = task->getSGP4TLE();
    CHECK(tle.satelliteNumber == 0);

    task->handleMessage(transfer);
    task->handleTask();

    tle = task->getSGP4TLE();
    CHECK(tle.satelliteNumber == data.satelliteNumber);
    CHECK(tle.elementNumber == data.elementNumber);
    CHECK(tle.ephemerisType == data.ephemerisType);
    CHECK(tle.epochYear == data.epochYear);
    CHECK(tle.meanMotionDerivative1 == data.meanMotionDerivative1);
    CHECK(tle.meanMotionDerivative2 == data.meanMotionDerivative2);
    CHECK(tle.bStarDrag == data.bStarDrag);
    CHECK(tle.inclination == data.inclination);
    CHECK(tle.rightAscensionAscendingNode == data.rightAscensionAscendingNode);
    CHECK(tle.eccentricity == data.eccentricity);
    CHECK(tle.argumentOfPerigee == data.argumentOfPerigee);
    CHECK(tle.meanAnomaly == data.meanAnomaly);
    CHECK(tle.meanMotion == data.meanMotion);
    CHECK(tle.revolutionNumberAtEpoch == data.revolutionNumberAtEpoch);
}

TEST_CASE("receive two TLE")
{
    RTC_HandleTypeDef hrtc;
    set_current_tick(1001);

    constexpr CyphalNodeID id = 11;

    // Create adapters
    LoopardAdapter loopard;
    loopard.memory_allocate = loopardMemoryAllocate;
    loopard.memory_free = loopardMemoryFree;
    Cyphal<LoopardAdapter> loopard_cyphal(&loopard);
    loopard_cyphal.setNodeID(id);
    std::tuple<Cyphal<LoopardAdapter>> adapters(loopard_cyphal);

    auto task = std::make_shared<TaskSGP4<Cyphal<LoopardAdapter>>>(&hrtc, 1000, 0, static_cast<CyphalTransferID>(0), adapters);

    // Create a uavcan_time_Synchronization_1_0 message
    auto transfer = std::make_shared<CyphalTransfer>();
    _4111spyglass_sat_data_SPG4TLE_0_1 data{
        .satelliteNumber = 25544,
        .elementNumber = 999,
        .ephemerisType = 0,
        .epochYear = 25,
        .epochDay = 173.704,
        .meanMotionDerivative1 = 0.00010306f,
        .meanMotionDerivative2 = 0.0f,
        .bStarDrag = 0.00018707f,
        .inclination = 51.6391f,
        .rightAscensionAscendingNode = 279.729f,
        .eccentricity = 0.0002026f,
        .argumentOfPerigee = 272.772f,
        .meanAnomaly = 232.5f,
        .meanMotion = 15.5019f,
        .revolutionNumberAtEpoch = 51601};

    uint8_t payload[_4111spyglass_sat_data_SPG4TLE_0_1_SERIALIZATION_BUFFER_SIZE_BYTES_];
    size_t payload_size = sizeof(payload);
    _4111spyglass_sat_data_SPG4TLE_0_1_serialize_(&data, payload, &payload_size);

    transfer->payload = payload;
    transfer->payload_size = payload_size;

    SGP4TwoLineElement tle = task->getSGP4TLE();
    CHECK(tle.satelliteNumber == 0);

    task->handleMessage(transfer);

    data = _4111spyglass_sat_data_SPG4TLE_0_1{
        .satelliteNumber = 99999,
        .elementNumber = 999,
        .ephemerisType = 0,
        .epochYear = 25,
        .epochDay = 173.704,
        .meanMotionDerivative1 = 0.00010306f,
        .meanMotionDerivative2 = 0.0f,
        .bStarDrag = 0.00018707f,
        .inclination = 51.6391f,
        .rightAscensionAscendingNode = 279.729f,
        .eccentricity = 0.0002026f,
        .argumentOfPerigee = 272.772f,
        .meanAnomaly = 232.5f,
        .meanMotion = 15.5019f,
        .revolutionNumberAtEpoch = 77777};

    _4111spyglass_sat_data_SPG4TLE_0_1_serialize_(&data, payload, &payload_size);

    transfer->payload = payload;
    transfer->payload_size = payload_size;

    task->handleMessage(transfer);
    task->handleTask();

    tle = task->getSGP4TLE();
    CHECK(tle.satelliteNumber == 99999);
    CHECK(tle.elementNumber == data.elementNumber);
    CHECK(tle.ephemerisType == data.ephemerisType);
    CHECK(tle.epochYear == data.epochYear);
    CHECK(tle.meanMotionDerivative1 == data.meanMotionDerivative1);
    CHECK(tle.meanMotionDerivative2 == data.meanMotionDerivative2);
    CHECK(tle.bStarDrag == data.bStarDrag);
    CHECK(tle.inclination == data.inclination);
    CHECK(tle.rightAscensionAscendingNode == data.rightAscensionAscendingNode);
    CHECK(tle.eccentricity == data.eccentricity);
    CHECK(tle.argumentOfPerigee == data.argumentOfPerigee);
    CHECK(tle.meanAnomaly == data.meanAnomaly);
    CHECK(tle.meanMotion == data.meanMotion);
    CHECK(tle.revolutionNumberAtEpoch == data.revolutionNumberAtEpoch);
}

TEST_CASE("send position")
{
    RTC_HandleTypeDef hrtc;
    hrtc.Init.SynchPrediv = 1023;
    set_current_tick(1001);

    TimeUtils::DateTimeComponents components{
        .year = 2025,
        .month = 6,
        .day = 25,
        .hour = 18,
        .minute = 0,
        .second = 0,
        .millisecond = 0};
    TimeUtils::RTCDateTimeSubseconds rtc = TimeUtils::to_rtc(components, hrtc.Init.SynchPrediv);
    HAL_RTC_SetTime(&hrtc, &rtc.time, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &rtc.date, RTC_FORMAT_BIN);
    HAL_RTCEx_SetSynchroShift(RTC_SHIFTADD1S_RESET, rtc.time.SubSeconds);

    constexpr CyphalNodeID id = 11;

    // Create adapters
    LoopardAdapter loopard;
    loopard.memory_allocate = loopardMemoryAllocate;
    loopard.memory_free = loopardMemoryFree;
    Cyphal<LoopardAdapter> loopard_cyphal(&loopard);
    loopard_cyphal.setNodeID(id);
    std::tuple<Cyphal<LoopardAdapter>> adapters(loopard_cyphal);

    auto task = std::make_shared<TaskSGP4<Cyphal<LoopardAdapter>>>(&hrtc, 1000, 0, static_cast<CyphalTransferID>(0), adapters);

    // Create a uavcan_time_Synchronization_1_0 message
    //char longstr1[] = "1 25544U 98067A   25176.66408718  .00008346  00000-0  15277-3 0  9997";
    //char longstr2[] = "2 25544  51.6390 265.0566 0001950 279.7547 194.0443 15.50239840516477";

    // ISS (ZARYA)
    char longstr1[] = "1 25544U 98067A   25176.73245655  .00008102  00000-0  14854-3 0  9994";
    char longstr2[] = "2 25544  51.6390 264.7180 0001990 278.3788 217.2311 15.50240116516482";  

    auto parsed = sgp4_utils::parseTLE(longstr1, longstr2);
    REQUIRE(parsed.has_value());
    SGP4TwoLineElement data = parsed.value();
    task->setSGP4TLE(data);
    
    SGP4TwoLineElement tle = task->getSGP4TLE();
    CHECK(tle.satelliteNumber == data.satelliteNumber);
    CHECK(tle.elementNumber == data.elementNumber);
    CHECK(tle.ephemerisType == data.ephemerisType);
    CHECK(tle.epochYear == data.epochYear);
    CHECK(tle.meanMotionDerivative1 == data.meanMotionDerivative1);
    CHECK(tle.meanMotionDerivative2 == data.meanMotionDerivative2);
    CHECK(tle.bStarDrag == data.bStarDrag);
    CHECK(tle.inclination == data.inclination);
    CHECK(tle.rightAscensionAscendingNode == data.rightAscensionAscendingNode);
    CHECK(tle.eccentricity == data.eccentricity);
    CHECK(tle.argumentOfPerigee == data.argumentOfPerigee);
    CHECK(tle.meanAnomaly == data.meanAnomaly);
    CHECK(tle.meanMotion == data.meanMotion);
    CHECK(tle.revolutionNumberAtEpoch == data.revolutionNumberAtEpoch);

    CHECK(loopard.buffer.is_empty());
    task->handleTask();
    REQUIRE(loopard.buffer.size() == 1);

    auto transfer = loopard.buffer.pop();
    CHECK(transfer.metadata.port_id == _4111spyglass_sat_model_PositionVelocity_0_1_PORT_ID_);
    CHECK(transfer.metadata.transfer_kind == CyphalTransferKindMessage);
    CHECK(transfer.metadata.remote_node_id == id);
    CHECK(transfer.payload_size == _4111spyglass_sat_model_PositionVelocity_0_1_SERIALIZATION_BUFFER_SIZE_BYTES_);

    // Deserialize the payload and verify the values
    _4111spyglass_sat_model_PositionVelocity_0_1 received_data;
    size_t deserialized_size = transfer.payload_size;
    int8_t deserialization_result = _4111spyglass_sat_model_PositionVelocity_0_1_deserialize_(&received_data, static_cast<const uint8_t*>(transfer.payload), &deserialized_size);
    REQUIRE(deserialization_result >= 0);
    CHECK(received_data.timestamp.microsecond == 804189600000000);
    CHECK(received_data.position_m[0] == doctest::Approx(3.24706e+06f));
    CHECK(received_data.velocity_ms[0] == doctest::Approx(-3079.46f));
    loopardMemoryFree(transfer.payload);

}