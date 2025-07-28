#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "coordinate_transformations.hpp"
#include "TimeUtils.hpp"
#include <random>
#include <limits> // For NaN
#include <cmath>  // For std::isnan

using namespace coordinate_transformations;

uint32_t getExponent(float f)
{
    if (std::isnan(f) || std::isinf(f))
    {
        return std::numeric_limits<int32_t>::quiet_NaN(); // Or some other appropriate value for special cases
    }

    uint32_t floatBits = 0;
    static_assert(sizeof(float) == sizeof(uint32_t), "Size mismatch between float and uint32_t");
    std::memcpy(&floatBits, &f, sizeof(float)); // Copy the float's bits to the integer

    // Extract the exponent bits (bits 23-30) and shift them to the right
    uint32_t exponentBits = (floatBits >> 23) & 0xFF;
    return exponentBits;

    //   // Subtract the bias (127) to get the actual exponent
    //   int32_t exponent = static_cast<int32_t>(exponentBits) - 127;
    //   return exponent;
}

float resolution(float f)
{
    uint32_t exponent = getExponent(f);
    if (std::isnan(f) || std::isinf(f))
    {
        return std::numeric_limits<float>::quiet_NaN(); // Or some other appropriate value for special cases
    }

    return std::pow(2.0f, static_cast<float>(exponent - 150));
}

TEST_CASE("ECEF to Geodetic Conversion - Iterative")
{
    SUBCASE("Equator, Sea Level")
    {
        ECEF ecef = {WGS84_A, 0.0, 0.0};
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(0.0).epsilon(1e-8));
        CHECK(geodetic.longitude_deg == doctest::Approx(0.0).epsilon(1e-8));
        CHECK(geodetic.height_m == doctest::Approx(0.0).epsilon(1e-3));
    }

    SUBCASE("Equator, Sea Level but somewhere else")
    {
        ECEF ecef = {WGS84_A / sqrtf(2.f), WGS84_A / sqrtf(2.f), 0.0};
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(0.0).epsilon(1e-8));
        CHECK(geodetic.longitude_deg == doctest::Approx(45.0).epsilon(1e-8));
        CHECK(geodetic.height_m == doctest::Approx(0.0).epsilon(1e-3));
    }

    SUBCASE("Equator, Sea Level + 1000")
    {
        ECEF ecef = {WGS84_A + 1000.f, 0.0, 0.0};
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(0.0).epsilon(1e-8));
        CHECK(geodetic.longitude_deg == doctest::Approx(0.0).epsilon(1e-8));
        CHECK(geodetic.height_m == doctest::Approx(1000.0).epsilon(1e-3));
    }

    SUBCASE("Equator, Sea Level - 10000")
    {
        ECEF ecef = {WGS84_A - 10000.f, 0.0, 0.0};
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(0.0).epsilon(1e-8));
        CHECK(geodetic.longitude_deg == doctest::Approx(0.0).epsilon(1e-8));
        CHECK(geodetic.height_m == doctest::Approx(-10000.0).epsilon(1e-3));
    }

    SUBCASE("North Pole, Sea Level")
    {
        ECEF ecef = {0.0, 0.0, WGS84_B};
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(90.0).epsilon(1e-8));
        CHECK(geodetic.longitude_deg == doctest::Approx(0.0).epsilon(1e-8));
        CHECK(geodetic.height_m == doctest::Approx(0.0).epsilon(1e-3));
    }

    SUBCASE("North Pole, Sea Level + 12000")
    {
        ECEF ecef = {0.0, 0.0, WGS84_B + 12000.0f};
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(90.0).epsilon(1e-8));
        CHECK(geodetic.longitude_deg == doctest::Approx(0.0).epsilon(1e-8));
        CHECK(geodetic.height_m == doctest::Approx(12000.0).epsilon(1e-3));
    }

    SUBCASE("North Pole, Sea Level - 4000")
    {
        ECEF ecef = {0.0, 0.0, WGS84_B - 4000.0f};
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(90.0).epsilon(1e-8));
        CHECK(geodetic.longitude_deg == doctest::Approx(0.0).epsilon(1e-8));
        CHECK(geodetic.height_m == doctest::Approx(-4000.0).epsilon(1e-3));
    }

    SUBCASE("45 degrees Latitude, 30 degrees Longitude, 100km Altitude")
    {
        Geodetic initial_geodetic = {45.0, 30.0, 100000.0};
        ECEF ecef = geodeticToECEF(initial_geodetic);
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(45.0).epsilon(1e-4));
        CHECK(geodetic.longitude_deg == doctest::Approx(30.0).epsilon(1e-4));
        CHECK(geodetic.height_m == doctest::Approx(100000.0).epsilon(1e-3));
    }

    SUBCASE("Negative Latitude and Longitude, High Altitude")
    {
        Geodetic initial_geodetic = {-30.0, -60.0, 100000.0};
        ECEF ecef = geodeticToECEF(initial_geodetic);
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(-30.0).epsilon(1e-4));
        CHECK(geodetic.longitude_deg == doctest::Approx(-60.0).epsilon(1e-4));
        CHECK(geodetic.height_m == doctest::Approx(100000.0).epsilon(1e-3));
    }
}

