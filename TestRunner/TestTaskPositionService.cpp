#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "TaskPositionService.hpp"
#include "PositionTracker9D.hpp"
#include "SGP4PositionTracker.hpp"
#include "GNSS.hpp"
#include "BMI270.hpp"
#include "mock_hal.h"
#include "TimeUtils.hpp"
#include "RegistrationManager.hpp"
#include "loopard_adapter.hpp"
#include <array>
#include <tuple>

// Mock GNSS class
class MockGNSS
{
public:
    MockGNSS() : has_data(false) {}

    void setPosition(int32_t x, int32_t y, int32_t z)
    {
        position.ecefX = x;
        position.ecefY = y;
        position.ecefZ = z;
        has_data = true;
    }

    std::optional<PositionECEF> GetNavPosECEF()
    {
        if (has_data)
        {
            return position;
        }
        else
        {
            return std::nullopt;
        }
    }

private:
    PositionECEF position;
    bool has_data;
};

// Mock IMU class
class MockIMU
{
public:
    MockIMU() : has_data(false) {}

    void setAcceleration(float x, float y, float z)
    {
        acceleration.x = au::make_quantity<au::MetersPerSecondSquared>(x);
        acceleration.y = au::make_quantity<au::MetersPerSecondSquared>(y);
        acceleration.z = au::make_quantity<au::MetersPerSecondSquared>(z);
        has_data = true;
    }

    std::optional<Accelerometer> getAcceleration()
    {
        if (has_data)
        {
            return acceleration;
        }
        else
        {
            return std::nullopt;
        }
    }

private:
    Accelerometer acceleration;
    bool has_data;
};

class MockSGP4
{
public:
    MockSGP4() : has_data(false) {}

    void setPosition(float x, float y, float z)
    {
        position[0] = au::make_quantity<au::Meters>(x);
        position[1] = au::make_quantity<au::Meters>(y);
        position[2] = au::make_quantity<au::Meters>(z);
        has_data = true;
    }

    void setVelocity(float x, float y, float z)
    {
        velocity[0] = au::make_quantity<au::MetersPerSecond>(x);
        velocity[1] = au::make_quantity<au::MetersPerSecond>(y);
        velocity[2] = au::make_quantity<au::MetersPerSecond>(z);
    }

    bool predict(std::array<au::QuantityF<au::Meters>, 3> &r, std::array<au::QuantityF<au::MetersPerSecond>, 3> &v, au::QuantityU64<au::Milli<au::Seconds>> timestamp)
    {
        (void)timestamp;
        if (has_data)
        {
            r[0] = position[0];
            r[1] = position[1];
            r[2] = position[2];
            v[0] = velocity[0];
            v[1] = velocity[1];
            v[2] = velocity[2];
            return true;
        }
        else
        {
            return false;
        }
    }

private:
    std::array<au::QuantityF<au::Meters>, 3> position;
    std::array<au::QuantityF<au::MetersPerSecond>, 3> velocity;
    bool has_data;
};
void *loopardMemoryAllocate(size_t amount) { return static_cast<void *>(malloc(amount)); };
void loopardMemoryFree(void *pointer) { free(pointer); };

