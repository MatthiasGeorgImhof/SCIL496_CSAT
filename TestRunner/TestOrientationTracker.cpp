#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "OrientationTracker.hpp"
#include <Eigen/Dense>
#include <iostream>
#include <cmath>

// TEST_CASE("GyrMagOrientationTracker initializes with identity quaternion")
// {
//     GyrMagOrientationTracker tracker;
//     auto q = tracker.getOrientation();
//     CHECK(q.isApprox(Eigen::Quaternionf::Identity(), 1e-6f));
// }

// TEST_CASE("predictTo integrates quaternion forward using gyro state")
// {
//     GyrMagOrientationTracker tracker;

//     // Set gyro *before* any prediction so it’s used for [0 → 1] interval
//     Eigen::Vector3f omega(0, 0, M_PIf / 2.0f); // 90°/s yaw
//     tracker.setGyroAngularRate(omega);         // Inject ω into state

//     // Now integrate 1 second forward from t=0.0 → t=1.0
//     tracker.predictTo(1.0f);

//     Eigen::Quaternionf q = tracker.getOrientation();
//     float yaw = std::atan2(2.f * (q.w() * q.z() + q.x() * q.y()),
//                            1.f - 2.f * (q.y() * q.y() + q.z() * q.z()));

//     // std::cout << "Yaw (deg): " << yaw * 180.0f / M_PIf << "\n";
//     // std::cout << "Yaw (rad): " << yaw << "\n";
//     // std::cout << "Yaw (rad) - M_PIf/2.0f: " << yaw - (M_PIf / 2.0f) << "\n";
//     CHECK(std::abs(yaw - (M_PIf / 2.0f)) < 0.01f); // ✅ ~90°
// }

// TEST_CASE("updateMagnetometer reduces yaw error after prediction")
// {
//     GyrMagOrientationTracker tracker;
//     Eigen::Vector3f omega(0, 0, M_PIf / 180.0f * 45.0f); // 45 deg/s
//     tracker.updateGyro(omega, 0.0f);

//     Eigen::Quaternionf q_true(static_cast<Eigen::Quaternionf>(Eigen::AngleAxisf(M_PIf / 4.0f, Eigen::Vector3f::UnitZ())));
//     Eigen::Vector3f mag_ned(1.f, 0.f, 0.f);
//     Eigen::Vector3f mag_meas = q_true.conjugate() * mag_ned;

//     tracker.predictTo(4.0f); // Predict without correction
//     float yaw_before = std::atan2(
//         2.f * (tracker.getOrientation().w() * tracker.getOrientation().z()),
//         1.f - 2.f * (tracker.getOrientation().z() * tracker.getOrientation().z()));

//     // std::cout << "Yaw before update: " << yaw_before * 180.0f / M_PIf << " deg\n";
//     for (int i = 0; i < 50; ++i)
//     {
//         tracker.updateMagnetometer(mag_meas, 4.0f);
//         // float yaw_after = std::atan2(
//         //     2.f * (tracker.getOrientation().w() * tracker.getOrientation().z()),
//         //     1.f - 2.f * (tracker.getOrientation().z() * tracker.getOrientation().z()));
//         // std::cout << "Yaw after update: " << yaw_after * 180.0f / M_PIf << " deg\n";
//     }
//     float yaw_after = std::atan2(
//         2.f * (tracker.getOrientation().w() * tracker.getOrientation().z()),
//         1.f - 2.f * (tracker.getOrientation().z() * tracker.getOrientation().z()));
//     // std::cout << "Yaw after update: " << yaw_after * 180.0f / M_PIf << " deg\n";

//     CHECK(std::abs(yaw_before - yaw_after) > 1e-3f); // Should adjust
// }

// TEST_CASE("GyrMagOrientationTracker follows yaw rotation with magnetometer corrections")
// {
//     GyrMagOrientationTracker tracker;

//     float dt = 0.5f;
//     float yaw_rate = 30.f * M_PIf / 180.f; // 30 deg/s
//     Eigen::Vector3f omega(0.f, 0.f, yaw_rate);

//     Eigen::Quaternionf q_true = Eigen::Quaternionf::Identity();
//     Eigen::Vector3f mag_ned = Eigen::Vector3f(1.0f, 0.0f, 0.0f); // aligned with North

//     for (int step = 0; step < 20; ++step)
//     {
//         float t = static_cast<float>(step) * dt;

//         // Integrate true orientation
//         Eigen::Quaternionf dq(1.0f, 0.0f, 0.0f, 0.5f * omega(2) * dt);
//         q_true = (q_true * dq).normalized();

//         // Simulate measurements
//         Eigen::Vector3f mag_meas = q_true.conjugate() * mag_ned + Eigen::Vector3f::Random() * 0.01f;

