#pragma once
#include <Eigen/Dense>
#include <functional>
#include "Kalman.hpp"

#include "au.hpp"

#include "TimeUtils.hpp"
#ifdef __arm__
#include "usbd_cdc_if.h"
#endif
#ifdef __x86_64__
#include "mock_hal.h"
#endif

class GyrMagOrientationTracker
{
public:
    static constexpr int StateSize = 7;       // [qx, qy, qz, qw, wx, wy, wz]
    static constexpr int MeasurementSize = 3; // imunetometer: 3D vector

    using StateVector = Eigen::Matrix<float, StateSize, 1>;
    using Measurement = Eigen::Vector3f;

    GyrMagOrientationTracker()
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

    void predictTo(float new_timestamp_sec)
    {
        float dt = new_timestamp_sec - last_timestamp_sec;
        if (dt <= 0.f)
            return;

        auto &x = ekf.stateVector;
        Eigen::Quaternionf q(x(3), x(0), x(1), x(2));
        Eigen::Vector3f omega = x.segment<3>(4);

        float angle = omega.norm() * dt;
        Eigen::Quaternionf delta_q = Eigen::Quaternionf::Identity();
        if (angle > 1e-6f)
        {
            Eigen::Vector3f axis = omega.normalized();
            delta_q = Eigen::AngleAxisf(angle, axis);
        }

        q = (q * delta_q).normalized();

        x(0) = q.x();
        x(1) = q.y();
        x(2) = q.z();
        x(3) = q.w();
        ekf.stateCovarianceMatrix += ekf.processNoiseCovarianceMatrix;
        last_timestamp_sec = new_timestamp_sec;
    }

    void updateGyro(const Eigen::Vector3f &omega, float timestamp_sec)
    {
        predictTo(timestamp_sec);
        ekf.stateVector.segment<3>(4) = omega;
    }

