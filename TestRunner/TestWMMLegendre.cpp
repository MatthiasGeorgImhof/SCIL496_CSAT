#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "wmm_legendre.hpp"

#include <cmath>

constexpr float DEG_TO_RAD = M_PI / 180.0f; // Conversion factor from degrees to radians
constexpr float RAD_TO_DEG = 180.0f / M_PI; // Conversion factor from radians to degrees

TEST_CASE("Associated Legendre Polynomial Tests: 0.5")
{
    using namespace wmm_model;

    constexpr int nMax = 4;
    constexpr size_t N = (nMax + 1) * (nMax + 2) / 2;
    constexpr std::array<float, N + 1> expected = {1.0000, 0.5000, 0.8660, -0.1250, 0.7500, 0.6495, -0.4375, 0.1326,
                                                   0.7262, 0.5135, -0.2891, -0.4279, 0.3144, 0.6793, 0.4160};

    std::array<float, N> Pcup{};
    std::array<float, N> dPcup{};

    bool result = MAG_PcupLow<nMax, N>(Pcup, dPcup, 0.5f);
    (void)Pcup;
    (void)dPcup;
    CHECK(result == true);

    int k = 0;
    for (int i = 0; i <= nMax; ++i)
    {
        for (int j = 0; j <= i; ++j)
        {
            char name[128];
            sprintf(name, "n=%d, m=%d", i, j);
            SUBCASE(name)
            {
                CHECK(expected[k] == doctest::Approx(Pcup[k]).epsilon(1e-4f));
            }
            ++k;
        }
    }
}

TEST_CASE("Associated Legendre Polynomial Tests MAG_PcupHigh: 0.5")
{
    using namespace wmm_model;

    constexpr int nMax = 4;
    constexpr size_t N = (nMax + 1) * (nMax + 2) / 2;
    constexpr std::array<float, N + 1> expected = {1.0000, 0.5000, 0.8660, -0.1250, 0.7500, 0.6495, -0.4375, 0.1326,
                                                   0.7262, 0.5135, -0.2891, -0.4279, 0.3144, 0.6793, 0.4160};

    std::array<float, N> Pcup{};
    std::array<float, N> dPcup{};

    bool result = MAG_PcupHigh<nMax, N>(Pcup, dPcup, 0.5f);
    (void)Pcup;
    (void)dPcup;
    CHECK(result == true);

    int k = 0;
    for (int i = 0; i <= nMax; ++i)
    {
        for (int j = 0; j <= i; ++j)
        {
            char name[128];
            sprintf(name, "n=%d, m=%d", i, j);
            SUBCASE(name)
            {
                CHECK(expected[k] == doctest::Approx(Pcup[k]).epsilon(1e-4f));
            }
            ++k;
        }
    }
}

TEST_CASE("Associated Legendre Polynomial Tests: 0.1")
{
    using namespace wmm_model;

    constexpr int nMax = 4;
    constexpr size_t N = (nMax + 1) * (nMax + 2) / 2;
    constexpr std::array<float, N + 1> expected = {1.0000, 0.1000, 0.9950, -0.4850, 0.1723, 0.8574, -0.1475,
                                                   -0.5788, 0.1917, 0.7787, 0.3379, -0.2305, -0.5147, 0.2060, 0.7248};

    std::array<float, N> Pcup{};
    std::array<float, N> dPcup{};

    bool result = MAG_PcupLow<nMax, N>(Pcup, dPcup, 0.1f);
    (void)Pcup;
    (void)dPcup;
    CHECK(result == true);

    int k = 0;
    for (int i = 0; i <= nMax; ++i)
    {
        for (int j = 0; j <= i; ++j)
        {
            char name[128];
            sprintf(name, "n=%d, m=%d", i, j);
            SUBCASE(name)
            {
                CHECK(expected[k] == doctest::Approx(Pcup[k]).epsilon(1e-4f));
            }
            ++k;
        }
    }
}

TEST_CASE("Associated Legendre Polynomial Tests: 0.9")
{
    using namespace wmm_model;

    constexpr int nMax = 4;
    constexpr size_t N = (nMax + 1) * (nMax + 2) / 2;
    constexpr std::array<float, N + 1> expected = {1.000000, 0.900000, 0.435890, 0.715000, 0.679485, 0.164545,
                                                   0.472500, 0.814127, 0.331140, 0.065474, 0.207938, 0.828077,
                                                   0.496016, 0.155906, 0.026696};

    std::array<float, N> Pcup{};
    std::array<float, N> dPcup{};

    bool result = MAG_PcupLow<nMax, N>(Pcup, dPcup, 0.9f);
    (void)Pcup;
    (void)dPcup;
    CHECK(result == true);

    int k = 0;
    for (int i = 0; i <= nMax; ++i)
    {
        for (int j = 0; j <= i; ++j)
        {
            char name[128];
            sprintf(name, "n=%d, m=%d", i, j);
            SUBCASE(name)
            {
                CHECK(expected[k] == doctest::Approx(Pcup[k]).epsilon(1e-4f));
            }
            ++k;
        }
    }
}

TEST_CASE("Associated Legendre Polynomial Tests: -0.9")
{
    using namespace wmm_model;

    constexpr int nMax = 4;
    constexpr size_t N = (nMax + 1) * (nMax + 2) / 2;
    constexpr std::array<float, N + 1> expected = {1.000000, -0.900000, 0.435890, 0.715000, -0.679485, 0.164545,
                                                   -0.472500, 0.814127, -0.331140, 0.065474, 0.207938, -0.828077,
                                                   0.496016, -0.155906, 0.026696};

    std::array<float, N> Pcup{};
    std::array<float, N> dPcup{};

    bool result = MAG_PcupLow<nMax, N>(Pcup, dPcup, -0.9f);
    (void)Pcup;
    (void)dPcup;
    CHECK(result == true);

    int k = 0;
    for (int i = 0; i <= nMax; ++i)
    {
        for (int j = 0; j <= i; ++j)
        {
            char name[128];
            sprintf(name, "n=%d, m=%d", i, j);
            SUBCASE(name)
            {
                CHECK(expected[k] == doctest::Approx(Pcup[k]).epsilon(1e-4f));
            }
            ++k;
        }
    }
}

