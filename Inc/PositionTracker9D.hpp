#pragma once
#include <Eigen/Dense>
#include "Kalman.hpp"

class PositionTracker9D
{
public:
    static constexpr int StateSize = 9;
    static constexpr int PosMeasSize = 3;
    static constexpr int AccMeasSize = 3;

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
