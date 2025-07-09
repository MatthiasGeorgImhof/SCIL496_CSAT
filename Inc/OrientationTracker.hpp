#pragma once
#include <Eigen/Dense>
#include <functional>
#include "Kalman.hpp"

class OrientationTracker
{
public:
    static constexpr int StateSize = 7;       // [qx, qy, qz, qw, wx, wy, wz]
    static constexpr int MeasurementSize = 3; // magnetometer: 3D vector

    using StateVector = Eigen::Matrix<float, StateSize, 1>;
    using Measurement = Eigen::Vector3f;

    OrientationTracker()
        : ekf(
              Eigen::Matrix<float, StateSize, StateSize>::Identity() * 1e-5f,
              Eigen::Matrix3f::Identity() * 0.01f,
              Eigen::Matrix<float, StateSize, StateSize>::Identity() * 1e-5f,
              []
              {
                  StateVector x;
                  x << 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f;
                  return x;
              }()),
          last_timestamp_sec(0.0f)
    {
    }

void predictTo(float new_timestamp_sec) {
    float dt = new_timestamp_sec - last_timestamp_sec;
    if (dt <= 0.f) return;

    auto& x = ekf.stateVector;
    Eigen::Quaternionf q(x(3), x(0), x(1), x(2));
    Eigen::Vector3f omega = x.segment<3>(4);

    float angle = omega.norm() * dt;
    Eigen::Quaternionf delta_q = Eigen::Quaternionf::Identity();
    if (angle > 1e-6f) {
        Eigen::Vector3f axis = omega.normalized();
        delta_q = Eigen::AngleAxisf(angle, axis);
    }

    q = (q * delta_q).normalized();

    x(0) = q.x(); x(1) = q.y(); x(2) = q.z(); x(3) = q.w();
    ekf.stateCovarianceMatrix += ekf.processNoiseCovarianceMatrix;
    last_timestamp_sec = new_timestamp_sec;
}

    void updateGyro(const Eigen::Vector3f &omega, float timestamp_sec)
    {
        predictTo(timestamp_sec);
        ekf.stateVector.segment<3>(4) = omega;
    }

    void updateMagnetometer(const Eigen::Vector3f &mag_meas_body, float timestamp_sec)
    {
        predictTo(timestamp_sec);

        Eigen::Vector3f mag_ned(0.3f, 0.5f, 0.8f); // Fixed reference in NED
        mag_ned.normalize();

        auto h = [&](const StateVector &x)
        {
            Eigen::Quaternionf q(x(3), x(0), x(1), x(2));
            return q.conjugate() * mag_ned;
        };

        auto &x = ekf.stateVector;
        Eigen::Quaternionf q_hat(x(3), x(0), x(1), x(2));
        float qw = q_hat.w(), qx = q_hat.x(), qy = q_hat.y(), qz = q_hat.z();
        float mx = mag_ned(0), my = mag_ned(1), mz = mag_ned(2);

        Eigen::Matrix<float, MeasurementSize, StateSize> H_jac = Eigen::Matrix<float, MeasurementSize, StateSize>::Zero();

        H_jac(0, 0) = 2 * (my * qz - mz * qy);
        H_jac(0, 1) = 2 * (my * qy + mz * qz);
        H_jac(0, 2) = 2 * (my * qw - mx * qz);
        H_jac(0, 3) = -2 * (my * qx + mz * qy);
        H_jac(1, 0) = 2 * (mz * qx - mx * qz);
        H_jac(1, 1) = 2 * (mx * qx + mz * qw);
        H_jac(1, 2) = 2 * (mx * qz - my * qw);
        H_jac(1, 3) = -2 * (mx * qy + mz * qx);
        H_jac(2, 0) = 2 * (mx * qy - my * qx);
        H_jac(2, 1) = 2 * (mx * qz - mz * qx);
        H_jac(2, 2) = 2 * (my * qz + mz * qy);
        H_jac(2, 3) = -2 * (mx * qx + my * qy);

        ekf.updateEKF(h, H_jac, mag_meas_body);

        Eigen::Quaternionf q_corr(x(3), x(0), x(1), x(2));
        q_corr.normalize();
        x(0) = q_corr.x();
        x(1) = q_corr.y();
        x(2) = q_corr.z();
        x(3) = q_corr.w();
    }

    void setGyroAngularRate(const Eigen::Vector3f &omega)
    {
        ekf.stateVector.template segment<3>(4) = omega;
    }

    Eigen::Quaternionf getOrientation() const
    {
        const auto &x = ekf.stateVector;
        return Eigen::Quaternionf(x(3), x(0), x(1), x(2)).normalized();
    }

    Eigen::Vector3f getYawPitchRoll() const
    {
        const auto &q = getOrientation();

        float sinp = 2.f * (q.w() * q.y() - q.z() * q.x());
        sinp = std::clamp(sinp, -1.f, 1.f); // safe for asin

        float yaw = std::atan2(2.f * (q.w() * q.z() + q.x() * q.y()),
                               1.f - 2.f * (q.y() * q.y() + q.z() * q.z()));
        float pitch = std::asin(sinp);
        float roll = std::atan2(2.f * (q.w() * q.x() + q.y() * q.z()),
                                1.f - 2.f * (q.x() * q.x() + q.y() * q.y()));

        return Eigen::Vector3f(yaw, pitch, roll);
    }

    void printDebugState(const std::string &label = "") const
    {
        const auto &x = ekf.stateVector;
        std::cout << "\n[Tracker State] " << label << "\n";
        std::cout << "Quaternion: [" << x(0) << ", " << x(1) << ", " << x(2) << ", " << x(3) << "]\n";
        std::cout << "Angular rate: [" << x(4) << ", " << x(5) << ", " << x(6) << "]\n";
        std::cout << "Orientation: " << getYawPitchRoll().transpose() * 180.0f / M_PIf << " deg\n";
    }

private:
    KalmanFilter<StateSize, MeasurementSize> ekf;
    float last_timestamp_sec;
};
