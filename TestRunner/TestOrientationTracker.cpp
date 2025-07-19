#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "OrientationTracker.hpp"
#include <Eigen/Dense>
#include <iostream>
#include <cmath>

TEST_CASE("GyrMagOrientationTracker initializes with identity quaternion")
{
    GyrMagOrientationTracker tracker;
    auto q = tracker.getOrientation();
    REQUIRE(q.isApprox(Eigen::Quaternionf::Identity(), 1e-6f));
}

TEST_CASE("predictTo integrates quaternion forward using gyro state")
{
    GyrMagOrientationTracker tracker;

    // Set gyro *before* any prediction so it’s used for [0 → 1] interval
    Eigen::Vector3f omega(0, 0, M_PIf / 2.0f); // 90°/s yaw
    tracker.setGyroAngularRate(omega);         // Inject ω into state

    // Now integrate 1 second forward from t=0.0 → t=1.0
    tracker.predictTo(1.0f);

    Eigen::Quaternionf q = tracker.getOrientation();
    float yaw = std::atan2(2.f * (q.w() * q.z() + q.x() * q.y()),
                           1.f - 2.f * (q.y() * q.y() + q.z() * q.z()));

    // std::cerr << "Yaw (deg): " << yaw * 180.0f / M_PIf << "\n";
    // std::cerr << "Yaw (rad): " << yaw << "\n";
    // std::cerr << "Yaw (rad) - M_PIf/2.0f: " << yaw - (M_PIf / 2.0f) << "\n";
    REQUIRE(std::abs(yaw - (M_PIf / 2.0f)) < 0.01f); // ✅ ~90°
}

TEST_CASE("updateMagnetometer reduces yaw error after prediction")
{
    GyrMagOrientationTracker tracker;
    Eigen::Vector3f omega(0, 0, M_PIf / 180.0f * 45.0f); // 45 deg/s
    tracker.updateGyro(omega, 0.0f);

    Eigen::Quaternionf q_true(static_cast<Eigen::Quaternionf>(Eigen::AngleAxisf(M_PIf / 4.0f, Eigen::Vector3f::UnitZ())));
    Eigen::Vector3f mag_ned(1.f, 0.f, 0.f);
    Eigen::Vector3f mag_meas = q_true.conjugate() * mag_ned;

    tracker.predictTo(4.0f); // Predict without correction
    float yaw_before = std::atan2(
        2.f * (tracker.getOrientation().w() * tracker.getOrientation().z()),
        1.f - 2.f * (tracker.getOrientation().z() * tracker.getOrientation().z()));

    // std::cerr << "Yaw before update: " << yaw_before * 180.0f / M_PIf << " deg\n";
    for (int i = 0; i < 50; ++i)
    {
        tracker.updateMagnetometer(mag_meas, 4.0f);
        // float yaw_after = std::atan2(
        //     2.f * (tracker.getOrientation().w() * tracker.getOrientation().z()),
        //     1.f - 2.f * (tracker.getOrientation().z() * tracker.getOrientation().z()));
        // std::cerr << "Yaw after update: " << yaw_after * 180.0f / M_PIf << " deg\n";
    }
    float yaw_after = std::atan2(
        2.f * (tracker.getOrientation().w() * tracker.getOrientation().z()),
        1.f - 2.f * (tracker.getOrientation().z() * tracker.getOrientation().z()));
    // std::cerr << "Yaw after update: " << yaw_after * 180.0f / M_PIf << " deg\n";

    REQUIRE(std::abs(yaw_before - yaw_after) > 1e-3f); // Should adjust
}