TEST_CASE("TaskPositionService Test with GNSSandAccelPosition")
{
    // Arrange
    RTC_HandleTypeDef hrtc;
    const uint32_t secondFraction = 1023;
    hrtc.Init.SynchPrediv = secondFraction;

    TimeUtils::DateTimeComponents dtc{
        .year = 2000,
        .month = 1,
        .day = 1,
        .hour = 0,
        .minute = 0,
        .second = 1,
        .millisecond = 0};
    auto duration = TimeUtils::to_epoch_duration(dtc);
    auto rtc = TimeUtils::to_rtc(duration, secondFraction);
    set_mocked_rtc_time(rtc.time);
    set_mocked_rtc_date(rtc.date);

    constexpr CyphalNodeID id = 11;
    LoopardAdapter loopard;
    loopard.memory_allocate = loopardMemoryAllocate;
    loopard.memory_free = loopardMemoryFree;
    Cyphal<LoopardAdapter> loopard_cyphal(&loopard);
    loopard_cyphal.setNodeID(id);
    std::tuple<Cyphal<LoopardAdapter>> adapters(loopard_cyphal);

    MockGNSS gnss;
    MockIMU imu;
    PositionTracker9D tracker;
    GNSSandAccelPosition<PositionTracker9D, MockGNSS, MockIMU> positionTracker(&hrtc, tracker, gnss, imu);
    auto task = TaskPositionService<GNSSandAccelPosition<PositionTracker9D, MockGNSS, MockIMU>, Cyphal<LoopardAdapter>>(positionTracker, 100, 1, 123, adapters);

    const float x0 = 100.0f;
    const float y0 = 200.0f;
    const float z0 = 300.0f;

    const float vx0 = 10.0f;
    const float vy0 = 20.0f;
    const float vz0 = 30.0f;

    const float ax0 = 0.1f;
    const float ay0 = 0.2f;
    const float az0 = 0.3f;

    const float dt = 0.1f; // 100 ms
    std::chrono::milliseconds dduration(100);

    for (int i = 0; i < 100; ++i)
    {
        float t = dt * static_cast<float>(i);

        float x = x0 + vx0 * t + 0.5f * ax0 * t * t;
        float y = y0 + vy0 * t + 0.5f * ay0 * t * t;
        float z = z0 + vz0 * t + 0.5f * az0 * t * t;

        // GNSS returns cm!
        gnss.setPosition(static_cast<int32_t>(x * 1e2f), static_cast<int32_t>(y * 1e2f), static_cast<int32_t>(z * 1e2f));
        imu.setAcceleration(ax0, ay0, az0);
        task.handleTaskImpl();
        REQUIRE(loopard.buffer.size() == 1);

        auto transfer = loopard.buffer.pop();
        CHECK(transfer.metadata.port_id == _4111spyglass_sat_model_PositionVelocity_0_1_PORT_ID_);
        CHECK(transfer.metadata.transfer_kind == CyphalTransferKindMessage);
        CHECK(transfer.metadata.remote_node_id == id);
        CHECK(transfer.payload_size == _4111spyglass_sat_model_PositionVelocity_0_1_SERIALIZATION_BUFFER_SIZE_BYTES_);

        _4111spyglass_sat_model_PositionVelocity_0_1 received_data;
        size_t deserialized_size = transfer.payload_size;
        int8_t deserialization_result = _4111spyglass_sat_model_PositionVelocity_0_1_deserialize_(&received_data, static_cast<const uint8_t *>(transfer.payload), &deserialized_size);
        REQUIRE(deserialization_result >= 0);

        float vx = vx0 + ax0 * t;
        float vy = vy0 + ay0 * t;
        float vz = vz0 + az0 * t;

        if (i > 50)
        {
            CHECK(received_data.timestamp.microsecond == duration.count() * 1000);
            CHECK(received_data.position_m[0] == doctest::Approx(x).epsilon(0.1f));
            CHECK(received_data.position_m[1] == doctest::Approx(y).epsilon(0.1f));
            CHECK(received_data.position_m[2] == doctest::Approx(z).epsilon(0.1f));
            CHECK(received_data.velocity_ms[0] == doctest::Approx(vx).epsilon(0.1f));
            CHECK(received_data.velocity_ms[1] == doctest::Approx(vy).epsilon(0.1f));
            CHECK(received_data.velocity_ms[2] == doctest::Approx(vz).epsilon(0.1f));
        }
        duration += dduration;
        auto rtc = TimeUtils::to_rtc(duration, secondFraction);
        set_mocked_rtc_time(rtc.time);
        set_mocked_rtc_date(rtc.date);
    }
}