TEST_CASE("Associated Legendre Polynomial Tests MAG_PcupHigh: -0.9")
{
    using namespace wmm_model;

    constexpr int nMax = 4;
    constexpr size_t N = (nMax + 1) * (nMax + 2) / 2;
    constexpr std::array<float, N + 1> expected = {1.000000, -0.900000, 0.435890, 0.715000, -0.679485, 0.164545,
                                                   -0.472500, 0.814127, -0.331140, 0.065474, 0.207938, -0.828077,
                                                   0.496016, -0.155906, 0.026696};

    std::array<float, N> Pcup{};
    std::array<float, N> dPcup{};

    bool result = MAG_PcupHigh<nMax, N>(Pcup, dPcup, -0.9f);
    (void)Pcup;
    (void)dPcup;
    CHECK(result == true);

    int k = 0;
    for (int i = 0; i <= nMax; ++i)
    {
        for (int j = 0; j <= i; ++j)
        {
            char name[128];
            sprintf(name, "n=%d, m=%d", i, j);
            SUBCASE(name)
            {
                CHECK(expected[k] == doctest::Approx(Pcup[k]).epsilon(1e-4f));
            }
            ++k;
        }
    }
}

TEST_CASE("Derivative Associated Legendre Polynomial Tests: 0.5")
{
    using namespace wmm_model;

    constexpr int nMax = 4;
    constexpr size_t N = (nMax + 1) * (nMax + 2) / 2;

    std::array<float, N> Pcuph{};
    std::array<float, N> Pcupl{};
    std::array<float, N> Pcup{};
    std::array<float, N> dPcup{};

    const float angle = 1.0472;
    const float delta = 1e-3f;
    bool result = false;
    result = MAG_PcupLow<nMax, N>(Pcuph, dPcup, sinf(angle + 0.5f * delta));
    result = MAG_PcupLow<nMax, N>(Pcupl, dPcup, sinf(angle - 0.5f * delta));
    result = MAG_PcupLow<nMax, N>(Pcup, dPcup, sinf(angle));
    CHECK(result == true);

    int k = 0;
    for (int i = 0; i <= nMax; ++i)
    {
        for (int j = 0; j <= i; ++j)
        {
            char name[128];
            sprintf(name, "n=%d, m=%d", i, j);
            SUBCASE(name)
            {
                CHECK((Pcuph[k]-Pcupl[k])/delta == doctest::Approx(dPcup[k]).epsilon(1e-3f)); // -expected to convert to latitude derivative
            }
            ++k;
        }
    }
}

TEST_CASE("Derivative Associated Legendre Polynomial Tests: -0.5")
{
    using namespace wmm_model;

    constexpr int nMax = 4;
    constexpr size_t N = (nMax + 1) * (nMax + 2) / 2;

    std::array<float, N> Pcuph{};
    std::array<float, N> Pcupl{};
    std::array<float, N> Pcup{};
    std::array<float, N> dPcup{};

    const float angle = -1.0472;
    const float delta = 1e-3f;
    bool result = false;
    result = MAG_PcupLow<nMax, N>(Pcuph, dPcup, sinf(angle + 0.5f * delta));
    result = MAG_PcupLow<nMax, N>(Pcupl, dPcup, sinf(angle - 0.5f * delta));
    result = MAG_PcupLow<nMax, N>(Pcup, dPcup, sinf(angle));
    CHECK(result == true);

    int k = 0;
    for (int i = 0; i <= nMax; ++i)
    {
        for (int j = 0; j <= i; ++j)
        {
            char name[128];
            sprintf(name, "n=%d, m=%d", i, j);
            SUBCASE(name)
            {
                CHECK((Pcuph[k]-Pcupl[k])/delta == doctest::Approx(dPcup[k]).epsilon(1e-3f)); // -expected to convert to latitude derivative
            }
            ++k;
        }
    }
}

TEST_CASE("Derivative Associated Legendre Polynomial Tests MAG_PcupHigh: 0.5")
{
    using namespace wmm_model;

    constexpr int nMax = 4;
    constexpr size_t N = (nMax + 1) * (nMax + 2) / 2;

    std::array<float, N> Pcuph{};
    std::array<float, N> Pcupl{};
    std::array<float, N> Pcup{};
    std::array<float, N> dPcup{};

    const float angle = 1.0472;
    const float delta = 1e-3f;
    bool result = false;
    result = MAG_PcupHigh<nMax, N>(Pcuph, dPcup, sinf(angle + 0.5f * delta));
    result = MAG_PcupHigh<nMax, N>(Pcupl, dPcup, sinf(angle - 0.5f * delta));
    result = MAG_PcupHigh<nMax, N>(Pcup, dPcup, sinf(angle));
    CHECK(result == true);

    int k = 0;
    for (int i = 0; i <= nMax; ++i)
    {
        for (int j = 0; j <= i; ++j)
        {
            char name[128];
            sprintf(name, "n=%d, m=%d", i, j);
            SUBCASE(name)
            {
                CHECK((Pcuph[k]-Pcupl[k])/delta == doctest::Approx(dPcup[k]).epsilon(1e-3f)); // -expected to convert to latitude derivative
            }
            ++k;
        }
    }
}