//         tracker.updateGyro(omega, t);
//         if (step % 2 == 0)
//         { // magnetometer at 1 Hz
//             tracker.updateMagnetometer(mag_meas, t);
//         }

//         // Compare estimated yaw
//         Eigen::Quaternionf q_est = tracker.getOrientation();
//         float yaw_est = std::atan2(2.f * (q_est.w() * q_est.z() + q_est.x() * q_est.y()),
//                                    1.f - 2.f * (q_est.y() * q_est.y() + q_est.z() * q_est.z()));

//         float yaw_true = std::atan2(2.f * (q_true.w() * q_true.z() + q_true.x() * q_true.y()),
//                                     1.f - 2.f * (q_true.y() * q_true.y() + q_true.z() * q_true.z()));

//         float err = std::atan2(std::sin(yaw_est - yaw_true), std::cos(yaw_est - yaw_true));
//         // std::cout << "Step " << step + 1 << " | Yaw error: " << err * 180.f / M_PIf << " deg\n";

//         CHECK(std::abs(err) < 0.3f);
//     }
// }

// #
// #
// #

// TEST_CASE("AccGyrMagOrientationTracker initializes with identity quaternion")
// {
//     AccGyrMagOrientationTracker tracker;
//     auto q = tracker.getStableOrientation();
//     CHECK(q.isApprox(Eigen::Quaternionf::Identity(), 1e-6f));
// }

// TEST_CASE("predictTo integrates quaternion forward using gyro state")
// {
//     AccGyrMagOrientationTracker tracker;

//     // Set gyro *before* any prediction so it’s used for [0 → 1] interval
//     Eigen::Vector3f omega(0, 0, M_PIf / 2.0f); // 90°/s yaw
//     tracker.setGyroAngularRate(omega);         // Inject ω into state

//     // Now integrate 1 second forward from t=0.0 → t=1.0
//     tracker.predictTo(1.0f);

//     Eigen::Quaternionf q = tracker.getStableOrientation();
//     float yaw = std::atan2(2.f * (q.w() * q.z() + q.x() * q.y()),
//                            1.f - 2.f * (q.y() * q.y() + q.z() * q.z()));

//     CHECK(std::abs(yaw - (M_PIf / 2.0f)) < 0.01f); // ✅ ~90°
// }

TEST_CASE("updateAccelerometerMagnetometer converges yaw orientation within envelope")
{
    AccGyrMagOrientationTracker tracker;

    Eigen::Quaternionf q_true(Eigen::AngleAxisf(M_PIf / 4.0f, Eigen::Vector3f::UnitZ()));
    Eigen::Vector3f accel_ned(0.0f, 0.0f, 9.81f);
    Eigen::Vector3f mag_ned(1.0f, 0.0f, 0.0f);
    Eigen::Vector3f accel_body = q_true.conjugate() * accel_ned;
    Eigen::Vector3f mag_body = q_true.conjugate() * mag_ned;

    Eigen::Vector3f omega(0.0f, 0.0f, 0.1f);
    tracker.setGyroAngularRate(omega);

    // Add these lines:
    std::cout << "Before predictTo:\n";
    std::cout << "  q_hat: " << tracker.getStableOrientation().coeffs().transpose() << "\n";
    tracker.predictTo(0.1f);
    std::cout << "After predictTo:\n";
    std::cout << "  q_hat: " << tracker.getStableOrientation().coeffs().transpose() << "\n";

    float yaw_true = M_PIf / 4.0f;
    std::vector<float> yaw_errors;

    for (int i = 0; i < 15; ++i)
    {
        std::cout << "accel_body: " << accel_body.transpose() << "\n";
        std::cout << "mag_body: " << mag_body.transpose() << "\n";
        
        tracker.updateAccelerometerMagnetometer(accel_body, mag_body, 0.1f);
        float yaw_est = tracker.getYawPitchRoll()(0);
        float err = std::atan2(std::sin(yaw_est - yaw_true), std::cos(yaw_est - yaw_true));
        yaw_errors.push_back(std::abs(err));

        std::cout << "Step " << i + 1
                  << " | Estimated Yaw: " << yaw_est * 180.0f / M_PIf
                  << " deg | Error: " << err * 180.0f / M_PIf << " deg\n";
    }

    // Check that error drops below threshold within first N steps
    bool converged = false;
    for (size_t i = 0; i < yaw_errors.size(); ++i)
    {
        if (yaw_errors[i] < 0.6f) // ~34.4°
        {
            converged = true;
            break;
        }
    }
    CHECK(converged);

    // Check that error stays within envelope after convergence
    for (size_t i = 10; i < yaw_errors.size(); ++i)
    {
        CHECK(yaw_errors[i] < 2.0f); // ~91.6°, allows oscillation but bounds it
    }
}