    void updateMagnetometer(const Eigen::Vector3f &imu_meas_body, float timestamp_sec)
    {
        predictTo(timestamp_sec);

        Eigen::Vector3f imu_ned(0.3f, 0.5f, 0.8f); // Fixed reference in NED
        imu_ned.normalize();

        auto h = [&](const StateVector &x)
        {
            Eigen::Quaternionf q(x(3), x(0), x(1), x(2));
            return q.conjugate() * imu_ned;
        };

        auto &x = ekf.stateVector;
        Eigen::Quaternionf q_hat(x(3), x(0), x(1), x(2));
        float qw = q_hat.w(), qx = q_hat.x(), qy = q_hat.y(), qz = q_hat.z();
        float mx = imu_ned(0), my = imu_ned(1), mz = imu_ned(2);

        Eigen::Matrix<float, MeasurementSize, StateSize> H_jac = Eigen::Matrix<float, MeasurementSize, StateSize>::Zero();

        H_jac(0, 0) = 2.0f * (my * qz - mz * qy);
        H_jac(0, 1) = 2.0f * (my * qy + mz * qz);
        H_jac(0, 2) = 2.0f * (my * qw - mx * qz);
        H_jac(0, 3) = -2.0f * (my * qx + mz * qy);
        H_jac(1, 0) = 2.0f * (mz * qx - mx * qz);
        H_jac(1, 1) = 2.0f * (mx * qx + mz * qw);
        H_jac(1, 2) = 2.0f * (mx * qz - my * qw);
        H_jac(1, 3) = -2.0f * (mx * qy + mz * qx);
        H_jac(2, 0) = 2.0f * (mx * qy - my * qx);
        H_jac(2, 1) = 2.0f * (mx * qz - mz * qx);
        H_jac(2, 2) = 2.0f * (my * qz + mz * qy); // Corre
        H_jac(2, 3) = -2.0f * (mx * qx + my * qy);

        ekf.updateEKF(h, H_jac, imu_meas_body);

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

template <typename Tracker, typename IMU, typename MAG>
class GyrMagOrientation
{
public:
    GyrMagOrientation() = delete;

    GyrMagOrientation(RTC_HandleTypeDef *hrtc, Tracker &tracker, IMU &imu, MAG &mag, uint16_t imu_rate = 1, uint16_t mag_rate = 1) : hrtc_(hrtc), tracker_(tracker), imu_(imu), mag_(mag), imu_rate_(imu_rate), mag_rate_(mag_rate), imu_counter_(0U), mag_counter_(0U) {}

    bool predict(std::array<float, 4> &q, au::QuantityU64<au::Milli<au::Seconds>> &timestamp);

private:
    RTC_HandleTypeDef *hrtc_;
    Tracker &tracker_;
    IMU &imu_;
    MAG &mag_;

    uint16_t imu_rate_;
    uint16_t mag_rate_;

    uint16_t imu_counter_;
    uint16_t mag_counter_;
};

template <typename Tracker, typename IMU, typename MAG>
bool GyrMagOrientation<Tracker, IMU, MAG>::predict(std::array<float, 4> &q, au::QuantityU64<au::Milli<au::Seconds>> &timestamp)
{
    TimeUtils::RTCDateTimeSubseconds rtc;
    HAL_RTC_GetTime(hrtc_, &rtc.time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(hrtc_, &rtc.date, RTC_FORMAT_BIN);
    timestamp = au::make_quantity<au::Milli<au::Seconds>>(TimeUtils::from_rtc(rtc, hrtc_->Init.SynchPrediv).count());
    au::QuantityF<au::Seconds> timestamp_sec = au::make_quantity<au::Milli<au::Seconds>>(timestamp.in(au::milli(au::seconds)));

    if (imu_counter_ % imu_rate_ == 0)
    {
        auto optional_angular = imu_.getGyroscope();
        if (optional_angular.has_value())
        {
            auto angular = optional_angular.value();
            tracker_.updateGyro(Eigen::Vector3f(angular.x.in(au::radians / au::seconds), angular.y.in(au::radians / au::seconds), angular.z.in(au::radians / au::seconds)), timestamp_sec.in(au::seconds));
        }
    }

    if (mag_counter_ % mag_rate_ == 0)
    {
        auto optional_magnetic = imu_.getMagnetometer();
        if (optional_magnetic.has_value())
        {
            auto magnetic = optional_magnetic.value();
            tracker_.updateMagnetometer(Eigen::Vector3f(magnetic.x.in(au::tesla), magnetic.y.in(au::tesla), magnetic.z.in(au::tesla)), timestamp_sec.in(au::seconds));
        }
    }

    auto q_ = tracker_.getOrientation();
    q[0] = q_.w();
    q[1] = q_.x();
    q[2] = q_.y();
    q[3] = q_.z();

    ++imu_counter_;
    ++mag_counter_;
    return true;
}

#
#
#


Eigen::Matrix<float, 3, 4> computeNumericalJacobian(const Eigen::Quaternionf &q, const Eigen::Vector3f &v)
{
    const float eps = 1e-4f;
    Eigen::Matrix<float, 3, 4> J = Eigen::Matrix<float, 3, 4>::Zero();

    for (int i = 0; i < 4; ++i)
    {
        Eigen::Quaternionf q_plus = q;
        Eigen::Quaternionf q_minus = q;
        q_plus.normalize();
        q_minus.normalize();


        if (i == 0)
        {
            q_plus.x() += eps;
            q_minus.x() -= eps;
        }
        else if (i == 1)
        {
            q_plus.y() += eps;
            q_minus.y() -= eps;
        }
        else if (i == 2)
        {
            q_plus.z() += eps;
            q_minus.z() -= eps;
        }
        else
        {
            q_plus.w() += eps;
            q_minus.w() -= eps;
        }

        // float eps_plus = eps / q_plus.norm();
        // float eps_minus = eps / q_minus.norm();

        // // std::cerr << "Norms " << eps << ": "<< eps_plus << " " << eps_minus << std::endl;

        q_plus.normalize();
        q_minus.normalize();

        Eigen::Matrix3f R_plus = q_plus.conjugate().toRotationMatrix();
        Eigen::Matrix3f R_minus = q_minus.conjugate().toRotationMatrix();

        J.col(i) = (R_plus - R_minus) * v / (2.0f * eps);
    }

    // std::cerr << "Jacobian computed via numerical perturbation:\n" << J << std::endl;
    return J;
}

Eigen::Matrix<float, 3, 4> computeAnalyticalJacobian(const Eigen::Quaternionf &q, const Eigen::Vector3f &v)
{
    const float x = q.x(), y = q.y(), z = q.z(), w = q.w();
    const float vx = v.x(), vy = v.y(), vz = v.z();

    Eigen::Matrix<float, 3, 4> J;

    // d(R^T * v) / dx
    J(0, 0) =  2.0f * x * vx + 2.0f * y * vy + 2.0f * z * vz;
    J(1, 0) =  2.0f * y * vx - 2.0f * x * vy + 2.0f * w * vz;
    J(2, 0) =  2.0f * z * vx - 2.0f * w * vy - 2.0f * x * vz;

    // d(...) / dy
    J(0, 1) = -2.0f * y * vx + 2.0f * x * vy - 2.0f * w * vz;
    J(1, 1) =  2.0f * x * vx + 2.0f * y * vy + 2.0f * z * vz;
    J(2, 1) =  2.0f * w * vx + 2.0f * z * vy - 2.0f * y * vz;

    // d(...) / dz
    J(0, 2) = -2.0f * z * vx + 2.0f * w * vy + 2.0f * x * vz;
    J(1, 2) = -2.0f * w * vx - 2.0f * z * vy + 2.0f * y * vz;
    J(2, 2) =  2.0f * x * vx + 2.0f * y * vy + 2.0f * z * vz;

    // // d(...) / dw
    J(0, 3) =  2.0f * w * vx + 2.0f * z * vy - 2.0f * y * vz;
    J(1, 3) = -2.0f * z * vx + 2.0f * w * vy + 2.0f * x * vz;
    J(2, 3) =  2.0f * y * vx - 2.0f * x * vy + 2.0f * w * vz;

    // std::cerr << "Jacobian computed via analytical formula:\n" << J << std::endl;
    return J;
}

Eigen::Matrix<float, 3, 4> normalizeAnalyticalJacobian(
    const Eigen::Matrix<float, 3, 4>& J_analytical,
    const Eigen::Quaternionf& q,
    const Eigen::Vector3f& v)
{
    Eigen::Matrix<float, 3, 4> J_normalized;
    const float w = q.w(), x = q.x(), y = q.y(), z = q.z();
    const float N = x * x + y * y + z * z + w * w;

    // Rotated vector: R(q*) * v
    Eigen::Vector3f v_rot = q.conjugate().toRotationMatrix() * v;

    for (int i = 0; i < 4; ++i)
    {
        float qi = (i == 0) ? x :
                   (i == 1) ? y :
                   (i == 2) ? z :
                              w;

        J_normalized.col(i) = J_analytical.col(i) / N - (2.0f * qi / (N * N)) * v_rot;
    }

    // std::cerr << "Jacobian normalized via analytical formula:\n" << J_normalized << std::endl;
    return J_normalized;
}

class AccGyrMagOrientationTracker
{
public:
    static constexpr int StateSize = 7;
    static constexpr int MeasurementSize = 6;

    using StateVector = Eigen::Matrix<float, StateSize, 1>;
    using Measurement = Eigen::Matrix<float, MeasurementSize, 1>;

    AccGyrMagOrientationTracker()
        : ekf(
              Eigen::Matrix<float, StateSize, StateSize>::Identity() * 0.1f,
              Eigen::Matrix<float, MeasurementSize, MeasurementSize>::Identity() * 0.001f,
              Eigen::Matrix<float, StateSize, StateSize>::Identity() * 1e-5f,
              []
              {
                  StateVector x;
                  x << 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f;
                  return x;
              }()),
          last_timestamp_sec(0.0f),
          prev_orientation(Eigen::Quaternionf::Identity())
    {
    }

    void predictTo(float new_timestamp_sec)
    {
        float dt = new_timestamp_sec - last_timestamp_sec;
        if (dt <= 0.f)
            return;

        auto &x = ekf.stateVector;
        Eigen::Quaternionf q(x(3), x(0), x(1), x(2));
        Eigen::Vector3f omega = x.segment<3>(4);

        float angle = omega.norm() * dt;
        Eigen::Quaternionf delta_q = Eigen::Quaternionf::Identity();
        if (angle > 1e-6f)
        {
            Eigen::Vector3f axis = omega.normalized();
            delta_q = Eigen::AngleAxisf(angle, axis);
        }

        q = (q * delta_q).normalized();
        if (q.dot(prev_orientation) < 0.f)
            q.coeffs() *= -1.f;

        // std::cout << "[predictTo] dt: " << dt << ", omega: " << omega.transpose()
        //           << ", angle: " << angle << "\n";
        // std::cout << "[predictTo] q updated: " << q.coeffs().transpose() << "\n";

        prev_orientation = q;
        x(0) = q.x();
        x(1) = q.y();
        x(2) = q.z();
        x(3) = q.w();
        ekf.stateCovarianceMatrix += ekf.processNoiseCovarianceMatrix;
        last_timestamp_sec = new_timestamp_sec;
    }

    void updateGyro(const Eigen::Vector3f &omega, float timestamp_sec)
    {
        predictTo(timestamp_sec);
        ekf.stateVector.segment<3>(4) = omega;
    }

    void updateAccelerometerMagnetometer(const Eigen::Vector3f &accel_body,
                                         const Eigen::Vector3f &mag_body,
                                         float timestamp_sec)
    {
        predictTo(timestamp_sec);

        Eigen::Vector3f accel_ned(0.f, 0.f, 9.81f); // Gravity points down
        Eigen::Vector3f mag_ned(1.f, 0.f, 0.f);     // Needs to be adjusted for magnetic declination and inclination

        // Combined measurement vector
        Measurement combined_meas;
        combined_meas.segment<3>(0) = accel_body;
        combined_meas.segment<3>(3) = mag_body;

        auto h = [&](const StateVector &x)
        {
            Eigen::Quaternionf q(x(3), x(0), x(1), x(2));
            Eigen::Vector3f predicted_accel_body = q.conjugate() * accel_ned;
            Eigen::Vector3f predicted_mag_body = q.conjugate() * mag_ned;

            Measurement z;
            z.segment<3>(0) = predicted_accel_body;
            z.segment<3>(3) = predicted_mag_body;
            return z;
        };

        Eigen::Quaternionf q_raw(ekf.stateVector(3), ekf.stateVector(0), ekf.stateVector(1), ekf.stateVector(2));
        Eigen::Matrix<float, 3, 4> J_accel = computeAnalyticalJacobian(q_raw, accel_ned);
        J_accel = normalizeAnalyticalJacobian(J_accel, q_raw, accel_ned);
        Eigen::Matrix<float, 3, 4> J_mag = computeAnalyticalJacobian(q_raw, mag_ned);
        J_mag = normalizeAnalyticalJacobian(J_mag, q_raw, mag_ned);

        Eigen::Matrix<float, MeasurementSize, StateSize> H_analytical = Eigen::Matrix<float, MeasurementSize, StateSize>::Zero();
        H_analytical.block<3, 4>(0, 0) = J_accel;
        H_analytical.block<3, 4>(3, 0) = J_mag;

        ekf.updateEKF(h, H_analytical, combined_meas);

        auto &x = ekf.stateVector;
        Eigen::Quaternionf q_corr(x(3), x(0), x(1), x(2));
        q_corr.normalize();
        x(0) = q_corr.x();
        x(1) = q_corr.y();
        x(2) = q_corr.z();
        x(3) = q_corr.w();
    }

    void setGyroAngularRate(const Eigen::Vector3f &omega)
    {
        ekf.stateVector.segment<3>(4) = omega;
    }

    void setOrientation(const Eigen::Quaternionf &q)
    {
        ekf.stateVector(0) = q.x();
        ekf.stateVector(1) = q.y();
        ekf.stateVector(2) = q.z();
        ekf.stateVector(3) = q.w();
    }

    Eigen::Quaternionf getStableOrientation() const
    {
        Eigen::Quaternionf q(ekf.stateVector(3), ekf.stateVector(0), ekf.stateVector(1), ekf.stateVector(2));
        if (q.dot(prev_orientation) < 0.f)
            q.coeffs() *= -1.f;
        return q.normalized();
    }

    Eigen::Vector3f getYawPitchRoll() const
    {
        const auto &q = getStableOrientation();
        float sinp = 2.f * (q.w() * q.y() - q.z() * q.x());
        sinp = std::clamp(sinp, -1.f, 1.f);

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
    Eigen::Quaternionf prev_orientation;
};

template <typename Tracker, typename IMU, typename MAG>
class AccGyrMagOrientation
{
public:
    AccGyrMagOrientation() = delete;

    AccGyrMagOrientation(RTC_HandleTypeDef *hrtc, Tracker &tracker, IMU &imu, MAG &mag, uint16_t imu_rate = 1, uint16_t mag_rate = 1)
        : hrtc_(hrtc), tracker_(tracker), imu_(imu), mag_(mag), imu_rate_(imu_rate), mag_rate_(mag_rate), imu_counter_(0U), mag_counter_(0U)
    {
    }

    bool predict(std::array<float, 4> &q, au::QuantityU64<au::Milli<au::Seconds>> &timestamp);

private:
    RTC_HandleTypeDef *hrtc_;
    Tracker &tracker_;
    IMU &imu_;
    MAG &mag_;

    uint16_t imu_rate_;
    uint16_t mag_rate_;

    uint16_t imu_counter_;
    uint16_t mag_counter_;
};

template <typename Tracker, typename IMU, typename MAG>
bool AccGyrMagOrientation<Tracker, IMU, MAG>::predict(std::array<float, 4> &q, au::QuantityU64<au::Milli<au::Seconds>> &timestamp)
{
    TimeUtils::RTCDateTimeSubseconds rtc;
    HAL_RTC_GetTime(hrtc_, &rtc.time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(hrtc_, &rtc.date, RTC_FORMAT_BIN);
    timestamp = au::make_quantity<au::Milli<au::Seconds>>(TimeUtils::from_rtc(rtc, hrtc_->Init.SynchPrediv).count());
    au::QuantityF<au::Seconds> timestamp_sec = au::make_quantity<au::Milli<au::Seconds>>(timestamp.in(au::milli(au::seconds)));

    if (imu_counter_ % imu_rate_ == 0)
    {
        auto optional_angular = imu_.getGyroscope();
        if (optional_angular.has_value())
        {
            auto angular = optional_angular.value();
            tracker_.updateGyro(Eigen::Vector3f(angular.x.in(au::radians / au::seconds), angular.y.in(au::radians / au::seconds), angular.z.in(au::radians / au::seconds)), timestamp_sec.in(au::seconds));

            auto optional_accel = imu_.getAccelerometer();
            auto optional_magnetic = imu_.getMagnetometer();

            if (optional_accel.has_value() && optional_magnetic.has_value())
            {
                auto accel = optional_accel.value();
                auto magnetic = optional_magnetic.value();

                Eigen::Vector3f accel_body(accel.x.in(au::meters / (au::seconds * au::seconds)),
                                           accel.y.in(au::meters / (au::seconds * au::seconds)),
                                           accel.z.in(au::meters / (au::seconds * au::seconds)));
                Eigen::Vector3f mag_body(magnetic.x.in(au::tesla), magnetic.y.in(au::tesla), magnetic.z.in(au::tesla));

                tracker_.updateAccelerometerMagnetometer(accel_body, mag_body, timestamp_sec.in(au::seconds));
            }
        }
    }

    auto q_ = tracker_.getOrientation();
    q[0] = q_.w();
    q[1] = q_.x();
    q[2] = q_.y();
    q[3] = q_.z();

    ++imu_counter_;
    ++mag_counter_;
    return true;
}