TEST_CASE("TaskPositionService Test with GNSSandAccelPosition: noisy measurements")
{
    // Arrange
    RTC_HandleTypeDef hrtc;
    const uint32_t secondFraction = 1023;
    hrtc.Init.SynchPrediv = secondFraction;

    TimeUtils::DateTimeComponents dtc{
        .year = 2000,
        .month = 1,
        .day = 1,
        .hour = 0,
        .minute = 0,
        .second = 1,
        .millisecond = 0};
    auto duration = TimeUtils::to_epoch_duration(dtc);
    auto rtc = TimeUtils::to_rtc(duration, secondFraction);
    set_mocked_rtc_time(rtc.time);
    set_mocked_rtc_date(rtc.date);

    constexpr CyphalNodeID id = 11;
    LoopardAdapter loopard;
    loopard.memory_allocate = loopardMemoryAllocate;
    loopard.memory_free = loopardMemoryFree;
    Cyphal<LoopardAdapter> loopard_cyphal(&loopard);
    loopard_cyphal.setNodeID(id);
    std::tuple<Cyphal<LoopardAdapter>> adapters(loopard_cyphal);

    MockGNSS gnss;
    MockIMU imu;
    PositionTracker9D tracker;
    GNSSandAccelPosition<PositionTracker9D, MockGNSS, MockIMU> positionTracker(&hrtc, tracker, gnss, imu);
    auto task = TaskPositionService<GNSSandAccelPosition<PositionTracker9D, MockGNSS, MockIMU>, Cyphal<LoopardAdapter>>(positionTracker, 100, 1, 123, adapters);

    const float x0 = 100.0f;
    const float y0 = 200.0f;
    const float z0 = 300.0f;

    const float vx0 = 10.0f;
    const float vy0 = 20.0f;
    const float vz0 = 30.0f;

    const float ax0 = 0.1f;
    const float ay0 = 0.2f;
    const float az0 = 0.3f;

    const float dt = 0.1f; // 100 ms
    std::chrono::milliseconds dduration(100);

    for (int i = 0; i < 100; ++i)
    {
        float t = dt * static_cast<float>(i);

        float x = x0 + vx0 * t + 0.5f * ax0 * t * t;
        float y = y0 + vy0 * t + 0.5f * ay0 * t * t;
        float z = z0 + vz0 * t + 0.5f * az0 * t * t;

        float x_ = x + 0.1f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise
        float y_ = y + 0.1f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise
        float z_ = z + 0.1f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise

        float ax_ = ax0 + 0.1f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise
        float ay_ = ay0 + 0.1f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise
        float az_ = az0 + 0.1f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise

        // GNSS returns cm!
        gnss.setPosition(static_cast<int32_t>(x_ * 1e2f), static_cast<int32_t>(y_ * 1e2f), static_cast<int32_t>(z_ * 1e2f));
        imu.setAcceleration(ax_, ay_, az_);
        task.handleTaskImpl();
        REQUIRE(loopard.buffer.size() == 1);

        auto transfer = loopard.buffer.pop();
        CHECK(transfer.metadata.port_id == _4111spyglass_sat_model_PositionVelocity_0_1_PORT_ID_);
        CHECK(transfer.metadata.transfer_kind == CyphalTransferKindMessage);
        CHECK(transfer.metadata.remote_node_id == id);
        CHECK(transfer.payload_size == _4111spyglass_sat_model_PositionVelocity_0_1_SERIALIZATION_BUFFER_SIZE_BYTES_);

        _4111spyglass_sat_model_PositionVelocity_0_1 received_data;
        size_t deserialized_size = transfer.payload_size;
        int8_t deserialization_result = _4111spyglass_sat_model_PositionVelocity_0_1_deserialize_(&received_data, static_cast<const uint8_t *>(transfer.payload), &deserialized_size);
        REQUIRE(deserialization_result >= 0);

        float vx = vx0 + ax0 * t;
        float vy = vy0 + ay0 * t;
        float vz = vz0 + az0 * t;

        if (i > 50)
        {
            CHECK(received_data.timestamp.microsecond == duration.count() * 1000);
            CHECK(received_data.position_m[0] == doctest::Approx(x).epsilon(0.1f));
            CHECK(received_data.position_m[1] == doctest::Approx(y).epsilon(0.1f));
            CHECK(received_data.position_m[2] == doctest::Approx(z).epsilon(0.1f));
            CHECK(received_data.velocity_ms[0] == doctest::Approx(vx).epsilon(0.1f));
            CHECK(received_data.velocity_ms[1] == doctest::Approx(vy).epsilon(0.1f));
            CHECK(received_data.velocity_ms[2] == doctest::Approx(vz).epsilon(0.1f));
        }
        duration += dduration;
        auto rtc = TimeUtils::to_rtc(duration, secondFraction);
        set_mocked_rtc_time(rtc.time);
        set_mocked_rtc_date(rtc.date);
    }
}