TEST_CASE("AccGyrMagOrientationTracker follows yaw rotation with accelerometer and magnetometer corrections")
{
    AccGyrMagOrientationTracker tracker;

    float dt = 0.5f;
    float yaw_rate = 30.f * M_PIf / 180.f; // 30 deg/s
    Eigen::Vector3f omega(0.f, 0.f, yaw_rate);

    Eigen::Quaternionf q_true = Eigen::Quaternionf::Identity();
    Eigen::Vector3f accel_ned(0.0f, 0.0f, 9.81f);
    Eigen::Vector3f mag_ned(1.0f, 0.0f, 0.0f); // aligned with North

    for (int step = 0; step < 15; ++step)
    {
        float t = static_cast<float>(step) * dt;

        // Integrate true orientation
        Eigen::Quaternionf dq(1.0f, 0.0f, 0.0f, 0.5f * omega(2) * dt);
        q_true = (q_true * dq).normalized();

        // Simulate measurements
        Eigen::Vector3f accel_meas = q_true.conjugate() * accel_ned + Eigen::Vector3f::Random() * 0.01f;
        Eigen::Vector3f mag_meas = q_true.conjugate() * mag_ned + Eigen::Vector3f::Random() * 0.01f;

        tracker.updateGyro(omega, t);
        tracker.updateAccelerometerMagnetometer(accel_meas, mag_meas, t);

        // Compare estimated yaw
        Eigen::Quaternionf q_est = tracker.getStableOrientation();
        float yaw_est = std::atan2(2.f * (q_est.w() * q_est.z() + q_est.x() * q_est.y()),
                                   1.f - 2.f * (q_est.y() * q_est.y() + q_est.z() * q_est.z()));

        float yaw_true = std::atan2(2.f * (q_true.w() * q_true.z() + q_true.x() * q_true.y()),
                                    1.f - 2.f * (q_true.y() * q_true.y() + q_true.z() * q_true.z()));

        float err = std::atan2(std::sin(yaw_est - yaw_true), std::cos(yaw_est - yaw_true));
        float err_deg = err * 180.f / M_PIf;
        if (err_deg > 180.f)
            err_deg -= 360.f;
        if (err_deg < -180.f)
            err_deg += 360.f;

        std::cout << "Step " << step + 1 << "\n";
        std::cout << "True Yaw (degrees): " << yaw_true * 180.0f / M_PIf << std::endl;
        std::cout << "Estimated Yaw (degrees): " << yaw_est * 180.0f / M_PIf << std::endl;
        std::cout << "Yaw Error (wrapped): " << err_deg << " deg\n";

        CHECK(std::abs(err) < 0.3f);
    }
}

TEST_CASE("updateAccelerometerMagnetometer converges yaw orientation within envelope - SIMPLIFIED")
{
    AccGyrMagOrientationTracker tracker;

    // Set a known initial yaw error (e.g., start at 0 yaw, target is M_PI/4)
    Eigen::Quaternionf q_initial = Eigen::Quaternionf::Identity(); // Start with yaw = 0
    tracker.setOrientation(q_initial);
    Eigen::Quaternionf q_true(Eigen::AngleAxisf(M_PIf / 4.0f, Eigen::Vector3f::UnitZ())); // Target yaw = M_PI/4

    Eigen::Vector3f accel_ned(0.0f, 0.0f, 9.81f);
    Eigen::Vector3f mag_ned(1.0f, 0.0f, 0.0f);
    Eigen::Vector3f accel_body = q_true.conjugate() * accel_ned;
    Eigen::Vector3f mag_body = q_true.conjugate() * mag_ned;

    //Zero the gyro
    Eigen::Vector3f omega(0.0f, 0.0f, 0.0f);
    tracker.setGyroAngularRate(omega);
    tracker.predictTo(0.1f);

    float yaw_true = M_PIf / 4.0f;
    float yaw_initial = 0.0f;
    // Single-step the correction
    tracker.updateAccelerometerMagnetometer(accel_body, mag_body, 0.1f);

    float yaw_est = tracker.getYawPitchRoll()(0);
    float err = std::atan2(std::sin(yaw_est - yaw_true), std::cos(yaw_est - yaw_true));

    std::cout << "Initial Yaw (degrees): " << yaw_initial * 180.0f / M_PIf << "\n";
    std::cout << "True Yaw (degrees): " << yaw_true * 180.0f / M_PIf << "\n";
    std::cout << "Estimated Yaw (degrees): " << yaw_est * 180.0f / M_PIf << "\n";
    std::cout << "Yaw Error (degrees): " << err * 180.0f / M_PIf << "\n";
    CHECK(std::abs(err) < 0.6f);
}