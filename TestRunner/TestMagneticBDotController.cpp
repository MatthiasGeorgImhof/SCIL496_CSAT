#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "MagneticBDotController.hpp"
#include <Eigen/Core>

TEST_CASE("BDotController: first call returns zero and initializes") {
    BDotController bdot(1e4f);
    MagneticField B_now(10e-6f, -5e-6f, 20e-6f);
    float dt = 0.1f;

    DipoleMoment m_cmd = bdot.computeDipoleMoment(B_now, dt);
    CHECK(m_cmd.isZero());
}

TEST_CASE("BDotController: second call returns scaled negative B-dot") {
    BDotController bdot(1e4f);
    MagneticField B1(10e-6f, -5e-6f, 20e-6f);
    MagneticField B2(12e-6f, -4e-6f, 18e-6f);
    float t0 = 0.1f;
    float t1 = 0.2f;

    DipoleMoment m_cmd = bdot.computeDipoleMoment(B1, t0); // initialize
    CHECK(m_cmd.isZero());
    m_cmd = bdot.computeDipoleMoment(B2, t1);

    MagneticField B_dot = (B2 - B1) / (t1 - t0);
    Eigen::Vector3f expected = -1e4f * B_dot;

    CHECK(m_cmd.isApprox(expected));
}

TEST_CASE("BDotController: zero or negative dt returns zero") {
    BDotController bdot(1e4f);
    MagneticField B(10e-6f, 0.0f, 0.0f);

    DipoleMoment m1 = bdot.computeDipoleMoment(B, 0.0f);
    DipoleMoment m2 = bdot.computeDipoleMoment(B, -0.1f);

    CHECK(m1.isZero());
    CHECK(m2.isZero());
}

TEST_CASE("BDotController: reset clears state") {
    BDotController bdot(1e4f);
    MagneticField B1(10e-6f, 0.0f, 0.0f);
    MagneticField B2(12e-6f, 0.0f, 0.0f);
    float dt = 0.1f;

    bdot.computeDipoleMoment(B1, dt);
    bdot.reset();
    DipoleMoment m_cmd = bdot.computeDipoleMoment(B2, dt);

    CHECK(m_cmd.isZero());
}
