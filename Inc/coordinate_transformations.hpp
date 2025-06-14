#ifndef COORDINATE_TRANSFORMATIONS_HPP
#define COORDINATE_TRANSFORMATIONS_HPP

#include <cmath>

namespace coordinate_transformations {

// --- Constants ---
constexpr float DEG_TO_RAD = M_PI / 180.0f; // Conversion factor from degrees to radians
constexpr float RAD_TO_DEG = 180.0f / M_PI; // Conversion factor from radians to degrees 

// --- Constants for WGS84 ---
constexpr float WGS84_A = 6378137.0f; // Semi-major axis
constexpr float WGS84_F = 1.0f / 298.257223563f; // Flattening
constexpr float WGS84_B = WGS84_A * (1.0f - WGS84_F); // Semi-minor axis
constexpr float WGS84_E2 = 2 * WGS84_F - WGS84_F * WGS84_F; // Eccentricity squared


// --- Coordinate System Structs ---

// Geodetic Coordinates (Latitude, Longitude, Height)
struct Geodetic {
    float latitude_deg;  // Latitude in degrees
    float longitude_deg; // Longitude in degrees
    float height_m;      // Height above ellipsoid in meters
};

// Geocentric Coordinates (Latitude, Longitude, Radius)
struct Geocentric {
    float latitude_deg;  // Geocentric latitude in degrees
    float longitude_deg; // Longitude in degrees
    float radius_m;      // Radius from center of Earth in meters
};

// ECEF Coordinates (X, Y, Z)
struct ECEF {
    float x_m;           // X coordinate in meters
    float y_m;           // Y coordinate in meters
    float z_m;           // Z coordinate in meters
};

// --- Coordinate Conversion Functions ---

ECEF geodeticToECEF(Geodetic geodetic);
Geodetic ecefToGeodetic(ECEF ecef);

Geocentric geodeticToGeocentric(Geodetic geodetic); 
Geodetic geocentricToGeodetic(Geocentric geocentric);

} /* namespace coordinate_transformations */

#endif // COORDINATE_TRANSFORMATIONS_HPP