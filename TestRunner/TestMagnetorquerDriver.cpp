#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "MagnetorquerDriver.hpp"
#include <cmath>

constexpr float TOL = 1e-4f;

TEST_CASE("MagnetorquerDriver: computePWM")
{
    MagnetorquerDriver::Config config{0.05f, 0.10f, 0.20f}; // A·m²
    MagnetorquerDriver driver(config);

    SUBCASE("Nominal dipole within bounds")
    {
        Eigen::Vector3f m_cmd(0.025f, -0.05f, 0.10f);
        auto pwm = driver.computePWM(m_cmd);

        CHECK(std::abs(pwm.duty_x - 0.5f) < TOL);
        CHECK(std::abs(pwm.duty_y + 0.5f) < TOL);
        CHECK(std::abs(pwm.duty_z - 0.5f) < TOL);
    }

    SUBCASE("Saturation clamps to ±1.0")
    {
        Eigen::Vector3f m_cmd(0.10f, -0.20f, 0.50f); // All exceed limits
        auto pwm = driver.computePWM(m_cmd);

        CHECK(std::abs(pwm.duty_x - 1.0f) < TOL);
        CHECK(std::abs(pwm.duty_y + 1.0f) < TOL);
        CHECK(std::abs(pwm.duty_z - 1.0f) < TOL);
    }

    SUBCASE("Zero dipole yields zero duty")
    {
        Eigen::Vector3f m_cmd = Eigen::Vector3f::Zero();
        auto pwm = driver.computePWM(m_cmd);

        CHECK(std::abs(pwm.duty_x) < TOL);
        CHECK(std::abs(pwm.duty_y) < TOL);
        CHECK(std::abs(pwm.duty_z) < TOL);
    }

    SUBCASE("Polarity is preserved")
    {
        Eigen::Vector3f m_cmd_pos(0.01f, 0.01f, 0.01f);
        Eigen::Vector3f m_cmd_neg(-0.01f, -0.01f, -0.01f);

        auto pwm_pos = driver.computePWM(m_cmd_pos);
        auto pwm_neg = driver.computePWM(m_cmd_neg);

        CHECK(pwm_pos.duty_x > 0.0f);
        CHECK(pwm_neg.duty_x < 0.0f);
        CHECK(pwm_pos.duty_y > 0.0f);
        CHECK(pwm_neg.duty_y < 0.0f);
        CHECK(pwm_pos.duty_z > 0.0f);
        CHECK(pwm_neg.duty_z < 0.0f);
    }

    SUBCASE("Asymmetric config scales independently")
    {
        Eigen::Vector3f m_cmd(0.05f, 0.10f, 0.20f); // All at max
        auto pwm = driver.computePWM(m_cmd);

        CHECK(std::abs(pwm.duty_x - 1.0f) < TOL);
        CHECK(std::abs(pwm.duty_y - 1.0f) < TOL);
        CHECK(std::abs(pwm.duty_z - 1.0f) < TOL);
    }
}