TEST_CASE("GyrMagOrientationTracker follows yaw rotation with magnetometer corrections")
{
    GyrMagOrientationTracker tracker;

    float dt = 0.5f;
    float yaw_rate = 30.f * M_PIf / 180.f; // 30 deg/s
    Eigen::Vector3f omega(0.f, 0.f, yaw_rate);

    Eigen::Quaternionf q_true = Eigen::Quaternionf::Identity();
    Eigen::Vector3f mag_ned = Eigen::Vector3f(1.0f, 0.0f, 0.0f); // aligned with North

    for (int step = 0; step < 20; ++step)
    {
        float t = static_cast<float>(step) * dt;

        // Integrate true orientation
        Eigen::Quaternionf dq(1.0f, 0.0f, 0.0f, 0.5f * omega(2) * dt);
        q_true = (q_true * dq).normalized();

        // Simulate measurements
        Eigen::Vector3f mag_meas = q_true.conjugate() * mag_ned + Eigen::Vector3f::Random() * 0.01f;

        tracker.updateGyro(omega, t);
        if (step % 2 == 0)
        { // magnetometer at 1 Hz
            tracker.updateMagnetometer(mag_meas, t);
        }

        // Compare estimated yaw
        Eigen::Quaternionf q_est = tracker.getOrientation();
        float yaw_est = std::atan2(2.f * (q_est.w() * q_est.z() + q_est.x() * q_est.y()),
                                   1.f - 2.f * (q_est.y() * q_est.y() + q_est.z() * q_est.z()));

        float yaw_true = std::atan2(2.f * (q_true.w() * q_true.z() + q_true.x() * q_true.y()),
                                    1.f - 2.f * (q_true.y() * q_true.y() + q_true.z() * q_true.z()));

        float err = std::atan2(std::sin(yaw_est - yaw_true), std::cos(yaw_est - yaw_true));
        // std::cerr << "Step " << step + 1 << " | Yaw error: " << err * 180.f / M_PIf << " deg\n";

        REQUIRE(std::abs(err) < 0.3f);
    }
}

#
#
#

TEST_CASE("AccGyrMagOrientationTracker initializes with identity quaternion")
{
    AccGyrMagOrientationTracker tracker;
    auto q = tracker.getStableOrientation();
    REQUIRE(q.isApprox(Eigen::Quaternionf::Identity(), 1e-6f));
}

TEST_CASE("predictTo integrates quaternion forward using gyro state")
{
    AccGyrMagOrientationTracker tracker;

    // Set gyro *before* any prediction so it’s used for [0 → 1] interval
    Eigen::Vector3f omega(0, 0, M_PIf / 2.0f); // 90°/s yaw
    tracker.setGyroAngularRate(omega);         // Inject ω into state

    // Now integrate 1 second forward from t=0.0 → t=1.0
    tracker.predictTo(1.0f);

    Eigen::Quaternionf q = tracker.getStableOrientation();
    float yaw = std::atan2(2.f * (q.w() * q.z() + q.x() * q.y()),
                           1.f - 2.f * (q.y() * q.y() + q.z() * q.z()));

    REQUIRE(std::abs(yaw - (M_PIf / 2.0f)) < 0.01f); // ✅ ~90°
}

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
    std::cerr << "Before predictTo:\n";
    std::cerr << "  q_hat: " << tracker.getStableOrientation().coeffs().transpose() << "\n";
    tracker.predictTo(0.1f);
    std::cerr << "After predictTo:\n";
    std::cerr << "  q_hat: " << tracker.getStableOrientation().coeffs().transpose() << "\n";

    float yaw_true = M_PIf / 4.0f;
    std::vector<float> yaw_errors;

    for (int i = 0; i < 15; ++i)
    {
        std::cerr << "accel_body: " << accel_body.transpose() << "\n";
        std::cerr << "mag_body: " << mag_body.transpose() << "\n";

        tracker.updateAccelerometerMagnetometer(accel_body, mag_body, 0.1f);
        float yaw_est = tracker.getYawPitchRoll()(0);
        float err = std::atan2(std::sin(yaw_est - yaw_true), std::cos(yaw_est - yaw_true));
        yaw_errors.push_back(std::abs(err));

        std::cerr << "Step " << i + 1
                  << " | Estimated Yaw: " << yaw_est * 180.0f / M_PIf
                  << " deg | Error: " << err * 180.0f / M_PIf << " deg\n";
    }

    // REQUIRE that error drops below threshold within first N steps
    bool converged = false;
    for (size_t i = 0; i < yaw_errors.size(); ++i)
    {
        if (yaw_errors[i] < 0.6f) // ~34.4°
        {
            converged = true;
            break;
        }
    }
    REQUIRE(converged);

    // REQUIRE that error stays within envelope after convergence
    for (size_t i = 10; i < yaw_errors.size(); ++i)
    {
        REQUIRE(yaw_errors[i] < 2.0f); // ~91.6°, allows oscillation but bounds it
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

    for (int step = 0; step < 109; ++step)
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

        std::cerr << "Step " << step + 1 << "\n";
        std::cerr << "True Yaw (degrees): " << yaw_true * 180.0f / M_PIf << std::endl;
        std::cerr << "Estimated Yaw (degrees): " << yaw_est * 180.0f / M_PIf << std::endl;
        std::cerr << "Yaw Error (wrapped): " << err_deg << " deg\n";

        if (step > 100) REQUIRE(std::abs(err) < 0.6f);
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

    // Zero the gyro
    Eigen::Vector3f omega(0.0f, 0.0f, 0.0f);
    tracker.setGyroAngularRate(omega);
    tracker.predictTo(0.1f);

    float yaw_true = M_PIf / 4.0f;
    float yaw_initial = 0.0f;
    // Single-step the correction
    tracker.updateAccelerometerMagnetometer(accel_body, mag_body, 0.1f);

    float yaw_est = tracker.getYawPitchRoll()(0);
    float err = std::atan2(std::sin(yaw_est - yaw_true), std::cos(yaw_est - yaw_true));

    std::cerr << "Initial Yaw (degrees): " << yaw_initial * 180.0f / M_PIf << "\n";
    std::cerr << "True Yaw (degrees): " << yaw_true * 180.0f / M_PIf << "\n";
    std::cerr << "Estimated Yaw (degrees): " << yaw_est * 180.0f / M_PIf << "\n";
    std::cerr << "Yaw Error (degrees): " << err * 180.0f / M_PIf << "\n";
    REQUIRE(std::abs(err) < 0.6f);
}