TEST_CASE("Geodetic to Geocentric Conversion")
{
    SUBCASE("Equator, Sea Level")
    {
        Geodetic geodetic = {0.0f, 0.0f, 0.0f};
        Geocentric geocentric = geodeticToGeocentric(geodetic);
        CHECK(geocentric.latitude_deg == doctest::Approx(0.0f).epsilon(1e-6f));
        CHECK(geocentric.longitude_deg == doctest::Approx(0.0f).epsilon(1e-6f));
        CHECK(geocentric.radius_m == doctest::Approx(WGS84_A).epsilon(1e-6f));
    }

    SUBCASE("North Pole, Sea Level")
    {
        Geodetic geodetic = {90.0f, 0.0f, 0.0f};
        Geocentric geocentric = geodeticToGeocentric(geodetic);
        CHECK(geocentric.latitude_deg == doctest::Approx(90.0f).epsilon(1e-6f));
        CHECK(geocentric.longitude_deg == doctest::Approx(0.0f).epsilon(1e-6f));
        CHECK(geocentric.radius_m == doctest::Approx(WGS84_B).epsilon(1e-6f));
    }

    SUBCASE("South Pole, Sea Level")
    {
        Geodetic geodetic = {-90.0f, 0.0f, 0.0f};
        Geocentric geocentric = geodeticToGeocentric(geodetic);
        CHECK(geocentric.latitude_deg == doctest::Approx(-90.0f).epsilon(1e-6f));
        CHECK(geocentric.longitude_deg == doctest::Approx(0.0f).epsilon(1e-6f));
        CHECK(geocentric.radius_m == doctest::Approx(WGS84_B).epsilon(1e-6f));
    }

    SUBCASE("45 degrees Latitude, 30 degrees Longitude, 100km Altitude")
    {
        Geodetic geodetic = {45.0f, 30.0f, 100000.0f}; // 100km == 100000m
        Geocentric geocentric = geodeticToGeocentric(geodetic);
        CHECK(geocentric.radius_m > WGS84_A);
        CHECK(geocentric.longitude_deg == doctest::Approx(30.0f).epsilon(1e-6f));
    }

    SUBCASE("Negative Latitude and Longitude, High Altitude")
    {
        Geodetic geodetic = {-30.0f, -60.0f, 100000.0f}; // 100km == 100000m
        Geocentric geocentric = geodeticToGeocentric(geodetic);
        CHECK(geocentric.radius_m > WGS84_A);
        CHECK(geocentric.longitude_deg == doctest::Approx(-60.0f).epsilon(1e-6f));
    }

    SUBCASE("Invalid Latitude - Returns NaN")
    {
        Geodetic geodetic = {100.0f, 0.0f, 0.0f};
        Geocentric geocentric = geodeticToGeocentric(geodetic);
        CHECK(std::isnan(geocentric.latitude_deg));
        CHECK(std::isnan(geocentric.longitude_deg));
        CHECK(std::isnan(geocentric.radius_m));
    }

    SUBCASE("Large Positive Height")
    {
        Geodetic geodetic = {45.0f, 30.0f, 10000000.0f};        // 10,000 km
        Geocentric geocentric = geodeticToGeocentric(geodetic); // Should still be valid
        CHECK(std::isfinite(geocentric.latitude_deg));
        CHECK(std::isfinite(geocentric.longitude_deg));
        CHECK(std::isfinite(geocentric.radius_m));
    }

    SUBCASE("Large Negative Height (Below Ellipsoid)")
    {
        Geodetic geodetic = {45.0f, 30.0f, -10000.0f};          // -10km
        Geocentric geocentric = geodeticToGeocentric(geodetic); // Still valid
        CHECK(std::isfinite(geocentric.latitude_deg));
        CHECK(std::isfinite(geocentric.longitude_deg));
        CHECK(std::isfinite(geocentric.radius_m));
    }

    SUBCASE("Longitude at 180")
    {
        Geodetic geodetic = {0.0f, 180.0f, 0.0f};
        Geocentric geocentric = geodeticToGeocentric(geodetic);
        CHECK(geocentric.latitude_deg == doctest::Approx(0.0f).epsilon(1e-6f));
        CHECK(geocentric.longitude_deg == doctest::Approx(180.0f).epsilon(1e-6f));
    }

    SUBCASE("Longitude at -180")
    {
        Geodetic geodetic = {0.0f, -180.0f, 0.0f};
        Geocentric geocentric = geodeticToGeocentric(geodetic);
        CHECK(geocentric.latitude_deg == doctest::Approx(0.0f).epsilon(1e-6f));
        CHECK(geocentric.longitude_deg == doctest::Approx(-180.0f).epsilon(1e-6f));
    }
}

TEST_CASE("Geocentric to Geodetic Conversion")
{
    SUBCASE("Earth Center - Returns NaN")
    {
        Geocentric geocentric = {0.0f, 0.0f, 0.0f}; // Invalid as there is no coordinate for this location.
        Geodetic geodetic = geocentricToGeodetic(geocentric);
        CHECK(std::isnan(geodetic.latitude_deg));
        CHECK(std::isnan(geodetic.longitude_deg));
        CHECK(std::isnan(geodetic.height_m));
    }

    SUBCASE("Equator")
    {
        Geocentric geocentric = {0.0f, 0.0f, WGS84_A};

        // Calculate radius
        float x = geocentric.radius_m * cosf(geocentric.latitude_deg) * cosf(geocentric.longitude_deg);
        float y = geocentric.radius_m * cosf(geocentric.latitude_deg) * sinf(geocentric.longitude_deg);
        float z = geocentric.radius_m * sinf(geocentric.latitude_deg);
        geocentric.radius_m = sqrtf(x * x + y * y + z * z);

        Geodetic geodetic = geocentricToGeodetic(geocentric);
        CHECK(geodetic.latitude_deg == doctest::Approx(0.0f).epsilon(1e-6f));
        CHECK(geodetic.longitude_deg == doctest::Approx(0.0f).epsilon(1e-6f));
        CHECK(geodetic.height_m == doctest::Approx(-WGS84_A + geocentric.radius_m).epsilon(1e-6f));
    }

    SUBCASE("Equator With Longitude")
    {
        Geocentric geocentric = {0.0f, 30.0f, WGS84_A};
        // Calculate radius
        float x = geocentric.radius_m * cosf(geocentric.latitude_deg) * cosf(geocentric.longitude_deg);
        float y = geocentric.radius_m * cosf(geocentric.latitude_deg) * sinf(geocentric.longitude_deg);
        float z = geocentric.radius_m * sinf(geocentric.latitude_deg);
        geocentric.radius_m = sqrtf(x * x + y * y + z * z);

        Geodetic geodetic = geocentricToGeodetic(geocentric);
        CHECK(geodetic.latitude_deg == doctest::Approx(0.0f).epsilon(1e-6f));
        CHECK(geodetic.longitude_deg == doctest::Approx(30.0f).epsilon(1e-6f));
        CHECK(geodetic.height_m == doctest::Approx(-WGS84_A + geocentric.radius_m).epsilon(1e-6f));
    }

    SUBCASE("Non-Zero Geocentric Latitude")
    {
        Geocentric geocentric = {30.0f, 45.0f, 5000000.0f}; // radius in meters
        Geodetic geodetic = geocentricToGeodetic(geocentric);
        // Further checks here would be ideal if you have reference values
        CHECK(std::isfinite(geodetic.latitude_deg));
        CHECK(std::isfinite(geodetic.longitude_deg));
        CHECK(std::isfinite(geodetic.height_m));
    }
    SUBCASE("Geocentric Latitude near 90 - Radiius zero")
    {
        Geocentric geocentric = {89.99999f, 45.0f, 0.0000001f}; // radius in meters

        Geodetic geodetic = geocentricToGeodetic(geocentric);
        // Should Return NAN if it cannot handle these values.
        CHECK(std::isfinite(geodetic.latitude_deg));
        CHECK(std::isfinite(geodetic.longitude_deg));
        CHECK(std::isfinite(geodetic.height_m));
    }
}

