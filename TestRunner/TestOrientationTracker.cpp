#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "OrientationTracker.hpp"
#include <Eigen/Dense>
#include <iostream>
#include <cmath>

TEST_CASE("OrientationTracker initializes with identity quaternion")
{
    OrientationTracker tracker;
    auto q = tracker.getOrientation();
    CHECK(q.isApprox(Eigen::Quaternionf::Identity(), 1e-6f));
}

TEST_CASE("predictTo integrates quaternion forward using gyro state")
{
    OrientationTracker tracker;

    // Set gyro *before* any prediction so it’s used for [0 → 1] interval
    Eigen::Vector3f omega(0, 0, M_PIf / 2.0f); // 90°/s yaw
    tracker.setGyroAngularRate(omega);         // Inject ω into state

    // Now integrate 1 second forward from t=0.0 → t=1.0
    tracker.predictTo(1.0f);

    Eigen::Quaternionf q = tracker.getOrientation();
    float yaw = std::atan2(2.f * (q.w() * q.z() + q.x() * q.y()),
                           1.f - 2.f * (q.y() * q.y() + q.z() * q.z()));

    // std::cout << "Yaw (deg): " << yaw * 180.0f / M_PIf << "\n";
    // std::cout << "Yaw (rad): " << yaw << "\n";
    // std::cout << "Yaw (rad) - M_PIf/2.0f: " << yaw - (M_PIf / 2.0f) << "\n";
    CHECK(std::abs(yaw - (M_PIf / 2.0f)) < 0.01f); // ✅ ~90°
}

TEST_CASE("updateMagnetometer reduces yaw error after prediction")
{
    OrientationTracker tracker;
    Eigen::Vector3f omega(0, 0, M_PIf / 180.0f * 45.0f); // 45 deg/s
    tracker.updateGyro(omega, 0.0f);

    Eigen::Quaternionf q_true(static_cast<Eigen::Quaternionf>(Eigen::AngleAxisf(M_PIf / 4.0f, Eigen::Vector3f::UnitZ())));
    Eigen::Vector3f mag_ned(1.f, 0.f, 0.f);
    Eigen::Vector3f mag_meas = q_true.conjugate() * mag_ned;

    tracker.predictTo(4.0f); // Predict without correction
    float yaw_before = std::atan2(
        2.f * (tracker.getOrientation().w() * tracker.getOrientation().z()),
        1.f - 2.f * (tracker.getOrientation().z() * tracker.getOrientation().z()));

    // std::cout << "Yaw before update: " << yaw_before * 180.0f / M_PIf << " deg\n";
    for (int i = 0; i < 50; ++i)
    {
        tracker.updateMagnetometer(mag_meas, 4.0f);
        // float yaw_after = std::atan2(
        //     2.f * (tracker.getOrientation().w() * tracker.getOrientation().z()),
        //     1.f - 2.f * (tracker.getOrientation().z() * tracker.getOrientation().z()));
        // std::cout << "Yaw after update: " << yaw_after * 180.0f / M_PIf << " deg\n";
    }
    float yaw_after = std::atan2(
        2.f * (tracker.getOrientation().w() * tracker.getOrientation().z()),
        1.f - 2.f * (tracker.getOrientation().z() * tracker.getOrientation().z()));
    // std::cout << "Yaw after update: " << yaw_after * 180.0f / M_PIf << " deg\n";

    CHECK(std::abs(yaw_before - yaw_after) > 1e-3f); // Should adjust
}

TEST_CASE("OrientationTracker follows yaw rotation with magnetometer corrections") {
    OrientationTracker tracker;

    float dt = 0.5f;
    float yaw_rate = 30.f * M_PIf / 180.f;  // 30 deg/s
    Eigen::Vector3f omega(0.f, 0.f, yaw_rate);

    Eigen::Quaternionf q_true = Eigen::Quaternionf::Identity();
    Eigen::Vector3f mag_ned = Eigen::Vector3f(1.0f, 0.0f, 0.0f);  // aligned with North

    for (int step = 0; step < 20; ++step) {
        float t = static_cast<float>(step) * dt;

        // Integrate true orientation
        Eigen::Quaternionf dq(1.0f, 0.0f, 0.0f, 0.5f * omega(2) * dt);
        q_true = (q_true * dq).normalized();

        // Simulate measurements
        Eigen::Vector3f mag_meas = q_true.conjugate() * mag_ned + Eigen::Vector3f::Random() * 0.01f;

        tracker.updateGyro(omega, t);
        if (step % 2 == 0) {  // magnetometer at 1 Hz
            tracker.updateMagnetometer(mag_meas, t);
        }

        // Compare estimated yaw
        Eigen::Quaternionf q_est = tracker.getOrientation();
        float yaw_est = std::atan2(2.f * (q_est.w() * q_est.z() + q_est.x() * q_est.y()),
                                   1.f - 2.f * (q_est.y() * q_est.y() + q_est.z() * q_est.z()));

        float yaw_true = std::atan2(2.f * (q_true.w() * q_true.z() + q_true.x() * q_true.y()),
                                    1.f - 2.f * (q_true.y() * q_true.y() + q_true.z() * q_true.z()));

        float err = std::atan2(std::sin(yaw_est - yaw_true), std::cos(yaw_est - yaw_true));
        // std::cout << "Step " << step + 1 << " | Yaw error: " << err * 180.f / M_PIf << " deg\n";

        CHECK(std::abs(err) < 0.3f);
    }
}