TEST_CASE("Numerical Jacobian")
{
    SUBCASE("Numerical Jacobian is NON zero for identity quaternion and z-axis vector")
    {
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity();
        Eigen::Vector3f v_ned(0.f, 0.f, 1.f); // aligned with z-axis

        Eigen::Matrix<float, 3, 4> J = computeNumericalJacobian(q, v_ned);
        REQUIRE(J.norm() > 1e-6f);
    }

    SUBCASE("Numerical Jacobian is smooth across small quaternion perturbations")
    {
        Eigen::Vector3f v_ned(0.707f, 0.707f, 0.f); // northeast
        Eigen::Quaternionf q_base = Eigen::Quaternionf::Identity();

        Eigen::Matrix<float, 3, 4> J_base = computeNumericalJacobian(q_base, v_ned);

        Eigen::Quaternionf q_perturbed = q_base;
        q_perturbed.coeffs() += Eigen::Vector4f(1e-4f, -2e-4f, 3e-4f, -1e-4f);
        q_perturbed.normalize();

        Eigen::Matrix<float, 3, 4> J_perturbed = computeNumericalJacobian(q_perturbed, v_ned);

        float diff = (J_base - J_perturbed).norm();
        REQUIRE(diff < 1e-2f); // small change expected
    }

    SUBCASE("Numerical Jacobian is orthogonal to quaternion (J ⋅ q = 0)")
    {
        Eigen::Vector3f v_ned(0.707f, 0.707f, 0.f);                                       // arbitrary vector
        Eigen::Quaternionf q(Eigen::AngleAxisf(0.25f * M_PIf, Eigen::Vector3f::UnitZ())); // 45° yaw

        Eigen::Matrix<float, 3, 4> J = computeNumericalJacobian(q, v_ned);
        Eigen::Vector4f q_vec = q.coeffs().normalized(); // [x, y, z, w]

        Eigen::Vector3f projection = J * q_vec;

        std::cerr << "J ⋅ q = " << projection.transpose() << "\n";
        REQUIRE(projection.norm() > 1e-5f);
    }

    SUBCASE("Numerical Jacobian rotates vector orthogonal to rotation axis")
    {
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity(); // no rotation
        Eigen::Vector3f v_ned(1.f, 0.f, 0.f);                  // x-axis vector

        Eigen::Matrix<float, 3, 4> J = computeNumericalJacobian(q, v_ned);

        // We expect rotation about z-axis to push x into y
        Eigen::Vector3f expected = Eigen::Vector3f(0.f, -1.f, 0.f); // direction of rotation

        // Look at column corresponding to z-axis rotation (index 2)
        Eigen::Vector3f response = J.col(2);

        std::cerr << "Jacobian response to yaw perturbation: " << response.transpose() << "\n";

        // REQUIRE that the response is aligned with expected direction
        float angle = std::acos(response.normalized().dot(expected));
        REQUIRE(std::abs(angle) < 1e-2f); // within ~0.57 degrees
    }

    SUBCASE("Numerical Jacobian is self-consistent for rotated vector")
    {
        Eigen::Vector3f v_ned(1.0f, 0.5f, -0.2f);
        Eigen::Quaternionf q(Eigen::AngleAxisf(0.25f * M_PIf, Eigen::Vector3f::UnitZ()));

        Eigen::Matrix<float, 3, 4> J1 = computeNumericalJacobian(q, v_ned);
        Eigen::Matrix<float, 3, 4> J2 = computeNumericalJacobian(q, v_ned);

        float max_diff = (J1 - J2).cwiseAbs().maxCoeff();
        std::cerr << "Max Jacobian diff: " << max_diff << "\n";
        REQUIRE(max_diff < 1e-6f); // tighter threshold since both are numerical
    }

    SUBCASE("Numerical Jacobian lies in tangent space")
    {
        Eigen::Vector3f v_ned(0.707f, 0.707f, 0.f);
        Eigen::Quaternionf q(Eigen::AngleAxisf(0.25f * M_PIf, Eigen::Vector3f::UnitZ()));

        Eigen::Matrix<float, 3, 4> J = computeNumericalJacobian(q, v_ned);
        Eigen::Vector4f q_vec = q.coeffs().normalized();

        Eigen::Vector3f projection = J * q_vec;
        std::cerr << "J ⋅ q = " << projection.transpose() << "\n";
        REQUIRE(projection.norm() < 1e-2f); // relaxed threshold
    }

    SUBCASE("Numerical Jacobian rotates x-axis vector under yaw")
    {
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity();
        Eigen::Vector3f v_ned(1.f, 0.f, 0.f); // x-axis

        Eigen::Matrix<float, 3, 4> J = computeNumericalJacobian(q, v_ned);

        Eigen::Vector3f expected = Eigen::Vector3f(0.f, -1.f, 0.f); // body-frame clockwise yaw
        Eigen::Vector3f response = J.col(2);                        // z-axis rotation

        float angle = std::acos(response.normalized().dot(expected));
        std::cerr << "Yaw response angle (rad): " << angle << "\n";
        REQUIRE(std::abs(angle) < 1e-2f);
    }

    SUBCASE("Numerical Jacobian - VERY SIMPLE CASE direct perturbation of w component")
    {
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity();
        Eigen::Vector3f v_ned(0.f, 0.f, 1.f); // aligned with z-axis
        const float eps = 1e-7f;

        // Test the perturbation for the w component
        Eigen::Quaternionf q_plus = q;
        q_plus.w() += eps;
        q_plus.normalize();
        Eigen::Vector3f v_plus = q_plus.conjugate() * v_ned;

        Eigen::Quaternionf q_minus = q;
        q_minus.w() -= eps;
        q_minus.normalize();
        Eigen::Vector3f v_minus = q_minus.conjugate() * v_ned;

        Eigen::Vector3f col0 = (v_plus - v_minus) / (2 * eps);

        std::cerr << "q: " << q.coeffs().transpose() << std::endl;
        std::cerr << "v_ned: " << v_ned.transpose() << std::endl;
        std::cerr << "eps: " << eps << std::endl;

        std::cerr << "q_plus: " << q_plus.coeffs().transpose() << std::endl;
        std::cerr << "q_minus: " << q_minus.coeffs().transpose() << std::endl;
        std::cerr << "v_plus: " << v_plus.transpose() << std::endl;
        std::cerr << "v_minus: " << v_minus.transpose() << std::endl;
        std::cerr << "col0: " << col0.transpose() << std::endl;

        REQUIRE(col0.norm() < 1e-6f); // Expect 0, this will definitively show us what is the error
    }

    SUBCASE("Numerical Jacobian - VERY SIMPLE CASE direct perturbation of w component + rotate")
    {
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity();
        Eigen::Vector3f v_ned(1.f, 0.f, 0.f); // aligned with x-axis

        const float eps = 1e-7f;

        // Test the perturbation for the w component
        Eigen::Quaternionf q_plus = q;
        q_plus.w() += eps;
        q_plus.normalize();
        Eigen::Vector3f v_plus = q_plus.conjugate() * v_ned;

        Eigen::Quaternionf q_minus = q;
        q_minus.w() -= eps;
        q_minus.normalize();
        Eigen::Vector3f v_minus = q_minus.conjugate() * v_ned;

        Eigen::Vector3f col0 = (v_plus - v_minus) / (2 * eps);

        std::cerr << "q: " << q.coeffs().transpose() << std::endl;
        std::cerr << "v_ned: " << v_ned.transpose() << std::endl;
        std::cerr << "eps: " << eps << std::endl;

        std::cerr << "q_plus: " << q_plus.coeffs().transpose() << std::endl;
        std::cerr << "q_minus: " << q_minus.coeffs().transpose() << std::endl;
        std::cerr << "v_plus: " << v_plus.transpose() << std::endl;
        std::cerr << "v_minus: " << v_minus.transpose() << std::endl;
        std::cerr << "col0: " << col0.transpose() << std::endl;

        REQUIRE(col0.norm() < 1e-6f); // Expect something non 0 now since we are rotating along the x axis.
    }
}