TEST_CASE("Geodetic to ECEF Conversion")
{
    SUBCASE("Equator, Sea Level")
    {
        Geodetic geodetic = {0.0f, 0.0f, 0.0f};
        ECEF ecef = geodeticToECEF(geodetic);
        CHECK(ecef.x_m == doctest::Approx(WGS84_A).epsilon(1e-5f));
        CHECK(ecef.y_m == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(ecef.z_m == doctest::Approx(0.0f).epsilon(1e-5f));
    }

    SUBCASE("North Pole, Sea Level")
    {
        Geodetic geodetic = {90.0f, 0.0f, 0.0f};
        ECEF ecef = geodeticToECEF(geodetic);
        CHECK(ecef.x_m == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(ecef.y_m == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(ecef.z_m == doctest::Approx(WGS84_B).epsilon(1e-5f));
    }

    SUBCASE("Invalid Latitude - Returns NaN")
    {
        Geodetic geodetic = {100.0f, 0.0f, 0.0f};
        ECEF ecef = geodeticToECEF(geodetic);
        CHECK(std::isnan(ecef.x_m));
        CHECK(std::isnan(ecef.y_m));
        CHECK(std::isnan(ecef.z_m));
    }

    SUBCASE("Prime Meridian, Sea Level")
    {
        Geodetic geodetic = {0.0f, 0.0f, 0.0f};
        ECEF ecef = geodeticToECEF(geodetic);
        CHECK(ecef.x_m == doctest::Approx(WGS84_A).epsilon(1e-5f));
        CHECK(ecef.y_m == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(ecef.z_m == doctest::Approx(0.0f).epsilon(1e-5f));
    }

    SUBCASE("High Altitude")
    {
        Geodetic geodetic = {45.0f, 45.0f, 1000000.0f}; // 1000km above surface
        ECEF ecef = geodeticToECEF(geodetic);
        CHECK(std::isfinite(ecef.x_m));
        CHECK(std::isfinite(ecef.y_m));
        CHECK(std::isfinite(ecef.z_m));
    }

    SUBCASE("Longitude near 180")
    {
        Geodetic geodetic = {0.0f, 179.9999f, 0.0f};
        ECEF ecef = geodeticToECEF(geodetic);
        CHECK(!std::isnan(ecef.x_m));
        CHECK(!std::isnan(ecef.y_m));
        CHECK(!std::isnan(ecef.z_m));
    }
}

TEST_CASE("ECEF to Geodetic Conversion")
{
    SUBCASE("Equator, Sea Level")
    {
        ECEF ecef = {WGS84_A, 0.0f, 0.0f};
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(geodetic.longitude_deg == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(geodetic.height_m == doctest::Approx(0.0f).epsilon(1e-5f));
    }

    SUBCASE("North Pole, Sea Level")
    {
        ECEF ecef = {0.0f, 0.0f, WGS84_B};
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(90.0f).epsilon(1e-5f));
        CHECK(geodetic.longitude_deg == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(geodetic.height_m == doctest::Approx(0.0f).epsilon(1e-5f));
    }

    SUBCASE("45 degrees Latitude, 30 degrees Longitude, 100km Altitude")
    {
        // First, convert from geodetic to ECEF to get the expected ECEF values
        Geodetic initial_geodetic = {45.0f, 30.0f, 100000.0f};
        ECEF ecef = geodeticToECEF(initial_geodetic);

        // Then, convert back from ECEF to geodetic and check the values
        Geodetic final_geodetic = ecefToGeodetic(ecef);

        CHECK(final_geodetic.latitude_deg == doctest::Approx(45.0f).epsilon(1e-4f));
        CHECK(final_geodetic.longitude_deg == doctest::Approx(30.0f).epsilon(1e-4f));
        CHECK(final_geodetic.height_m == doctest::Approx(100000.0f).epsilon(1e-4f));
    }

    SUBCASE("Close to non-convergence")
    {
        ECEF ecef = {1.0, 2.0, 3.0};
        Geodetic geodetic = ecefToGeodetic(ecef);

        INFO("geodetic.latitude_deg ", geodetic.latitude_deg);
        INFO("geodetic.longitude_deg ", geodetic.longitude_deg);
        INFO("geodetic.height_m ", geodetic.height_m);

        CHECK_FALSE(std::isnan(geodetic.latitude_deg));
        CHECK_FALSE(std::isnan(geodetic.longitude_deg));
        CHECK_FALSE(std::isnan(geodetic.height_m));
    }

    SUBCASE("Z axis close to zero")
    {
        ECEF ecef = {6378137.0f, 0.0f, 0.0f};
        Geodetic geodetic = ecefToGeodetic(ecef);
        CHECK(geodetic.latitude_deg == doctest::Approx(0.0f).epsilon(1e-5f));
    }
}

TEST_CASE("Round-Trip Geodetic -> ECEF -> Geodetic")
{
    SUBCASE("Positive Values")
    {
        Geodetic initial_geodetic = {45.0f, -120.0f, 500.0f};
        ECEF ecef = geodeticToECEF(initial_geodetic);
        Geodetic final_geodetic = ecefToGeodetic(ecef);
        CHECK(final_geodetic.latitude_deg == doctest::Approx(initial_geodetic.latitude_deg).epsilon(1e-4f));
        CHECK(final_geodetic.longitude_deg == doctest::Approx(initial_geodetic.longitude_deg).epsilon(1e-4f));
        CHECK(final_geodetic.height_m == doctest::Approx(initial_geodetic.height_m).epsilon(1e-4f));
    }

    SUBCASE("Negative Values and Sea Level")
    {
        Geodetic initial_geodetic = {-30.0f, 60.0f, 0.0f};
        ECEF ecef = geodeticToECEF(initial_geodetic);
        Geodetic final_geodetic = ecefToGeodetic(ecef);

        CHECK(final_geodetic.latitude_deg == doctest::Approx(initial_geodetic.latitude_deg).epsilon(1e-4f));
        CHECK(final_geodetic.longitude_deg == doctest::Approx(initial_geodetic.longitude_deg).epsilon(1e-4f));
        CHECK(final_geodetic.height_m == doctest::Approx(initial_geodetic.height_m).epsilon(1e-0f));
    }

    SUBCASE("Positive Values and Sea Level")
    {
        Geodetic initial_geodetic = {30.0f, 60.0f, 0.0f};
        ECEF ecef = geodeticToECEF(initial_geodetic);
        Geodetic final_geodetic = ecefToGeodetic(ecef);
        CHECK(final_geodetic.latitude_deg == doctest::Approx(initial_geodetic.latitude_deg).epsilon(1e-4f));
        CHECK(final_geodetic.longitude_deg == doctest::Approx(initial_geodetic.longitude_deg).epsilon(1e-4f));
        CHECK(final_geodetic.height_m == doctest::Approx(initial_geodetic.height_m).epsilon(1e-0f));
    }

    SUBCASE("Near-Singularity Case (Equator, Height = -6300000)")
    {
        // This tests a case where the point is very close to the center of the Earth,
        Geodetic initial_geodetic = {0.0f, 0.0f, -6300000.f};
        ECEF ecef = geodeticToECEF(initial_geodetic);
        Geodetic final_geodetic = ecefToGeodetic(ecef);

        CHECK(final_geodetic.latitude_deg == doctest::Approx(initial_geodetic.latitude_deg).epsilon(1e-3f));
        CHECK(final_geodetic.longitude_deg == doctest::Approx(initial_geodetic.longitude_deg).epsilon(1e-3f));
        CHECK(final_geodetic.height_m == doctest::Approx(initial_geodetic.height_m).epsilon(1e-3f));
    }

    SUBCASE("Near-Singularity Case (Equator, Height = -N)")
    {
        // This tests a case where the point is very close to the center of the Earth,
        // but still technically valid.
        Geodetic initial_geodetic = {0.0f, 0.0f, -6356752.314245f}; // Height = -N, where N is the radius of curvature
        ECEF ecef = geodeticToECEF(initial_geodetic);
        Geodetic final_geodetic = ecefToGeodetic(ecef);

        INFO("geodetic.latitude_deg ", final_geodetic.latitude_deg);
        INFO("geodetic.longitude_deg ", final_geodetic.longitude_deg);
        INFO("geodetic.height_m ", final_geodetic.height_m);

        CHECK(final_geodetic.latitude_deg == doctest::Approx(initial_geodetic.latitude_deg).epsilon(1e-3f));
        CHECK(final_geodetic.longitude_deg == doctest::Approx(initial_geodetic.longitude_deg).epsilon(1e-3f));
        CHECK(final_geodetic.height_m == doctest::Approx(initial_geodetic.height_m).epsilon(1e-3f));
    }
}

TEST_CASE("Round-Trip Conversion")
{
    SUBCASE("Positive Values")
    {
        Geodetic initial_geodetic = {45.0f, -120.0f, 500.0f};
        Geocentric geocentric = geodeticToGeocentric(initial_geodetic);
        Geodetic final_geodetic = geocentricToGeodetic(geocentric);

        CHECK(final_geodetic.latitude_deg == doctest::Approx(initial_geodetic.latitude_deg).epsilon(1e-5f));
        CHECK(final_geodetic.longitude_deg == doctest::Approx(initial_geodetic.longitude_deg).epsilon(1e-5f));
        CHECK(final_geodetic.height_m == doctest::Approx(initial_geodetic.height_m).epsilon(1e-5f));
    }

    SUBCASE("Negative Values and Sea Level")
    {
        Geodetic initial_geodetic = {-30.0f, 60.0f, 0.0f};
        Geocentric geocentric = geodeticToGeocentric(initial_geodetic);
        Geodetic final_geodetic = geocentricToGeodetic(geocentric);

        CHECK(final_geodetic.latitude_deg == doctest::Approx(initial_geodetic.latitude_deg).epsilon(1e-5f));
        CHECK(final_geodetic.longitude_deg == doctest::Approx(initial_geodetic.longitude_deg).epsilon(1e-5f));
        CHECK(final_geodetic.height_m == doctest::Approx(initial_geodetic.height_m).epsilon(1e-5f));
    }
}

TEST_CASE("Random Geodetic to Geocentric")
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> lat_dist(-90.0f, 90.0f);
    std::uniform_real_distribution<float> lon_dist(-180.0f, 180.0f);
    std::uniform_real_distribution<float> alt_dist(0.0f, 1000.0f); // Altitude in km

    for (int i = 0; i < 50; ++i)
    {
        Geodetic initial_geodetic = {lat_dist(gen), lon_dist(gen), alt_dist(gen) * 1000.0f}; // Altitude in meters
        Geocentric geocentric = geodeticToGeocentric(initial_geodetic);

        INFO("initial_geodetic.latitude_deg ", initial_geodetic.latitude_deg);
        INFO("initial_geodetic.longitude_deg ", initial_geodetic.longitude_deg);
        INFO("initial_geodetic.height_m ", initial_geodetic.height_m);

        CHECK_FALSE(doctest::IsNaN(geocentric.latitude_deg));
        CHECK_FALSE(doctest::IsNaN(geocentric.longitude_deg));
        CHECK_FALSE(doctest::IsNaN(geocentric.radius_m));
    }
}

TEST_CASE("Random Geodetic to Geocentric and Back")
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> lat_dist(-90.0f, 90.0f);
    std::uniform_real_distribution<float> lon_dist(-180.0f, 180.0f);
    std::uniform_real_distribution<float> alt_dist(0.0f, 1000.0f); // Altitude in km

    for (int i = 0; i < 50; ++i)
    {
        Geodetic initial_geodetic = {lat_dist(gen), lon_dist(gen), alt_dist(gen) * 1000.0f}; // Altitude in meters
        Geocentric geocentric = geodeticToGeocentric(initial_geodetic);
        Geodetic final_geodetic = geocentricToGeodetic(geocentric);

        CHECK(final_geodetic.latitude_deg == doctest::Approx(initial_geodetic.latitude_deg).epsilon(0.1f));
        CHECK(final_geodetic.longitude_deg == doctest::Approx(initial_geodetic.longitude_deg).epsilon(0.1f));
        CHECK(final_geodetic.height_m == doctest::Approx(initial_geodetic.height_m).epsilon(1000.0f));
    }
}

TEST_CASE("Random Geocentric to Geodetic")
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> lat_dist(-90.0f, 90.0f);
    std::uniform_real_distribution<float> lon_dist(-180.0f, 180.0f);
    std::uniform_real_distribution<float> rad_dist(6470.0f, 7470.0f); // radius in km

    for (int i = 0; i < 50; ++i)
    {
        Geocentric initial_geocentric = {lat_dist(gen), lon_dist(gen), rad_dist(gen) * 1000.0f}; // Altitude in meters
        Geodetic geodetic = geocentricToGeodetic(initial_geocentric);

        INFO("initial_geocentric.latitude_deg ", initial_geocentric.latitude_deg);
        INFO("initial_geocentric.longitude_deg ", initial_geocentric.longitude_deg);
        INFO("initial_geocentric.radius_m ", initial_geocentric.radius_m);

        CHECK_FALSE(doctest::IsNaN(geodetic.latitude_deg));
        CHECK_FALSE(doctest::IsNaN(geodetic.longitude_deg));
        CHECK_FALSE(doctest::IsNaN(geodetic.height_m));
    }
}

TEST_CASE("Random Geocentric to Geodetic and Back")
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> lat_dist(-90.0f, 90.0f);
    std::uniform_real_distribution<float> lon_dist(-180.0f, 180.0f);
    std::uniform_real_distribution<float> rad_dist(6470.0f, 7470.0f); // radius in km

    for (int i = 0; i < 50; ++i)
    {
        Geocentric initial_geocentric = {lat_dist(gen), lon_dist(gen), rad_dist(gen) * 1000.0f}; // Altitude in meters
        Geodetic geodetic = geocentricToGeodetic(initial_geocentric);
        Geocentric final_geocentric = geodeticToGeocentric(geodetic);

        CHECK(final_geocentric.latitude_deg == doctest::Approx(initial_geocentric.latitude_deg).epsilon(0.1f));
        CHECK(final_geocentric.longitude_deg == doctest::Approx(initial_geocentric.longitude_deg).epsilon(0.1f));
        CHECK(final_geocentric.radius_m == doctest::Approx(initial_geocentric.radius_m).epsilon(1000.0f));
    }
}

TEST_CASE("Random Geodetic to ECEF and Back")
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> lat_dist(-90.0f, 90.0f);
    std::uniform_real_distribution<float> lon_dist(-180.0f, 180.0f);
    std::uniform_real_distribution<float> alt_dist(0.0f, 1000.0f);

    for (int i = 0; i < 50; ++i)
    {
        Geodetic initial_geodetic = {lat_dist(gen), lon_dist(gen), alt_dist(gen) * 1000.0f}; // Altitude in meters
        ECEF ecef = geodeticToECEF(initial_geodetic);
        Geodetic final_geodetic = ecefToGeodetic(ecef);

        INFO("initial_geodetic.latitude_deg ", initial_geodetic.latitude_deg);
        INFO("initial_geodetic.longitude_deg ", initial_geodetic.longitude_deg);
        INFO("initial_geodetic.height_m ", initial_geodetic.height_m);
        INFO("ecef.x_m, ", ecef.x_m);
        INFO("ecef.y_m, ", ecef.y_m);
        INFO("ecef.z_m, ", ecef.z_m);

        CHECK(final_geodetic.latitude_deg == doctest::Approx(initial_geodetic.latitude_deg).epsilon(0.1f));
        CHECK(final_geodetic.longitude_deg == doctest::Approx(initial_geodetic.longitude_deg).epsilon(0.1f));
        CHECK(final_geodetic.height_m == doctest::Approx(initial_geodetic.height_m).epsilon(1000.0f));
    }
}

