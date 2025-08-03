#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "IMU.hpp"
#include "au.hpp"

#include <optional>

struct StubIMU {
    std::optional<std::array<au::QuantityF<au::MetersPerSecondSquaredInBodyFrame>, 3>> readAccelerometer() {
        return std::array{
            au::make_quantity<au::MetersPerSecondSquaredInBodyFrame>(1.0f),
            au::make_quantity<au::MetersPerSecondSquaredInBodyFrame>(0.0f),
            au::make_quantity<au::MetersPerSecondSquaredInBodyFrame>(0.0f)
        };
    }
};

struct StubOrientationProvider {
    void predict(std::array<float, 4>& q_body_to_ned, au::QuantityU64<au::Milli<au::Seconds>>& timestamp) {
        q_body_to_ned = {0.0f, 0.0f, 0.0f, 1.0f}; // Identity quaternion
        timestamp = au::make_quantity<au::Milli<au::Seconds>>(1000);
    }
};

struct StubPositionProvider {
    void predict(std::array<au::QuantityF<au::MetersInEcefFrame>, 3>& pos_ecef,
                 std::array<au::QuantityF<au::MetersPerSecondInEcefFrame>, 3>& velocity,
                 const au::QuantityU64<au::Milli<au::Seconds>>& /* timestamp */) {
        pos_ecef = {
            au::make_quantity<au::MetersInEcefFrame>(6378137.0f), // x
            au::make_quantity<au::MetersInEcefFrame>(0.0f),       // y
            au::make_quantity<au::MetersInEcefFrame>(0.0f)        // z
        };
        velocity = {
            au::make_quantity<au::MetersPerSecondInEcefFrame>(0.0f),
            au::make_quantity<au::MetersPerSecondInEcefFrame>(0.0f),
            au::make_quantity<au::MetersPerSecondInEcefFrame>(0.0f)
        };
    }
};

TEST_CASE("IMUWithReorientation: Identity rotation preserves acceleration direction") {
    StubIMU imu;
    StubOrientationProvider orientation;
    StubPositionProvider position;

    IMUWithReorientation<StubIMU, StubOrientationProvider, StubPositionProvider> imu_reoriented(imu, orientation, position);

    auto result = imu_reoriented.readAccelerometer();
    REQUIRE(result.has_value());

    auto accel_ecef = result.value();
    CHECK(doctest::Approx(accel_ecef[0].in(au::metersPerSecondSquaredInEcefFrame)) == 0.0f);  // X
    CHECK(doctest::Approx(accel_ecef[1].in(au::metersPerSecondSquaredInEcefFrame)) == 0.0f);  // Y
    CHECK(doctest::Approx(accel_ecef[2].in(au::metersPerSecondSquaredInEcefFrame)) == -1.0f); // Z
}

struct StubIMUNoData {
    std::optional<std::array<au::QuantityF<au::MetersPerSecondSquaredInBodyFrame>, 3>> readAccelerometer() {
        return std::nullopt;
    }
};

TEST_CASE("IMUWithReorientation: Returns nullopt when IMU data is missing") {
    StubIMUNoData imu;
    StubOrientationProvider orientation;
    StubPositionProvider position;

    IMUWithReorientation<StubIMUNoData, StubOrientationProvider, StubPositionProvider> imu_reoriented(imu, orientation, position);

    auto result = imu_reoriented.readAccelerometer();
    CHECK_FALSE(result.has_value());
}