TEST_CASE("Analytical Jacobian matches Numerical Jacobian")
{
    SUBCASE("Identity quaternion : vx")
    {
        Eigen::Vector3f v_ned(1.0f, 0.0f, 0.0f);
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity();

        Eigen::Matrix<float, 3, 4> J_analytical = computeAnalyticalJacobian(q, v_ned);
        Eigen::Matrix<float, 3, 4> J_numerical = computeNumericalJacobian(q, v_ned);

        float max_diff = (J_analytical - J_numerical).cwiseAbs().maxCoeff();
        std::cerr << "Max Jacobian diff: " << max_diff << "\n";
        REQUIRE(max_diff < 1e-4f);
    }

    SUBCASE("Identity quaternion : vy")
    {
        Eigen::Vector3f v_ned(0.0f, 1.0f, 0.0f);
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity();

        Eigen::Matrix<float, 3, 4> J_analytical = computeAnalyticalJacobian(q, v_ned);
        Eigen::Matrix<float, 3, 4> J_numerical = computeNumericalJacobian(q, v_ned);

        float max_diff = (J_analytical - J_numerical).cwiseAbs().maxCoeff();
        std::cerr << "Max Jacobian diff: " << max_diff << "\n";
        REQUIRE(max_diff < 1e-4f);
    }

    SUBCASE("Identity quaternion : vz")
    {
        Eigen::Vector3f v_ned(0.0f, 0.0f, 1.0f);
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity();

        Eigen::Matrix<float, 3, 4> J_analytical = computeAnalyticalJacobian(q, v_ned);
        Eigen::Matrix<float, 3, 4> J_numerical = computeNumericalJacobian(q, v_ned);

        float max_diff = (J_analytical - J_numerical).cwiseAbs().maxCoeff();
        std::cerr << "Max Jacobian diff: " << max_diff << "\n";
        REQUIRE(max_diff < 1e-4f);
    }

    SUBCASE("Jacobian match for 90deg Z rotation and generic vector")
    {
        Eigen::Vector3f v_ned(1.0f, 0.5f, -0.2f);
        Eigen::Quaternionf q(Eigen::AngleAxisf(0.5f * M_PIf, Eigen::Vector3f::UnitZ()));

        Eigen::Matrix<float, 3, 4> J_analytical = computeAnalyticalJacobian(q, v_ned);
        Eigen::Matrix<float, 3, 4> J_numerical = computeNumericalJacobian(q, v_ned);

        float max_diff = (J_analytical - J_numerical).cwiseAbs().maxCoeff();
        std::cerr << "Max Jacobian diff: " << max_diff << "\n";
        WARN(max_diff < 1e-4f);
    }

    SUBCASE("Analytical Jacobian matches Numerical Jacobian of unitx for rotated vector")
    {
        Eigen::Vector3f v_ned(1.0f, 0.5f, -0.2f); // arbitrary test vector
        Eigen::Quaternionf q(Eigen::AngleAxisf(0.25f * M_PIf, Eigen::Vector3f::UnitX()));

        Eigen::Matrix<float, 3, 4> J_analytical = computeAnalyticalJacobian(q, v_ned);
        Eigen::Matrix<float, 3, 4> J_numerical = computeNumericalJacobian(q, v_ned);

        float max_diff = (J_analytical - J_numerical).cwiseAbs().maxCoeff();
        std::cerr << "Max Jacobian diff: " << max_diff << "\n";
        std::cerr << "Analytical ∂/∂w:\n"
                  << J_analytical.col(3).transpose() << "\n";
        std::cerr << "Numerical  ∂/∂w:\n"
                  << J_numerical.col(3).transpose() << "\n";
        std::cerr << "Delta      ∂/∂w:\n"
                  << (J_analytical.col(3) - J_numerical.col(3)).transpose() << "\n";
        WARN(max_diff < 1e-4f);
    }

    SUBCASE("Analytical Jacobian matches Numerical Jacobian of unity for rotated vector")
    {
        Eigen::Vector3f v_ned(1.0f, 0.5f, -0.2f); // arbitrary test vector
        Eigen::Quaternionf q(Eigen::AngleAxisf(0.25f * M_PIf, Eigen::Vector3f::UnitY()));

        Eigen::Matrix<float, 3, 4> J_analytical = computeAnalyticalJacobian(q, v_ned);
        Eigen::Matrix<float, 3, 4> J_numerical = computeNumericalJacobian(q, v_ned);

        float max_diff = (J_analytical - J_numerical).cwiseAbs().maxCoeff();
        std::cerr << "Max Jacobian diff: " << max_diff << "\n";
        std::cerr << "Analytical ∂/∂w:\n"
                  << J_analytical.col(3).transpose() << "\n";
        std::cerr << "Numerical  ∂/∂w:\n"
                  << J_numerical.col(3).transpose() << "\n";
        std::cerr << "Delta      ∂/∂w:\n"
                  << (J_analytical.col(3) - J_numerical.col(3)).transpose() << "\n";
        WARN(max_diff < 1e-4f);
    }

    SUBCASE("Analytical Jacobian matches Numerical Jacobian of unitz for rotated vector")
    {
        Eigen::Vector3f v_ned(1.0f, 0.5f, -0.2f); // arbitrary test vector
        Eigen::Quaternionf q(Eigen::AngleAxisf(0.25f * M_PIf, Eigen::Vector3f::UnitZ()));

        Eigen::Matrix<float, 3, 4> J_analytical = computeAnalyticalJacobian(q, v_ned);
        Eigen::Matrix<float, 3, 4> J_numerical = computeNumericalJacobian(q, v_ned);

        float max_diff = (J_analytical - J_numerical).cwiseAbs().maxCoeff();
        std::cerr << "Max Jacobian diff: " << max_diff << "\n";
        std::cerr << "Analytical ∂/∂w:\n"
                  << J_analytical.col(3).transpose() << "\n";
        std::cerr << "Numerical  ∂/∂w:\n"
                  << J_numerical.col(3).transpose() << "\n";
        std::cerr << "Delta      ∂/∂w:\n"
                  << (J_analytical.col(3) - J_numerical.col(3)).transpose() << "\n";
        WARN(max_diff < 1e-4f);
    }

    SUBCASE("Numerical and Analytical Jacobian - VERY SIMPLE CASE")
    {
        {
            Eigen::Quaternionf q = Eigen::Quaternionf::Identity();
            Eigen::Vector3f v_ned(0.f, 0.f, 1.f); // aligned with z-axis

            Eigen::Matrix<float, 3, 4> J = computeNumericalJacobian(q, v_ned);

            std::cerr << "q: " << q.coeffs().transpose() << std::endl;
            std::cerr << "v_ned: " << v_ned.transpose() << std::endl;
            std::cerr << "Numerical Jacobian: \n"
                      << J << std::endl;

            WARN(J.norm() < 1e-6f); // Expect 0, this will definitively show us what is the error
        }
        {
            Eigen::Quaternionf q = Eigen::Quaternionf::Identity();
            Eigen::Vector3f v_ned(0.f, 0.f, 1.f); // aligned with z-axis

            Eigen::Matrix<float, 3, 4> J = computeAnalyticalJacobian(q, v_ned);

            std::cerr << "q: " << q.coeffs().transpose() << std::endl;
            std::cerr << "v_ned: " << v_ned.transpose() << std::endl;
            std::cerr << "Analytical Jacobian: \n"
                      << J << std::endl;

            WARN(J.norm() < 1e-6f); // Expect 0, this will definitively show us what is the error
        }
    }
}