// Add new tests for temeToECEF and ecefToTEME
TEST_CASE("TEME to ECEF and ECEF to TEME Conversions")
{
    // Define a Julian Date for the conversion
    //  float jdut1 = 2458863.0f; // Example Julian Date
    constexpr float jdut1 = 9336.0f; // Example J2000 Date
    constexpr float eps = 0.25f;

    SUBCASE("Basic Conversion Test")
    {
        // Define a TEME coordinate (example)
        TEME teme_coord = {7000000.0f, 0.0f, 0.0f}; // Example position in meters

        // Convert TEME to ECEF
        ECEF ecef_coord = temeToECEF(teme_coord, jdut1);

        // Now convert back from ECEF to TEME
        TEME recovered_teme_coord = ecefToTEME(ecef_coord, jdut1);

        // Check if the coordinates are approximately equal
        CHECK(recovered_teme_coord.x_m == doctest::Approx(teme_coord.x_m).epsilon(eps));
        CHECK(recovered_teme_coord.y_m == doctest::Approx(teme_coord.y_m).epsilon(eps));
        CHECK(recovered_teme_coord.z_m == doctest::Approx(teme_coord.z_m).epsilon(eps));
    }

    SUBCASE("Another TEME Coordinate Test")
    {
        // Define a different TEME coordinate
        TEME teme_coord = {0.0f, 7000000.0f, 0.0f};

        // Convert TEME to ECEF
        ECEF ecef_coord = temeToECEF(teme_coord, jdut1);

        // Convert back from ECEF to TEME
        TEME recovered_teme_coord = ecefToTEME(ecef_coord, jdut1);

        // Check for approximate equality
        CHECK(recovered_teme_coord.x_m == doctest::Approx(teme_coord.x_m).epsilon(eps));
        CHECK(recovered_teme_coord.y_m == doctest::Approx(teme_coord.y_m).epsilon(eps));
        CHECK(recovered_teme_coord.z_m == doctest::Approx(teme_coord.z_m).epsilon(eps));
    }

    SUBCASE("TEME Coordinate with Z Component")
    {
        // Define a different TEME coordinate
        TEME teme_coord = {1000000.0f, -1000000.0f, 5000000.0f};

        // Convert TEME to ECEF
        ECEF ecef_coord = temeToECEF(teme_coord, jdut1);

        // Convert back from ECEF to TEME
        TEME recovered_teme_coord = ecefToTEME(ecef_coord, jdut1);

        // Check for approximate equality
        CHECK(recovered_teme_coord.x_m == doctest::Approx(teme_coord.x_m).epsilon(eps));
        CHECK(recovered_teme_coord.y_m == doctest::Approx(teme_coord.y_m).epsilon(eps));
        CHECK(recovered_teme_coord.z_m == doctest::Approx(teme_coord.z_m).epsilon(eps));
    }

    SUBCASE("ECEF to TEME - Zero values, to test inverse polarm")
    {
        ECEF ecef_coord = {0.0f, 0.0f, 0.0f}; // Example position in meters
        TEME recovered_teme_coord = ecefToTEME(ecef_coord, jdut1);
        CHECK(recovered_teme_coord.x_m == doctest::Approx(ecef_coord.x_m).epsilon(eps));
        CHECK(recovered_teme_coord.y_m == doctest::Approx(ecef_coord.y_m).epsilon(eps));
        CHECK(recovered_teme_coord.z_m == doctest::Approx(ecef_coord.z_m).epsilon(eps));
    }
}

