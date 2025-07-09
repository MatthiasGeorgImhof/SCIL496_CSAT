#pragma once
#include <Eigen/Dense>
#include "Kalman.hpp"

class Sgp4PositionTracker {
public:
    static constexpr int StateSize = 6;        // [px, py, pz, vx, vy, vz]
    static constexpr int MeasurementSize = 3;  // GPS: [px, py, pz]

    using StateVector = Eigen::Matrix<float, StateSize, 1>;
    using Measurement = Eigen::Vector3f;

    Sgp4PositionTracker()
        : Q(Eigen::Matrix<float, StateSize, StateSize>::Identity() * 0.01f),
          R(Eigen::Matrix3f::Identity() * 0.1f),
          H(Eigen::Matrix<float, MeasurementSize, StateSize>::Zero()),
          kf(Q, R, Q, StateVector::Zero())
    {
        H.block<3, 3>(0, 0) = Eigen::Matrix3f::Identity();
    }

    void setPrediction(const Eigen::Vector3f& pos, const Eigen::Vector3f& vel) {
        StateVector pred;
        pred << pos, vel;
        kf.stateVector = pred;
        kf.stateCovarianceMatrix = Q;
    }

    void updateWithGps(const Eigen::Vector3f& gps_measurement) {
        kf.update(H, gps_measurement);
    }

    StateVector getState() const {
        return kf.getState();
    }

private:
    Eigen::Matrix<float, StateSize, StateSize> Q;
    Eigen::Matrix3f R;
    Eigen::Matrix<float, MeasurementSize, StateSize> H;
    KalmanFilter<StateSize, MeasurementSize> kf;
};