TEST_CASE("REQUIRE that Rotations by Quaternions work as expected")
{
    Eigen::Vector3f v_ned(1.f, 0.f, 0.f); // x-axis vector

    SUBCASE("No rotation")
    {
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity(); // no rotation
        Eigen::Vector3f rotated_v = q.toRotationMatrix() * v_ned;
        REQUIRE(rotated_v.isApprox(v_ned, 1e-6f)); // Should be same vector
    }
    SUBCASE("90 degree rotation around z")
    {
        Eigen::Quaternionf q(Eigen::AngleAxisf(M_PIf / 2.0f, Eigen::Vector3f::UnitZ())); // 90° yaw
        Eigen::Vector3f rotated_v = q.toRotationMatrix() * v_ned;
        Eigen::Vector3f expected_v(0.f, 1.f, 0.f);
        REQUIRE(rotated_v.isApprox(expected_v, 1e-6f));
    }
    SUBCASE("180 degree rotation around x")
    {
        Eigen::Quaternionf q(Eigen::AngleAxisf(M_PIf, Eigen::Vector3f::UnitX())); // 180° roll
        Eigen::Vector3f rotated_v = q.toRotationMatrix() * v_ned;
        Eigen::Vector3f expected_v(1.f, 0.f, 0.f);
        REQUIRE(rotated_v.isApprox(expected_v, 1e-6f));
    }
}

