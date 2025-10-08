// MagneticBDotController.hpp

#pragma once
#include <Eigen/Core>

class BDotController {
public:
    explicit BDotController(float gain) : k(gain), initialized(false) {}

    // Call this once per control cycle
    Eigen::Vector3f computeDipoleMoment(const Eigen::Vector3f& B_now, float dt) {
        if (!initialized || dt <= 0.0f) {
            B_prev = B_now;
            initialized = true;
            return Eigen::Vector3f::Zero();
        }

        Eigen::Vector3f B_dot = (B_now - B_prev) / dt;
        B_prev = B_now;

        return -k * B_dot;
    }

    void reset() {
        initialized = false;
        B_prev = Eigen::Vector3f::Zero();
    }

private:
    float k;
    bool initialized;
    Eigen::Vector3f B_prev;
};
