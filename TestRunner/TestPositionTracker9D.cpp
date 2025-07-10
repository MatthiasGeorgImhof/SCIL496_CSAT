
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "PositionTracker9D.hpp"
#include <Eigen/Dense>
#include <iostream>

TEST_CASE("PositionTracker9D handles asynchronous GPS and accel updates") {
    PositionTracker9D tracker;

    const Eigen::Vector3f true_accel(1.0f, 0.5f, -0.8f);
    float time = 0.0f;
    const float sim_duration = 10.0f;
    const float accel_dt = 0.1f;
    const float gps_dt = 1.0f;

    float next_gps_time = 0.0f;

    while (time <= sim_duration) {
        // Simulate noisy accel measurement
        Eigen::Vector3f accel_meas = true_accel + Eigen::Vector3f::Random() * 0.02f;
        tracker.updateWithAccel(accel_meas, time);

        // Simulate GPS at lower rate
        if (std::abs(time - next_gps_time) < 1e-4f) {
            Eigen::Vector3f true_pos = 0.5f * true_accel * time * time;
            Eigen::Vector3f gps_meas = true_pos + Eigen::Vector3f::Random() * 0.05f;
            tracker.updateWithGps(gps_meas, time);
            next_gps_time += gps_dt;
        }

        time += accel_dt;
    }

    auto est = tracker.getState();
    Eigen::Vector3f expected_pos = 0.5f * true_accel * sim_duration * sim_duration;
    Eigen::Vector3f expected_vel = true_accel * sim_duration;
    Eigen::Vector3f expected_acc = true_accel;

    // std::cout << "Estimated pos: " << est.segment<3>(0).transpose() << "\n";
    // std::cout << "Estimated vel: " << est.segment<3>(3).transpose() << "\n";
    // std::cout << "Estimated acc: " << est.segment<3>(6).transpose() << "\n";

    for (int i = 0; i < 3; ++i) {
        CHECK(est.segment<3>(0)(i) == doctest::Approx(expected_pos(i)).epsilon(0.15f));
        CHECK(est.segment<3>(3)(i) == doctest::Approx(expected_vel(i)).epsilon(0.1f));
        CHECK(est.segment<3>(6)(i) == doctest::Approx(expected_acc(i)).epsilon(0.05f));
    }
}