TEST_CASE("TEME to ECEF")
{
    auto j2000 = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
        .year = 2000,
        .month = 1,
        .day = 1,
        .hour = 12,
        .minute = 0,
        .second = 0,
        .millisecond = 0});

    constexpr float eps = 1.0e-1f;

    SUBCASE("2025/06/25 18:00:00")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 6,
            .day = 25,
            .hour = 18,
            .minute = 0,
            .second = 0,
            .millisecond = 0});

        float jd2000 = TimeUtils::to_fractional_days(j2000, now);

        TEME teme_coord = {-3006.1573609732827f * 1000.0f, 4331.221049310724f * 1000.0f, -4290.439626312989f * 1000.0f};
        ECEF ecef_coord = temeToECEF(teme_coord, jd2000);

        CHECK(ecef_coord.x_m == doctest::Approx( 2686.63188566f * 1000.0f).epsilon(eps));
        CHECK(ecef_coord.y_m == doctest::Approx(-4536.33846792f * 1000.0f).epsilon(eps));
        CHECK(ecef_coord.z_m == doctest::Approx(-4290.45131185f * 1000.0f).epsilon(eps));
    }


    SUBCASE("2025/07/06 20:43:13")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 7,
            .day = 6,
            .hour = 20,
            .minute = 43,
            .second = 13,
            .millisecond = 0});

        float jd2000 = TimeUtils::to_fractional_days(j2000, now);

        TEME teme_coord = {-4813.398435775674f * 1000.0f, -4416.344248277559f * 1000.0f, 1857.5065466212982f * 1000.0f};
        ECEF ecef_coord = temeToECEF(teme_coord, jd2000);

        CHECK(ecef_coord.x_m == doctest::Approx( 6355.96709238f * 1000.0f).epsilon(eps));
        CHECK(ecef_coord.y_m == doctest::Approx(-1508.18261367f * 1000.0f).epsilon(eps));
        CHECK(ecef_coord.z_m == doctest::Approx( 1857.49807967f * 1000.0f).epsilon(eps));
    }
}

