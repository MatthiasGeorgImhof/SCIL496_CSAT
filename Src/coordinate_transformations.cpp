#include <cmath>
#include <limits>
#include <iostream>
#include "coordinate_transformations.hpp"

// Make sure to use the correct namespace
namespace coordinate_transformations
{

    constexpr float EPSILON = 1.0e-9f;
    constexpr int MAX_ITERATIONS = 2000;

    // --- Utility Function ---
    // Checks if two float values are approximately equal within a given tolerance
    bool approximatelyEqual(const float a, const float b, const float tolerance = 1e-6f)
    {
        return std::fabs(a - b) <= tolerance;
    }

    // --- Coordinate Conversion Functions (Using Structs) ---

    // Geodetic to ECEF
    ECEF geodeticToECEF(Geodetic geodetic)
    {
        ECEF ecef;

        if (geodetic.latitude_deg < -90.0f || geodetic.latitude_deg > 90.0f)
        {
            ecef.x_m = std::numeric_limits<float>::quiet_NaN();
            ecef.y_m = std::numeric_limits<float>::quiet_NaN();
            ecef.z_m = std::numeric_limits<float>::quiet_NaN();
            return ecef;
        }

        float lon_rad = geodetic.longitude_deg * DEG_TO_RAD;
        float lat_rad = geodetic.latitude_deg * DEG_TO_RAD;

        float N = WGS84_A / sqrtf(1 - WGS84_E2 * sinf(lat_rad) * sinf(lat_rad));

        // Explicitly handle North/South Pole case to avoid floating-point errors
        if (approximatelyEqual(std::fabs(geodetic.latitude_deg), 90.0f))
        {
            ecef.x_m = 0.0f;
            ecef.y_m = 0.0f;
            ecef.z_m = ((1 - WGS84_E2) * N + geodetic.height_m) * sinf(lat_rad);
        }
        else
        {
            ecef.x_m = (N + geodetic.height_m) * cosf(lat_rad) * cosf(lon_rad);
            ecef.y_m = (N + geodetic.height_m) * cosf(lat_rad) * sinf(lon_rad);
            ecef.z_m = ((1 - WGS84_E2) * N + geodetic.height_m) * sinf(lat_rad);
        }

        return ecef;
    }

    // ECEF to Geodetic
    Geodetic ecefToGeodetic(ECEF ecef)
    {
        Geodetic geodetic;
        
        float p = sqrtf(ecef.x_m * ecef.x_m + ecef.y_m * ecef.y_m);

        if (approximatelyEqual(p, 0.0))
        {
            // Special case: point lies on the Z-axis, poles
            geodetic.longitude_deg = 0.0;
            geodetic.latitude_deg = (ecef.z_m >= 0.0) ? 90.0 : -90.0;
            geodetic.height_m = std::fabs(ecef.z_m) - WGS84_A * (1.0f - WGS84_F);
            return geodetic;
        }

        float lon_rad = atan2f(ecef.y_m, ecef.x_m);
        geodetic.longitude_deg = lon_rad * RAD_TO_DEG;

        float lat_rad = atan2f(ecef.z_m, p * (1.0f - WGS84_E2)); // Initial approximation

        int iteration = 0;
        float lat_old;
        do
        {
            lat_old = lat_rad;
            float N = WGS84_A / sqrtf(1.0f - WGS84_E2 * sinf(lat_rad) * sinf(lat_rad));
            lat_rad = atan2f(ecef.z_m + WGS84_E2 * N * sinf(lat_rad), p);
            geodetic.height_m = p / cos(lat_rad) - N;
            iteration++;
            if (iteration > 100*MAX_ITERATIONS)
            {
                // // // Did not converge, return NaN values
                // geodetic.latitude_deg = std::numeric_limits<float>::quiet_NaN();
                // geodetic.longitude_deg = std::numeric_limits<float>::quiet_NaN();
                // geodetic.height_m = std::numeric_limits<float>::quiet_NaN();
                // return geodetic;
                break;
            }
        } while (std::fabs(lat_rad - lat_old) > EPSILON);

        geodetic.latitude_deg = lat_rad * RAD_TO_DEG;
        return geodetic;
    }

    // Geodetic to Geocentric (Latitude, Longitude, Height -> Latitude, Longitude, Radius)
    Geocentric geodeticToGeocentric(Geodetic geodetic)
    {
        Geocentric geocentric;

        if (geodetic.latitude_deg < -90.0f || geodetic.latitude_deg > 90.0f)
        {
            geocentric.latitude_deg = std::numeric_limits<float>::quiet_NaN();
            geocentric.longitude_deg = std::numeric_limits<float>::quiet_NaN();
            geocentric.radius_m = std::numeric_limits<float>::quiet_NaN();
            return geocentric;
        }

        float lon_rad = geodetic.longitude_deg * DEG_TO_RAD;
        float lat_rad = geodetic.latitude_deg * DEG_TO_RAD;

        float N = WGS84_A / sqrtf(1 - WGS84_E2 * sinf(lat_rad) * sinf(lat_rad));

        float x = (N + geodetic.height_m) * cosf(lat_rad) * cosf(lon_rad);
        float y = (N + geodetic.height_m) * cosf(lat_rad) * sinf(lon_rad);
        float z = ((1 - WGS84_E2) * N + geodetic.height_m) * sinf(lat_rad); // Corrected z-coordinate calculation

        geocentric.radius_m = sqrtf(x * x + y * y + z * z);
        geocentric.latitude_deg = asinf(z / geocentric.radius_m) * RAD_TO_DEG;
        geocentric.longitude_deg = geodetic.longitude_deg; // Longitude remains the same
        return geocentric;
    }