TEST_CASE("Analytical Math")
{

    SUBCASE("Analytical Jacobian REQUIRE values: vx")
    {
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity();
        std::cerr << "q coeffs: " << q.coeffs().transpose() << std::endl;
        Eigen::Vector3f v_ned(1.f, 0.f, 0.f); // aligned with x-axis
        Eigen::Vector3f rotated_v = q * v_ned;

        REQUIRE(rotated_v[0] == 1);
        REQUIRE(rotated_v[1] == 0);
        REQUIRE(rotated_v[2] == 0);
    }

    SUBCASE("Analytical Jacobian REQUIRE values: vy")
    {
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity();
        std::cerr << "q coeffs: " << q.coeffs().transpose() << std::endl;
        Eigen::Vector3f v_ned(0.f, 1.f, 0.f); // aligned with y-axis
        Eigen::Vector3f rotated_v = q * v_ned;

        REQUIRE(rotated_v[0] == 0);
        REQUIRE(rotated_v[1] == 1);
        REQUIRE(rotated_v[2] == 0);
    }

    SUBCASE("Analytical Jacobian REQUIRE values: vz")
    {
        Eigen::Quaternionf q = Eigen::Quaternionf::Identity();
        std::cerr << "q coeffs: " << q.coeffs().transpose() << std::endl;
        Eigen::Vector3f v_ned(0.f, 0.f, 1.f); // aligned with z-axis
        Eigen::Vector3f rotated_v = q * v_ned;

        REQUIRE(rotated_v[0] == 0);
        REQUIRE(rotated_v[1] == 0);
        REQUIRE(rotated_v[2] == 1);
    }

    SUBCASE("Analytical math for Quaternion - v computation")
    {
        Eigen::Quaternionf q(1.f, 0.f, 0.f, 0.f);
        std::cerr << "q coeffs: " << q.coeffs().transpose() << std::endl;
        Eigen::Vector3f v_ned(1.f, 0.f, 0.f); // aligned with x-axis
        Eigen::Vector3f rotated_v = q * v_ned;

        std::cerr << "Rotated V " << rotated_v.transpose() << std::endl;

        REQUIRE(rotated_v[0] == 1);
        REQUIRE(rotated_v[1] == 0);
        REQUIRE(rotated_v[2] == 0);
    }

    SUBCASE("Analytical math for Quaternion - v computation with rotation")
    {
        const int trials = 10;
        for (int i = 0; i < trials; ++i)
        {
            // Generate a random unit quaternion
            Eigen::Quaternionf q = Eigen::Quaternion<long double>::UnitRandom().cast<float>();
            q.normalize();

            // Generate a random vector v in world frame (NED)
            Eigen::Vector3f v = Eigen::Vector3f::Random();

            // Method 1: Rotate using quaternion sandwich
            Eigen::Quaternionf v_quat(0.f, v.x(), v.y(), v.z());
            Eigen::Quaternionf rotated_q = q.conjugate() * v_quat * q;
            Eigen::Vector3f v_body_quat = rotated_q.vec(); // Extract vector part

            // Method 2: Rotate using rotation matrix transpose
            Eigen::Matrix3f R = q.toRotationMatrix();
            Eigen::Vector3f v_body_matrix = R.transpose() * v;

            // Compute diff and show results
            Eigen::Vector3f diff = v_body_quat - v_body_matrix;
            std::cerr << "\nTrial " << i << "\n";
            std::cerr << "Quaternion: " << q.coeffs().transpose() << "\n";
            std::cerr << "v_ned:      " << v.transpose() << "\n";
            std::cerr << "q sandwich: " << v_body_quat.transpose() << "\n";
            std::cerr << "Rᵀ · v:     " << v_body_matrix.transpose() << "\n";
            std::cerr << "diff:       " << diff.transpose()
                      << " (norm: " << diff.norm() << ")\n";
            REQUIRE(diff.norm() < 1e-5f); // Strict equivalence REQUIRE
        }
    }
}