TEST_CASE("TaskPositionService Test with SGP4andGNSSandPosition")
{
    // Arrange
    RTC_HandleTypeDef hrtc;
    const uint32_t secondFraction = 1023;
    hrtc.Init.SynchPrediv = secondFraction;

    TimeUtils::DateTimeComponents dtc{
        .year = 2000,
        .month = 1,
        .day = 1,
        .hour = 0,
        .minute = 0,
        .second = 1,
        .millisecond = 0};
    auto duration = TimeUtils::to_epoch_duration(dtc);
    auto rtc = TimeUtils::to_rtc(duration, secondFraction);
    set_mocked_rtc_time(rtc.time);
    set_mocked_rtc_date(rtc.date);

    constexpr CyphalNodeID id = 11;
    LoopardAdapter loopard;
    loopard.memory_allocate = loopardMemoryAllocate;
    loopard.memory_free = loopardMemoryFree;
    Cyphal<LoopardAdapter> loopard_cyphal(&loopard);
    loopard_cyphal.setNodeID(id);
    std::tuple<Cyphal<LoopardAdapter>> adapters(loopard_cyphal);

    MockSGP4 sgp4;  
    MockGNSS gnss;
    Sgp4PositionTracker tracker;
    SGP4andGNSSandPosition<Sgp4PositionTracker, MockSGP4, MockGNSS> positionTracker(&hrtc, tracker, sgp4, gnss);
    auto task = TaskPositionService<SGP4andGNSSandPosition<Sgp4PositionTracker, MockSGP4, MockGNSS>, Cyphal<LoopardAdapter>>(positionTracker, 100, 1, 123, adapters);

    const float x0 = 100.0f;
    const float y0 = 200.0f;
    const float z0 = 300.0f;

    const float vx0 = 10.0f;
    const float vy0 = 20.0f;
    const float vz0 = 30.0f;

    const float ax0 = 0.1f;
    const float ay0 = 0.2f;
    const float az0 = 0.3f;

    const float dt = 0.1f; // 100 ms
    std::chrono::milliseconds dduration(100);

    for (int i = 0; i < 1000; ++i)
    {
        float t = dt * static_cast<float>(i);

        float x = x0 + vx0 * t + 0.5f * ax0 * t * t;
        float y = y0 + vy0 * t + 0.5f * ay0 * t * t;
        float z = z0 + vz0 * t + 0.5f * az0 * t * t;

        float vx = vx0 + ax0 * t;
        float vy = vy0 + ay0 * t;
        float vz = vz0 + az0 * t;

        float xs = x + 10.f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise
        float ys = y + 10.f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise
        float zs = z + 10.f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise

        float xg = x + 10.f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise
        float yg = y + 10.f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise
        float zg = z + 10.f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise

        float vxs = vx + 10.f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise
        float vys = vy + 10.f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise
        float vzs = vz + 10.f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX); // Adding noise

        // GNSS returns cm!
        gnss.setPosition(static_cast<int32_t>(xg * 1e2f), static_cast<int32_t>(yg * 1e2f), static_cast<int32_t>(zg * 1e2f));
        sgp4.setPosition(xs, ys, zs);
        sgp4.setVelocity(vxs, vys, vzs);
        task.handleTaskImpl();
        REQUIRE(loopard.buffer.size() == 1);

        auto transfer = loopard.buffer.pop();
        CHECK(transfer.metadata.port_id == _4111spyglass_sat_model_PositionVelocity_0_1_PORT_ID_);
        CHECK(transfer.metadata.transfer_kind == CyphalTransferKindMessage);
        CHECK(transfer.metadata.remote_node_id == id);
        CHECK(transfer.payload_size == _4111spyglass_sat_model_PositionVelocity_0_1_SERIALIZATION_BUFFER_SIZE_BYTES_);

        _4111spyglass_sat_model_PositionVelocity_0_1 received_data;
        size_t deserialized_size = transfer.payload_size;
        int8_t deserialization_result = _4111spyglass_sat_model_PositionVelocity_0_1_deserialize_(&received_data, static_cast<const uint8_t *>(transfer.payload), &deserialized_size);
        REQUIRE(deserialization_result >= 0);

        if (i > 250)
        {
            CHECK(received_data.timestamp.microsecond == duration.count() * 1000);
            CHECK(received_data.position_m[0] == doctest::Approx(x).epsilon(0.1f));
            CHECK(received_data.position_m[1] == doctest::Approx(y).epsilon(0.1f));
            CHECK(received_data.position_m[2] == doctest::Approx(z).epsilon(0.1f));
            CHECK(received_data.velocity_ms[0] == doctest::Approx(vx).epsilon(10.f));
            CHECK(received_data.velocity_ms[1] == doctest::Approx(vy).epsilon(10.f));
            CHECK(received_data.velocity_ms[2] == doctest::Approx(vz).epsilon(10.f));
        }
        duration += dduration;
        auto rtc = TimeUtils::to_rtc(duration, secondFraction);
        set_mocked_rtc_time(rtc.time);
        set_mocked_rtc_date(rtc.date);
    }
}