    // Geocentric to Geodetic (Requires Iterative Solution)
    // IMPORTANT:  The geocentric latitude MUST be known accurately for this to work well.
    Geodetic geocentricToGeodetic(Geocentric geocentric)
    {
        Geodetic geodetic;

        if (geocentric.radius_m <= EPSILON || geocentric.latitude_deg < -90.0f || geocentric.latitude_deg > 90.0f)
        {
            geodetic.latitude_deg = std::numeric_limits<float>::quiet_NaN();
            geodetic.longitude_deg = std::numeric_limits<float>::quiet_NaN();
            geodetic.height_m = std::numeric_limits<float>::quiet_NaN();
            return geodetic;
        }

        float lon_rad = geocentric.longitude_deg * DEG_TO_RAD;
        float geocentric_lat_rad = geocentric.latitude_deg * DEG_TO_RAD;

        float x = geocentric.radius_m * cosf(geocentric_lat_rad) * cosf(lon_rad);
        float y = geocentric.radius_m * cosf(geocentric_lat_rad) * sinf(lon_rad);
        float z = geocentric.radius_m * sinf(geocentric_lat_rad);

        float lat_rad = atan2f(z, sqrtf(x * x + y * y)); // Initial guess for geodetic latitude

        float lat_old;
        float N;
        int iteration = 0;

        // Iterative refinement for latitude and height
        geodetic.height_m = 0; // Initial guess for height

        do
        {
            lat_old = lat_rad;
            N = WGS84_A / sqrtf(1 - WGS84_E2 * sinf(lat_rad) * sinf(lat_rad));
            geodetic.height_m = sqrtf(x * x + y * y) / cosf(lat_rad) - N;
            lat_rad = atan2f(z, (sqrtf(x * x + y * y) * (1 - WGS84_E2 * N / (N + geodetic.height_m))));
            iteration++;

            // Check for oscillation in height and terminate early
            if (iteration > MAX_ITERATIONS)
            {
                // // Did not converge, return NaN values
                // geodetic.latitude_deg = std::numeric_limits<float>::quiet_NaN();
                // geodetic.longitude_deg = std::numeric_limits<float>::quiet_NaN();
                // geodetic.height_m = std::numeric_limits<float>::quiet_NaN();
                // return geodetic;
                break;
            }

        } while (fabsf(lat_rad - lat_old) > EPSILON);

        geodetic.latitude_deg = lat_rad * RAD_TO_DEG;
        geodetic.longitude_deg = geocentric.longitude_deg; // Longitude remains the same
        return geodetic;
    }

    // --- TEME to ECEF Conversion ---

    inline float sgn(float x)
    {
        return (x < 0.0f) ? -1.0f : 1.0f;
    }