TEST_CASE("AU Coordinates TEME to ECEF")
{
    auto j2000 = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
        .year = 2000,
        .month = 1,
        .day = 1,
        .hour = 12,
        .minute = 0,
        .second = 0,
        .millisecond = 0});

    constexpr float eps = 1.0e-1f;

    SUBCASE("2025/06/25 18:00:00")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 6,
            .day = 25,
            .hour = 18,
            .minute = 0,
            .second = 0,
            .millisecond = 0});

        float jd2000 = TimeUtils::to_fractional_days(j2000, now);

        std::array<au::QuantityF<au::Kilo<au::MetersInTemeFrame>>,3> teme_coord = 
        {
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(-3006.1573609732827f), 
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(4331.221049310724f), 
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(-4290.439626312989f)
        };
        std::array<au::QuantityF<au::Kilo<au::MetersInEcefFrame>>,3> ecef_coord = temeToecef(teme_coord, jd2000);

        CHECK(ecef_coord[0].in(au::kilo(au::meters * au::ecefs)) == doctest::Approx( 2686.63188566f).epsilon(eps));
        CHECK(ecef_coord[1].in(au::kilo(au::meters * au::ecefs)) == doctest::Approx(-4536.33846792f).epsilon(eps));
        CHECK(ecef_coord[2].in(au::kilo(au::meters * au::ecefs)) == doctest::Approx(-4290.45131185f).epsilon(eps));
    }


    SUBCASE("2025/07/06 20:43:13")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 7,
            .day = 6,
            .hour = 20,
            .minute = 43,
            .second = 13,
            .millisecond = 0});

        float jd2000 = TimeUtils::to_fractional_days(j2000, now);

        std::array<au::QuantityF<au::Kilo<au::MetersInTemeFrame>>,3> teme_coord = 
        {
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(-4813.398435775674f),
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(-4416.344248277559f), 
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(1857.5065466212982f)
        };
        std::array<au::QuantityF<au::Kilo<au::MetersInEcefFrame>>,3> ecef_coord = temeToecef(teme_coord, jd2000);

        CHECK(ecef_coord[0].in(au::kilo(au::meters * au::ecefs)) == doctest::Approx( 6355.96709238f).epsilon(eps));
        CHECK(ecef_coord[1].in(au::kilo(au::meters * au::ecefs)) == doctest::Approx(-1508.18261367f).epsilon(eps));
        CHECK(ecef_coord[2].in(au::kilo(au::meters * au::ecefs)) == doctest::Approx( 1857.49807967f).epsilon(eps));
    }
}

