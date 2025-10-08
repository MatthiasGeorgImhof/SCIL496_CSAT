#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "MagneticBDotController.hpp"
#include <Eigen/Core>

TEST_CASE("BDotController: first call returns zero and initializes") {
    BDotController bdot(1e4f);
    Eigen::Vector3f B_now(10e-6f, -5e-6f, 20e-6f);
    float dt = 0.1f;

    Eigen::Vector3f m_cmd = bdot.computeDipoleMoment(B_now, dt);
    CHECK(m_cmd.isApprox(Eigen::Vector3f::Zero()));
}

TEST_CASE("BDotController: second call returns scaled negative B-dot") {
    BDotController bdot(1e4f);
    Eigen::Vector3f B1(10e-6f, -5e-6f, 20e-6f);
    Eigen::Vector3f B2(12e-6f, -4e-6f, 18e-6f);
    float dt = 0.1f;

    bdot.computeDipoleMoment(B1, dt); // initialize
    Eigen::Vector3f m_cmd = bdot.computeDipoleMoment(B2, dt);

    Eigen::Vector3f B_dot = (B2 - B1) / dt;
    Eigen::Vector3f expected = -1e4f * B_dot;

    CHECK(m_cmd.isApprox(expected));
}

TEST_CASE("BDotController: zero or negative dt returns zero") {
    BDotController bdot(1e4f);
    Eigen::Vector3f B(10e-6f, 0.0f, 0.0f);

    Eigen::Vector3f m1 = bdot.computeDipoleMoment(B, 0.0f);
    Eigen::Vector3f m2 = bdot.computeDipoleMoment(B, -0.1f);

    CHECK(m1.isApprox(Eigen::Vector3f::Zero()));
    CHECK(m2.isApprox(Eigen::Vector3f::Zero()));
}

TEST_CASE("BDotController: reset clears state") {
    BDotController bdot(1e4f);
    Eigen::Vector3f B1(10e-6f, 0.0f, 0.0f);
    Eigen::Vector3f B2(12e-6f, 0.0f, 0.0f);
    float dt = 0.1f;

    bdot.computeDipoleMoment(B1, dt);
    bdot.reset();
    Eigen::Vector3f m_cmd = bdot.computeDipoleMoment(B2, dt);

    CHECK(m_cmd.isApprox(Eigen::Vector3f::Zero()));
}
