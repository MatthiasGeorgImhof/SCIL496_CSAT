// LVLHAttitudeTarget.hpp

#pragma once
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <array>
#include "au.hpp"

class LVLHAttitudeTarget
{
public:
    LVLHAttitudeTarget() = default;

    // Computes desired body-to-NED quaternion for geocentric nadir pointing
    Eigen::Quaternionf computeDesiredAttitudeECEF(
        const std::array<au::QuantityF<au::MetersInEcefFrame>, 3> &ecef_position,
        const std::array<au::QuantityF<au::MetersPerSecondInEcefFrame>, 3> &ecef_velocity) const;
};

class AttitudeError
{
public:
    static Eigen::Quaternionf computeQuaternionError(
        const Eigen::Quaternionf &q_desired,
        const Eigen::Quaternionf &q_current);

    static Eigen::Vector3f rotationVector(const Eigen::Quaternionf &q_error);
};

class AttitudeController
{
public:
    AttitudeController(float kp, float kd) : Kp(kp), Kd(kd) {}

    Eigen::Vector3f computeOmegaCommand(
        const Eigen::Vector3f &rotation_error,
        const Eigen::Vector3f &omega_measured) const;

private:
    float Kp, Kd;
};

class MagnetorquerController
{
public:
    static Eigen::Vector3f computeDipoleMoment(
        const Eigen::Vector3f &omega_cmd,
        const Eigen::Vector3f &B_body);
};