TEST_CASE("AU Coordinates TEME to ECEF to TEME")
{
    auto j2000 = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
        .year = 2000,
        .month = 1,
        .day = 1,
        .hour = 12,
        .minute = 0,
        .second = 0,
        .millisecond = 0});

    constexpr float eps = 1.0e-1f;

    SUBCASE("2025/06/25 18:00:00")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 6,
            .day = 25,
            .hour = 18,
            .minute = 0,
            .second = 0,
            .millisecond = 0});

        float jd2000 = TimeUtils::to_fractional_days(j2000, now);

        std::array<au::QuantityF<au::Kilo<au::MetersInTemeFrame>>,3> teme_coord = 
        {
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(-3006.1573609732827f), 
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(4331.221049310724f), 
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(-4290.439626312989f)
        };
        std::array<au::QuantityF<au::Kilo<au::MetersInEcefFrame>>,3> ecef_coord = temeToecef(teme_coord, jd2000);
        std::array<au::QuantityF<au::Kilo<au::MetersInTemeFrame>>,3> result = ecefToteme(ecef_coord, jd2000);

        CHECK(result[0].in(au::kilo(au::meters * au::temes)) == doctest::Approx(teme_coord[0].in(au::kilo(au::meters * au::temes))).epsilon(eps));
        CHECK(result[1].in(au::kilo(au::meters * au::temes)) == doctest::Approx(teme_coord[1].in(au::kilo(au::meters * au::temes))).epsilon(eps));
        CHECK(result[2].in(au::kilo(au::meters * au::temes)) == doctest::Approx(teme_coord[2].in(au::kilo(au::meters * au::temes))).epsilon(eps));
    }


    SUBCASE("2025/07/06 20:43:13")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 7,
            .day = 6,
            .hour = 20,
            .minute = 43,
            .second = 13,
            .millisecond = 0});

        float jd2000 = TimeUtils::to_fractional_days(j2000, now);

        std::array<au::QuantityF<au::Kilo<au::MetersInTemeFrame>>,3> teme_coord = 
        {
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(-4813.398435775674f),
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(-4416.344248277559f), 
            au::make_quantity<au::Kilo<au::MetersInTemeFrame>>(1857.5065466212982f)
        };
        std::array<au::QuantityF<au::Kilo<au::MetersInEcefFrame>>,3> ecef_coord = temeToecef(teme_coord, jd2000);
        std::array<au::QuantityF<au::Kilo<au::MetersInTemeFrame>>,3> result = ecefToteme(ecef_coord, jd2000);

        CHECK(result[0].in(au::kilo(au::meters * au::temes)) == doctest::Approx(teme_coord[0].in(au::kilo(au::meters * au::temes))).epsilon(eps));
        CHECK(result[1].in(au::kilo(au::meters * au::temes)) == doctest::Approx(teme_coord[1].in(au::kilo(au::meters * au::temes))).epsilon(eps));
        CHECK(result[2].in(au::kilo(au::meters * au::temes)) == doctest::Approx(teme_coord[2].in(au::kilo(au::meters * au::temes))).epsilon(eps));
    }
}

TEST_CASE("AU Velocities TEME to ECEF")
{
    auto j2000 = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
        .year = 2000,
        .month = 1,
        .day = 1,
        .hour = 12,
        .minute = 0,
        .second = 0,
        .millisecond = 0});

    constexpr float eps = 1.0e-1f;

    SUBCASE("2025/06/25 18:00:00")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 6,
            .day = 25,
            .hour = 18,
            .minute = 0,
            .second = 0,
            .millisecond = 0});

        float jd2000 = TimeUtils::to_fractional_days(j2000, now);

        std::array<au::QuantityF<au::Kilo<au::MetersPerSecondInTemeFrame>>,3> teme_coord = 
        {
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(-3006.1573609732827f), 
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(4331.221049310724f), 
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(-4290.439626312989f)
        };
        std::array<au::QuantityF<au::Kilo<au::MetersPerSecondInEcefFrame>>,3> ecef_coord = temeToecef(teme_coord, jd2000);

        CHECK(ecef_coord[0].in(au::kilo(au::meters * au::ecefs / au::seconds)) == doctest::Approx( 2686.63188566f).epsilon(eps));
        CHECK(ecef_coord[1].in(au::kilo(au::meters * au::ecefs / au::seconds)) == doctest::Approx(-4536.33846792f).epsilon(eps));
        CHECK(ecef_coord[2].in(au::kilo(au::meters * au::ecefs / au::seconds)) == doctest::Approx(-4290.45131185f).epsilon(eps));
    }


    SUBCASE("2025/07/06 20:43:13")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 7,
            .day = 6,
            .hour = 20,
            .minute = 43,
            .second = 13,
            .millisecond = 0});

        float jd2000 = TimeUtils::to_fractional_days(j2000, now);

        std::array<au::QuantityF<au::Kilo<au::MetersPerSecondInTemeFrame>>,3> teme_coord = 
        {
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(-4813.398435775674f),
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(-4416.344248277559f), 
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(1857.5065466212982f)
        };
        std::array<au::QuantityF<au::Kilo<au::MetersPerSecondInEcefFrame>>,3> ecef_coord = temeToecef(teme_coord, jd2000);

        CHECK(ecef_coord[0].in(au::kilo(au::meters * au::ecefs / au::seconds)) == doctest::Approx( 6355.96709238f).epsilon(eps));
        CHECK(ecef_coord[1].in(au::kilo(au::meters * au::ecefs / au::seconds)) == doctest::Approx(-1508.18261367f).epsilon(eps));
        CHECK(ecef_coord[2].in(au::kilo(au::meters * au::ecefs / au::seconds)) == doctest::Approx( 1857.49807967f).epsilon(eps));
    }
}

