#include <cmath>
#include <limits>
#include <iostream>
#include "coordinate_transformations.hpp"

// Make sure to use the correct namespace
namespace coordinate_transformations {

constexpr float EPSILON = 1.0e-9f;
constexpr int MAX_ITERATIONS = 2000;

// --- Utility Function ---
// Checks if two float values are approximately equal within a given tolerance
bool approximatelyEqual(const float a, const float b, const float tolerance = 1e-6f) {
    return std::fabs(a - b) <= tolerance;
}

// --- Coordinate Conversion Functions (Using Structs) ---

// Geodetic to ECEF
ECEF geodeticToECEF(Geodetic geodetic) {
    ECEF ecef;

    if (geodetic.latitude_deg < -90.0f || geodetic.latitude_deg > 90.0f) {
        ecef.x_m = std::numeric_limits<float>::quiet_NaN();
        ecef.y_m = std::numeric_limits<float>::quiet_NaN();
        ecef.z_m = std::numeric_limits<float>::quiet_NaN();
        return ecef;
    }

    float lon_rad = geodetic.longitude_deg * DEG_TO_RAD;
    float lat_rad = geodetic.latitude_deg * DEG_TO_RAD;

    float N = WGS84_A / sqrtf(1 - WGS84_E2 * sinf(lat_rad) * sinf(lat_rad));

    // Explicitly handle North/South Pole case to avoid floating-point errors
    if (approximatelyEqual(std::fabs(geodetic.latitude_deg), 90.0f)) {
        ecef.x_m = 0.0f;
        ecef.y_m = 0.0f;
        ecef.z_m = ((1 - WGS84_E2) * N + geodetic.height_m) * sinf(lat_rad);

    } else {
        ecef.x_m = (N + geodetic.height_m) * cosf(lat_rad) * cosf(lon_rad);
        ecef.y_m = (N + geodetic.height_m) * cosf(lat_rad) * sinf(lon_rad);
        ecef.z_m = ((1 - WGS84_E2) * N + geodetic.height_m) * sinf(lat_rad);
    }

    return ecef;
}

// // ECEF to Geodetic
// Geodetic ecefToGeodetic(ECEF ecef) {
//     Geodetic geodetic;
//     float NaN = std::numeric_limits<float>::quiet_NaN(); // Get NaN value

//     float p = sqrtf(ecef.x_m * ecef.x_m + ecef.y_m * ecef.y_m);

//     if (approximatelyEqual(p, 0.0f)) {
//         // Special case: point lies on the Z-axis
//         geodetic.longitude_deg = 0.0f;
//         geodetic.latitude_deg = (ecef.z_m >= 0.0f) ? 90.0f : -90.0f;
//         geodetic.height_m = std::fabs(ecef.z_m) - WGS84_A * (1 - WGS84_F);
//         return geodetic;
//     }

//     float lon_rad = atan2f(ecef.y_m, ecef.x_m);
//     geodetic.longitude_deg = lon_rad * RAD_TO_DEG;

//     float lat_rad = atan2f(ecef.z_m, p * (1 - WGS84_E2)); // Initial approximation
//     float lat_old;
//     float N;
//     int iteration = 0;
//     float height_m_old = 0.0f;

//     // Iterative refinement using Bowring's method
//     do {
//         lat_old = lat_rad;
//         height_m_old = geodetic.height_m;
//         N = WGS84_A / sqrtf(1 - WGS84_E2 * sinf(lat_rad) * sinf(lat_rad));
//         geodetic.height_m = p / cosf(lat_rad) - N;
//         lat_rad = atan2f(ecef.z_m, p * (1 - WGS84_E2 * N / (N + geodetic.height_m)));
//         iteration++;
//         if (iteration > MAX_ITERATIONS) {
//             // Did not converge, return NaN values
//             geodetic.latitude_deg = NaN;
//             geodetic.longitude_deg = NaN;
//             geodetic.height_m = NaN;
//             return geodetic;
//         }
//        // Check for oscillation in height and terminate early
//         if (fabsf(geodetic.height_m - height_m_old) > 1000.0f) { // 1000 meter diff
//              geodetic.latitude_deg = NaN;
//             geodetic.longitude_deg = NaN;
//             geodetic.height_m = NaN;
//             return geodetic; //Did not converge.
//         }
//     } while (fabsf(lat_rad - lat_old) > EPSILON);

//     geodetic.latitude_deg = lat_rad * RAD_TO_DEG;
//     return geodetic;
// }

// ECEF to Geodetic
Geodetic ecefToGeodetic(ECEF ecef) {
    Geodetic geodetic;
    float NaN = std::numeric_limits<float>::quiet_NaN();

    float p = sqrtf(ecef.x_m * ecef.x_m + ecef.y_m * ecef.y_m);

    if (approximatelyEqual(p, 0.0)) {
        // Special case: point lies on the Z-axis, poles
        geodetic.longitude_deg = 0.0;
        geodetic.latitude_deg = (ecef.z_m >= 0.0) ? 90.0 : -90.0;
        geodetic.height_m = std::fabs(ecef.z_m) - WGS84_A * (1 - WGS84_F);
        return geodetic;
    }

    float lon_rad = atan2(ecef.y_m, ecef.x_m);
    geodetic.longitude_deg = lon_rad * RAD_TO_DEG;

    float lat_rad = atan2f(ecef.z_m, p * (1 - WGS84_E2)); // Initial approximation

    float lat_old;
    float N;
    int iteration = 0;
    do {
        lat_old = lat_rad;
        N = WGS84_A / sqrtf(1 - WGS84_E2 * sinf(lat_rad) * sinf(lat_rad));
        geodetic.height_m = p / cos(lat_rad) - N;
        lat_rad = atan2f(ecef.z_m, p * (1 - WGS84_E2 * N / (N + geodetic.height_m)));
        iteration++;
         if (iteration > MAX_ITERATIONS) {
            // Did not converge, return NaN values
            geodetic.latitude_deg = NaN;
            geodetic.longitude_deg = NaN;
            geodetic.height_m = NaN;
            return geodetic;
        }
    } while (std::fabs(lat_rad - lat_old) > EPSILON);

    geodetic.latitude_deg = lat_rad * RAD_TO_DEG;
    return geodetic;
}

// Geodetic to Geocentric (Latitude, Longitude, Height -> Latitude, Longitude, Radius)
Geocentric geodeticToGeocentric(Geodetic geodetic) {
    Geocentric geocentric;

    if (geodetic.latitude_deg < -90.0f || geodetic.latitude_deg > 90.0f) {
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
    float z = ((1 - WGS84_E2) * N + geodetic.height_m) * sinf(lat_rad);  // Corrected z-coordinate calculation

    geocentric.radius_m = sqrtf(x * x + y * y + z * z);
    geocentric.latitude_deg = asinf(z / geocentric.radius_m) * RAD_TO_DEG;
    geocentric.longitude_deg = geodetic.longitude_deg;  // Longitude remains the same
    return geocentric;
}

// Geocentric to Geodetic (Requires Iterative Solution)
// IMPORTANT:  The geocentric latitude MUST be known accurately for this to work well.
Geodetic geocentricToGeodetic(Geocentric geocentric) {
    Geodetic geodetic;

    if (geocentric.radius_m <= EPSILON || geocentric.latitude_deg < -90.0f || geocentric.latitude_deg > 90.0f) {
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

    float lat_rad = atan2f(z, sqrtf(x * x + y * y));  // Initial guess for geodetic latitude

    float lat_old;
    float N;
    int iteration = 0;

    // Iterative refinement for latitude and height
    geodetic.height_m = 0; // Initial guess for height

    do {
        lat_old = lat_rad;
        N = WGS84_A / sqrtf(1 - WGS84_E2 * sinf(lat_rad) * sinf(lat_rad));
        geodetic.height_m = sqrtf(x * x + y * y) / cosf(lat_rad) - N;
        lat_rad = atan2f(z, (sqrtf(x * x + y * y) * (1 - WGS84_E2 * N / (N + geodetic.height_m))));
        iteration++;

       // Check for oscillation in height and terminate early
        if (iteration > MAX_ITERATIONS) {
            // Did not converge, return NaN values
            geodetic.latitude_deg = std::numeric_limits<float>::quiet_NaN();
            geodetic.longitude_deg = std::numeric_limits<float>::quiet_NaN();
            geodetic.height_m = std::numeric_limits<float>::quiet_NaN();
            return geodetic;
        }

    } while (fabsf(lat_rad - lat_old) > EPSILON);

    geodetic.latitude_deg = lat_rad * RAD_TO_DEG;
    geodetic.longitude_deg = geocentric.longitude_deg; // Longitude remains the same
    return geodetic;
}

} // namespace coordinate_transformations