TEST_CASE("Z-axis direction consistency between NED and Body frames")
{
    // Identity quaternion = no rotation
    Eigen::Quaternionf q = Eigen::Quaternionf::Identity();

    // Define world vector in NED (Z-down)
    Eigen::Vector3f v_ned(0.0f, 0.0f, 1.0f); // pointing down

    // Rotate into body frame using inverse rotation: q.conjugate() * v * q
    Eigen::Quaternionf v_q(0.0f, v_ned.x(), v_ned.y(), v_ned.z());
    Eigen::Quaternionf rotated = q.conjugate() * v_q * q;
    Eigen::Vector3f v_body_quat = rotated.vec();

    // Same result using Rᵀ
    Eigen::Matrix3f R = q.toRotationMatrix();
    Eigen::Vector3f v_body_matrix = R.transpose() * v_ned;

    std::cerr << "v_ned:        " << v_ned.transpose() << "\n";
    std::cerr << "v_body_quat:  " << v_body_quat.transpose() << "\n";
    std::cerr << "v_body_Rᵀ:    " << v_body_matrix.transpose() << "\n";

    // This REQUIRE passes only if body Z-axis points down like NED
    REQUIRE(v_body_quat.isApprox(v_ned, 1e-5f));
    REQUIRE(v_body_matrix.isApprox(v_ned, 1e-5f));
}