TEST_CASE("AU Velocities TEME to ECEF to TEME")
{
    auto j2000 = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
        .year = 2000,
        .month = 1,
        .day = 1,
        .hour = 12,
        .minute = 0,
        .second = 0,
        .millisecond = 0});

    constexpr float eps = 1.0e-1f;

    SUBCASE("2025/06/25 18:00:00")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 6,
            .day = 25,
            .hour = 18,
            .minute = 0,
            .second = 0,
            .millisecond = 0});

        float jd2000 = TimeUtils::to_fractional_days(j2000, now);

        std::array<au::QuantityF<au::Kilo<au::MetersPerSecondInTemeFrame>>,3> teme_coord = 
        {
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(-3006.1573609732827f), 
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(4331.221049310724f), 
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(-4290.439626312989f)
        };
        std::array<au::QuantityF<au::Kilo<au::MetersPerSecondInEcefFrame>>,3> ecef_coord = temeToecef(teme_coord, jd2000);
        std::array<au::QuantityF<au::Kilo<au::MetersPerSecondInTemeFrame>>,3> result = ecefToteme(ecef_coord, jd2000);

        CHECK(result[0].in(au::kilo(au::meters * au::temes / au::seconds)) == doctest::Approx(teme_coord[0].in(au::kilo(au::meters * au::temes / au::seconds))).epsilon(eps));
        CHECK(result[1].in(au::kilo(au::meters * au::temes / au::seconds)) == doctest::Approx(teme_coord[1].in(au::kilo(au::meters * au::temes / au::seconds))).epsilon(eps));
        CHECK(result[2].in(au::kilo(au::meters * au::temes / au::seconds)) == doctest::Approx(teme_coord[2].in(au::kilo(au::meters * au::temes / au::seconds))).epsilon(eps));
    }


    SUBCASE("2025/07/06 20:43:13")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 7,
            .day = 6,
            .hour = 20,
            .minute = 43,
            .second = 13,
            .millisecond = 0});

        float jd2000 = TimeUtils::to_fractional_days(j2000, now);

        std::array<au::QuantityF<au::Kilo<au::MetersPerSecondInTemeFrame>>,3> teme_coord = 
        {
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(-4813.398435775674f),
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(-4416.344248277559f), 
            au::make_quantity<au::Kilo<au::MetersPerSecondInTemeFrame>>(1857.5065466212982f)
        };
        std::array<au::QuantityF<au::Kilo<au::MetersPerSecondInEcefFrame>>,3> ecef_coord = temeToecef(teme_coord, jd2000);
        std::array<au::QuantityF<au::Kilo<au::MetersPerSecondInTemeFrame>>,3> result = ecefToteme(ecef_coord, jd2000);

        CHECK(result[0].in(au::kilo(au::meters * au::temes / au::seconds)) == doctest::Approx(teme_coord[0].in(au::kilo(au::meters * au::temes / au::seconds))).epsilon(eps));
        CHECK(result[1].in(au::kilo(au::meters * au::temes / au::seconds)) == doctest::Approx(teme_coord[1].in(au::kilo(au::meters * au::temes / au::seconds))).epsilon(eps));
        CHECK(result[2].in(au::kilo(au::meters * au::temes / au::seconds)) == doctest::Approx(teme_coord[2].in(au::kilo(au::meters * au::temes / au::seconds))).epsilon(eps));
    }
}

TEST_CASE("Polar Motion")
{
    auto j2000 = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
        .year = 2000,
        .month = 1,
        .day = 1,
        .hour = 12,
        .minute = 0,
        .second = 0,
        .millisecond = 0});

    SUBCASE("2025 07 24 ")
    {
        // epoch for polar file
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2025,
            .month = 7,
            .day = 24,
            .hour = 0,
            .minute = 0,
            .second = 0,
            .millisecond = 0});

        float jdut2 = TimeUtils::to_fractional_days(j2000, now);
        float pm[3][3];
        auto polar_motion = polarmMJD2000(jdut2, pm);
        CHECK(polar_motion.x == doctest::Approx(0.19715f).epsilon(1e-1f));
        CHECK(polar_motion.y == doctest::Approx(0.43271f).epsilon(1e-1f));
    }

    SUBCASE("2026 01 01 ")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2026,
            .month = 1,
            .day = 1,
            .hour = 0,
            .minute = 0,
            .second = 0,
            .millisecond = 0});

        float jdut2 = TimeUtils::to_fractional_days(j2000, now);
        float pm[3][3];
        auto polar_motion = polarmMJD2000(jdut2, pm);
        CHECK(polar_motion.x == doctest::Approx(0.1091).epsilon(1e-1f));
        CHECK(polar_motion.y == doctest::Approx(0.3015).epsilon(1e-1f));
    }

    SUBCASE("2026 07 24 ")
    {
        auto now = TimeUtils::to_timepoint(TimeUtils::DateTimeComponents{
            .year = 2026,
            .month = 7,
            .day = 24,
            .hour = 0,
            .minute = 0,
            .second = 0,
            .millisecond = 0});

        float jdut2 = TimeUtils::to_fractional_days(j2000, now);
        float pm[3][3];
        auto polar_motion = polarmMJD2000(jdut2, pm);
        CHECK(polar_motion.x == doctest::Approx(0.2032f).epsilon(1e-1f));
        CHECK(polar_motion.y == doctest::Approx(0.4221f).epsilon(1e-1f));
    }
}
