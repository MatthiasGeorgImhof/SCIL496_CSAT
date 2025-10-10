// MagneticBDotController.hpp

#pragma once
#include <Eigen/Core>
#include "NamedVector3f.hpp"
#include "MagnetorquerDriver.hpp"
#include "MagnetorquerHardwareInterface.hpp"

class BDotController {
public:
    explicit BDotController(float gain) : k(gain), t_prev(0.0f), initialized(false) {}

    // Call this once per control cycle with current time and field
    DipoleMoment computeDipoleMoment(const MagneticField& B_now, float t_now) {
        if (!initialized || t_now <= t_prev) {
            B_prev = B_now;
            t_prev = t_now;
            initialized = true;
            return DipoleMoment::Zero();
        }

        float dt = t_now - t_prev;
        MagneticField B_dot = (B_now - B_prev) / dt;

        B_prev = B_now;
        t_prev = t_now;

        return DipoleMoment((-k * B_dot).value.eval());
    }

    void reset() {
        initialized = false;
        t_prev = 0.0f;
        B_prev = DipoleMoment::Zero();
    }

private:
    float k;
    float t_prev;
    bool initialized;
    MagneticField B_prev;
};

class DetumblerSystem
{
public:
    struct Config
    {
        float bdot_gain;
        MagnetorquerDriver::Config driver_config;
        MagnetorquerHardwareInterface::ChannelMap pwm_channels;
        MagnetorquerPolarityController::PinMap gpio_pins;
    };

    explicit DetumblerSystem(const Config &cfg)
        : bdot(cfg.bdot_gain),
          driver(cfg.driver_config),
          actuator(cfg.pwm_channels, cfg.gpio_pins) {}

    void apply(const MagneticField &B_body, float t)
    {
        DipoleMoment m_cmd = bdot.computeDipoleMoment(B_body, t);
        PWMCommand pwm = driver.computePWM(m_cmd);
        actuator.apply(pwm);
    }

    void reset()
    {
        bdot.reset();
    }

    void stopAll() const
    {
        actuator.stopAll();
    }

    void disableAll() const
    {
        actuator.disableAll();
    }

private:
    BDotController bdot;
    MagnetorquerDriver driver;
    MagnetorquerActuator actuator;
};