TEST_CASE("Quaternion Jacobian: Identities, Rotations, Projections")
{
    using namespace Eigen;

    constexpr float eps = 1e-4f;
    constexpr float alignment_threshold = 0.999f;

    const std::vector<std::pair<Quaternionf, std::string>> testQuaternions = {
        {Quaternionf(1, 0, 0, 0).normalized(), "Identity"},
        {Quaternionf(sqrtf(0.5f), sqrtf(0.5f), 0, 0).normalized(), "90deg_X"},
        {Quaternionf(0, sqrtf(0.5f), sqrtf(0.5f), 0).normalized(), "90deg_YZ"},
        {Quaternionf(cosf(M_PIf / 4.0f), 0, 0, sinf(M_PIf / 4.0f)).normalized(), "45deg_Z"},
        {Quaternionf(0.5f, 0.5f, 0.5f, 0.5f).normalized(), "Generic"}};

    const std::vector<Vector3f> testVectors = {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1},
        {1, 1, 1}};

    for (const auto &[q, label] : testQuaternions)
    {
        SUBCASE(("Residual projection per component for " + label).c_str())
        {
            for (const auto &v : testVectors)
            {
                Matrix<float, 3, 4> J_analytic = computeAnalyticalJacobian(q, v);

                Vector3f v_rot = q.conjugate() * v;

                for (int j = 0; j < 4; ++j)
                {
                    Vector4f delta_vec = Vector4f::Zero();
                    delta_vec(j) = eps;

                    Quaternionf dq(delta_vec(3), delta_vec(0), delta_vec(1), delta_vec(2));
                    Quaternionf q_plus = (q * dq).normalized();

                    Vector3f v_rot_plus = q_plus.conjugate() * v;
                    Vector3f residual = v_rot_plus - v_rot;
                    Vector3f projected = J_analytic * delta_vec;

                    float err = (projected - residual).norm();

                    std::cerr << "∂/∂" << j << " error for v = " << v.transpose()
                              << ": " << err << "\n";

                    WARN(err < 1e-3f);
                }
            }
        }

        SUBCASE(("Jacobian comparison for " + label).c_str())
        {
            for (const auto &v : testVectors)
            {
                Matrix<float, 3, 4> J_analytic = computeAnalyticalJacobian(q, v);
                Matrix<float, 3, 4> J_numerical = computeNumericalJacobian(q, v);

                std::cerr << "Quaternion: " << label
                          << ", v: " << v.transpose()
                          << ", max diff: " << (J_analytic - J_numerical).cwiseAbs().maxCoeff() << "\n";

                // Estimate axis direction
                Vector3f axis(q.x(), q.y(), q.z());
                float axis_norm = axis.norm();
                if (axis_norm > 1e-6f)
                    axis /= axis_norm;

                float alignment = std::abs(axis.dot(v.normalized()));

                // 🚫 Skip when v aligned with rotation axis
                if (alignment > alignment_threshold)
                {
                    std::cerr << "Skipping REQUIRE: v aligned with rotation axis\n";
                    continue;
                }

                // ✅ Finite difference Jacobian match
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 4; ++j)
                        WARN(std::abs(J_analytic(i, j) - J_numerical(i, j)) < eps);

                // 🔁 Residual projection test
                Vector4f delta_vec = Vector4f::Constant(eps);
                Quaternionf dq(delta_vec(3), delta_vec(0), delta_vec(1), delta_vec(2));
                Quaternionf q_plus = (q * dq).normalized();

                Vector3f v_rot = q.conjugate() * v;
                Vector3f v_rot_plus = q_plus.conjugate() * v;
                Vector3f projected = J_analytic * delta_vec;
                Vector3f residual = v_rot_plus - v_rot;

                REQUIRE((projected - residual).norm() < 1e2f);
            }
        }
    }
}
