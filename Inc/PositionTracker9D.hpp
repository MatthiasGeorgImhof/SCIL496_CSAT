#pragma once
#include <Eigen/Dense>
#include "Kalman.hpp"
#include "au.hpp"

#include "TimeUtils.hpp"
#ifdef __arm__
#include "usbd_cdc_if.h"
#endif
#ifdef __x86_64__
#include "mock_hal.h"
#endif

class PositionTracker9D
{
public:
    static constexpr int StateSize = 9;
    static constexpr int PosMeasSize = 3;
    static constexpr int VelMeasSize = 3;
    static constexpr int AccMeasSize = 3;

    static_assert(StateSize == PosMeasSize + VelMeasSize + AccMeasSize, "State size must match the sum of position, velocity, and acceleration measurement sizes.");

    using StateVector = Eigen::Matrix<float, StateSize, 1>;

    PositionTracker9D()
        : last_timestamp_sec_(-1.f),
          A(Eigen::Matrix<float, StateSize, StateSize>::Identity()),
          H_gps(Eigen::Matrix<float, PosMeasSize, StateSize>::Zero()),
          H_acc(Eigen::Matrix<float, AccMeasSize, StateSize>::Zero()),
          Q(Eigen::Matrix<float, StateSize, StateSize>::Identity() * 1e-4f),
          R_gps(Eigen::Matrix3f::Identity() * 5e-3f),
          R_accel(Eigen::Matrix3f::Identity() * 1e-2f),
          kf(Q, R_gps, Q, StateVector::Zero())

    {
        H_gps.block<3, 3>(0, 0) = Eigen::Matrix3f::Identity();
        H_acc.block<3, 3>(0, 6) = Eigen::Matrix3f::Identity();
    }

    void updateWithAccel(const Eigen::Vector3f &accel, float timestamp_sec)
    {
        maybePredict(timestamp_sec);

        KalmanFilter<StateSize, AccMeasSize> accelKF(Q, R_accel, kf.stateCovarianceMatrix, kf.stateVector);
        accelKF.update(H_acc, accel);
        kf.stateVector = accelKF.stateVector;
        kf.stateCovarianceMatrix = accelKF.stateCovarianceMatrix;
    }

    void updateWithGps(const Eigen::Vector3f &gps, float timestamp_sec)
    {
        maybePredict(timestamp_sec);
        kf.update(H_gps, gps);
    }

    StateVector getState() const
    {
        return kf.getState();
    }

private:
    void maybePredict(float timestamp_sec)
    {
        if (last_timestamp_sec_ < 0.f)
        {
            last_timestamp_sec_ = timestamp_sec;
            return;
        }

        float dt = timestamp_sec - last_timestamp_sec_;
        if (dt <= 0.f)
            return;

        updateTransitionMatrix(dt);
        kf.predict(A);
        last_timestamp_sec_ = timestamp_sec;
    }

    void updateTransitionMatrix(float dt)
    {
        A.setIdentity();
        for (int i = 0; i < 3; ++i)
        {
            A(i, i + 3) = dt;
            A(i, i + 6) = 0.5f * dt * dt;
            A(i + 3, i + 6) = dt;
        }
    }

    float last_timestamp_sec_;
    Eigen::Matrix<float, StateSize, StateSize> A;
    Eigen::Matrix<float, PosMeasSize, StateSize> H_gps;
    Eigen::Matrix<float, AccMeasSize, StateSize> H_acc;
    Eigen::Matrix<float, StateSize, StateSize> Q;
    Eigen::Matrix3f R_gps, R_accel;
    KalmanFilter<StateSize, PosMeasSize> kf;
};

template <typename Tracker, typename GNSS, typename IMU>
class GNSSandAccelPosition
{
public:
    GNSSandAccelPosition() = delete;

    GNSSandAccelPosition(RTC_HandleTypeDef *hrtc, Tracker &tracker, GNSS &gnss, IMU &imu, uint16_t gnss_rate = 1, uint16_t imu_rate = 1) : hrtc_(hrtc), tracker_(tracker), gnss_(gnss), imu_(imu), gnss_rate_(gnss_rate), imu_rate_(imu_rate), gnss_counter_(0U), imu_counter_(0) {}

    bool predict(std::array<au::QuantityF<au::Meters>, 3> &r, std::array<au::QuantityF<au::MetersPerSecond>, 3> &v, au::QuantityU64<au::Milli<au::Seconds>> &timestamp);

private:
    RTC_HandleTypeDef *hrtc_;
    Tracker tracker_;
    GNSS &gnss_;
    IMU &imu_;

    uint16_t gnss_rate_;
    uint16_t imu_rate_;

    uint16_t gnss_counter_;
    uint16_t imu_counter_;
};

template <typename Tracker, typename GNSS, typename IMU>
bool GNSSandAccelPosition<Tracker, GNSS, IMU>::predict(std::array<au::QuantityF<au::Meters>, 3> &r, std::array<au::QuantityF<au::MetersPerSecond>, 3> &v, au::QuantityU64<au::Milli<au::Seconds>> &timestamp)
{
    TimeUtils::RTCDateTimeSubseconds rtc;
    HAL_RTC_GetTime(hrtc_, &rtc.time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(hrtc_, &rtc.date, RTC_FORMAT_BIN);
    timestamp = au::make_quantity<au::Milli<au::Seconds>>(TimeUtils::from_rtc(rtc, hrtc_->Init.SynchPrediv).count());
    au::QuantityF<au::Seconds> timestamp_sec = au::make_quantity<au::Milli<au::Seconds>>(timestamp.in(au::milli(au::seconds)));

    if (gnss_counter_ % gnss_rate_ == 0)
    {
        auto optional_pos_ecef = gnss_.GetNavPosECEF();
        if (optional_pos_ecef.has_value())
        {
            auto pos_ecef = ConvertPositionECEF(optional_pos_ecef.value());
            tracker_.updateWithGps(Eigen::Vector3f(pos_ecef.x.in(au::meters), pos_ecef.y.in(au::meters), pos_ecef.z.in(au::meters)), timestamp_sec.in(au::seconds));
        }
    }

    if (imu_counter_ % imu_rate_ == 0)
    {
        auto optional_accel = imu_.getAcceleration();
        if (optional_accel.has_value())
        {
            auto accel = optional_accel.value();
            tracker_.updateWithAccel(Eigen::Vector3f(accel.x.in(au::meters / au::seconds / au::seconds), accel.y.in(au::meters / au::seconds / au::seconds), accel.z.in(au::meters / au::seconds / au::seconds)), timestamp_sec.in(au::seconds));
        }
    }

    auto state = tracker_.getState();
    std::transform(state.data(), state.data() + 3, r.begin(), [](const auto &item)
                   { return au::make_quantity<au::Meters>(item); });
    std::transform(state.data() + 3, state.data() + 6, v.begin(), [](const auto &item)
                   { return au::make_quantity<au::MetersPerSecond>(item); });

    ++gnss_counter_;
    ++imu_counter_;
    return true;
}