    inline float mag(const float x[3])
    {
        return sqrtf(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    }

    inline float floatmod(float a, float b)
    {
        return (a - b * floorf(a / b));
    }

    float gstime(float jdut1)
    {
        constexpr float pi = 3.14159265358979323846f;
        constexpr float twopi = 2.0f * pi;
        constexpr float deg2rad = pi / 180.0f;

        float tut1 = (jdut1 - 2451545.0f) / 36525.0f;
        float temp = -6.2e-6f * tut1 * tut1 * tut1 + 0.093104f * tut1 * tut1 +
                     (876600.0f * 3600.f + 8640184.812866f) * tut1 + 67310.54841f; // sec
        temp = floatmod(temp * deg2rad / 240.0f, twopi);                           // 360/86400 = 1/240, to deg, to rad

        // ------------------------ check quadrants ---------------------
        if (temp < 0.0f)
            temp += twopi;

        return temp;
    } // end gstime

    void polarm(float jdut1, float pm[3][3])
    {
        // Predict polar motion coefficients using IERS Bulletin - A (Vol. XXVIII No. 030)
        float MJD = jdut1 - 2400000.5f; // Julian Date - 2,400,000.5 days
        constexpr float pi = 3.14159265358979323846f;
        float A = 2.f * pi * (MJD - 57226.f) / 365.25f;
        float C = 2.f * pi * (MJD - 57226.f) / 435.f;

        // Polar motion coefficients in radians
        float xp = (0.1033f + 0.0494f * cosf(A) + 0.0482f * sinf(A) + 0.0297f * cosf(C) + 0.0307f * sinf(C)) * 4.84813681e-6f;
        float yp = (0.3498f + 0.0441f * cosf(A) - 0.0393f * sinf(A) + 0.0307f * cosf(C) - 0.0297f * sinf(C)) * 4.84813681e-6f;

        pm[0][0] = cosf(xp);
        pm[0][1] = 0.0f;
        pm[0][2] = -sinf(xp);
        pm[1][0] = sinf(xp) * sinf(yp);
        pm[1][1] = cosf(yp);
        pm[1][2] = cosf(xp) * sinf(yp);
        pm[2][0] = sinf(xp) * cosf(yp);
        pm[2][1] = -sinf(yp);
        pm[2][2] = cosf(xp) * cosf(yp);
    }

    void teme2ecef(const float rteme[3], float jdut1, float recef[3])
    {
        float st[3][3];
        float rpef[3];
        float pm[3][3];

        // Get Greenwich mean sidereal time
        float gmst = gstime(jdut1);

        // st is the pef - tod matrix
        st[0][0] = cosf(gmst);
        st[0][1] = -sinf(gmst);
        st[0][2] = 0.0f;
        st[1][0] = sinf(gmst);
        st[1][1] = cosf(gmst);
        st[1][2] = 0.0f;
        st[2][0] = 0.0f;
        st[2][1] = 0.0f;
        st[2][2] = 1.0f;

        // Get pseudo earth fixed position vector by multiplying the inverse pef-tod matrix by rteme
        rpef[0] = st[0][0] * rteme[0] + st[1][0] * rteme[1];
        rpef[1] = st[0][1] * rteme[0] + st[1][1] * rteme[1];
        rpef[2] = st[2][2] * rteme[2];

        // Get polar motion vector
        polarm(jdut1, pm);

        // ECEF postion vector is the inverse of the polar motion vector multiplied by rpef
        recef[0] = pm[0][0] * rpef[0] + pm[1][0] * rpef[1] + pm[2][0] * rpef[2];
        recef[1] = pm[0][1] * rpef[0] + pm[1][1] * rpef[1] + pm[2][1] * rpef[2];
        recef[2] = pm[0][2] * rpef[0] + pm[1][2] * rpef[1] + pm[2][2] * rpef[2];
    }

    ECEF temeToECEF(TEME teme, float jdut1)
    {
        ECEF ecef;
        float rteme[3] = {teme.x_m / 1000.0f, teme.y_m / 1000.0f, teme.z_m / 1000.0f}; // km
        float recef[3];

        teme2ecef(rteme, jdut1, recef);

        ecef.x_m = recef[0] * 1000.0f;
        ecef.y_m = recef[1] * 1000.0f;
        ecef.z_m = recef[2] * 1000.0f;

        return ecef;
    }

    void ecef2teme(const float recef[3], float jdut1, float rteme[3])
    {
        float st[3][3];
        float rpef[3];
        float pm[3][3];

        // 1. Inverse Polar Motion:
        polarm(jdut1, pm);

        // Transpose the polar motion matrix (pm) because we are inverting the transform.
        rpef[0] = pm[0][0] * recef[0] + pm[0][1] * recef[1] + pm[0][2] * recef[2];
        rpef[1] = pm[1][0] * recef[0] + pm[1][1] * recef[1] + pm[1][2] * recef[2];
        rpef[2] = pm[2][0] * recef[0] + pm[2][1] * recef[1] + pm[2][2] * recef[2];

        // 2. Inverse Greenwich Mean Sidereal Time Rotation:
        float gmst = gstime(jdut1);

        // Need the transpose (inverse) of the st matrix.
        st[0][0] = cosf(gmst);
        st[0][1] = -sinf(gmst);
        st[0][2] = 0.0f;
        st[1][0] = sinf(gmst);
        st[1][1] = cosf(gmst);
        st[1][2] = 0.0f;
        st[2][0] = 0.0f;
        st[2][1] = 0.0f;
        st[2][2] = 1.0f;

        // Transpose st matrix
        rteme[0] = st[0][0] * rpef[0] + st[0][1] * rpef[1];
        rteme[1] = st[1][0] * rpef[0] + st[1][1] * rpef[1];
        rteme[2] = st[2][2] * rpef[2];
    }

    TEME ecefToTEME(ECEF ecef, float jdut1)
    {
        TEME teme;
        float recef[3] = {ecef.x_m / 1000.0f, ecef.y_m / 1000.0f, ecef.z_m / 1000.0f}; // Convert to km
        float rteme[3];

        ecef2teme(recef, jdut1, rteme); // Call the inverse transformation function

        teme.x_m = rteme[0] * 1000.0f; // Convert back to meters
        teme.y_m = rteme[1] * 1000.0f;
        teme.z_m = rteme[2] * 1000.0f;

        return teme;
    }

} // namespace coordinate_transformations