// MagneticBDotController.hpp

#pragma once
#include <Eigen/Core>
#include "NamedVector3f.hpp"

class BDotController {
public:
    explicit BDotController(float gain) : k(gain), initialized(false) {}

    // Call this once per control cycle
    DipoleMoment computeDipoleMoment(const MagneticField& B_now, float dt) {
        if (!initialized || dt <= 0.0f) {
            B_prev = B_now;
            initialized = true;
            return DipoleMoment::Zero();
        }

        MagneticField B_dot = (B_now - B_prev) / dt;
        B_prev = B_now;

        return DipoleMoment((-k * B_dot).value.eval());
    }

    void reset() {
        initialized = false;
        B_prev = DipoleMoment::Zero();
    }

private:
    float k;
    bool initialized;
    MagneticField B_prev;
};
