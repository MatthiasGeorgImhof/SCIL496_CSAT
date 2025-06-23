#include "sgp4_tle.hpp"
#include <cmath>

#ifdef __linux__
namespace sgp4_utils
{

    // Helper function to validate that the data is within range to avoid overflow
    bool isValidRange(float value, float min, float max)
    {
        return !std::isnan(value) && value >= min && value <= max;
    }

    // Helper function to validate that the data is within range to avoid overflow
    bool isValidRange(int32_t value, int32_t min, int32_t max)
    {
        return value >= min && value <= max;
    }

    char checksum(const std::string &s)
    {
        uint sum = 0;
        for (size_t i = 0; i < s.length() - 1; ++i)
        {
            char c = s[i];
            if (c >= '0' && c <= '9')
                sum += c - '0';
            if (c == '-')
                sum += 1;
        }
        return '0' + (sum % 10);
    }

    std::optional<SGP4TwoLineElement> parseTLE(const std::string &l1, const std::string &l2)
    {
        if (l1.length() != 69 || l2.length() != 69)
            return std::nullopt;

        SGP4TwoLineElement tle{};
        // Line 1 Parsing
        tle.satelliteNumber = std::stoi(l1.substr(2, 5));               // Columns 3-7
        tle.epochYear = std::stoi(l1.substr(18, 2));                    // Columns 19-20
        tle.epochDay = std::stod(l1.substr(20, 12));                    // Columns 21-32
        tle.meanMotionDerivative1 = std::stod(l1.substr(33, 10));       // Columns 34-43
        tle.meanMotionDerivative2 = std::stod("0." + l1.substr(44, 8)); // Columns 45-52 (decimal point assumed)

        // Handle the exponent form of bstar
        std::string bStarStr = l1.substr(53, 8);
        std::string bStarSign = bStarStr.substr(0, 1);
        std::string bStarInteger = bStarStr.substr(1, 5);
        std::string bStarExponent = bStarStr.substr(6, 2);
        tle.bStarDrag = std::stod(bStarSign + "0." + bStarInteger + "e" + bStarExponent);

        tle.ephemerisType = std::stoi(l1.substr(62, 1));                // Column 63
        tle.elementNumber = std::stoi(l1.substr(64, 4));                // Columns 65-68
        if (l1[68] != checksum(l1))
            return std::nullopt;

        // Line 2 Parsing
        tle.inclination = std::stod(l2.substr(8, 8));                   // Columns 9-16
        tle.rightAscensionAscendingNode = std::stod(l2.substr(17, 8));  // Columns 18-25
        tle.eccentricity = std::stod("0." + l2.substr(26, 7));          // Columns 27-33 (decimal point assumed)
        tle.argumentOfPerigee = std::stod(l2.substr(34, 8));            // Columns 35-42
        tle.meanAnomaly = std::stod(l2.substr(43, 8));                  // Columns 44-51
        tle.meanMotion = std::stod(l2.substr(52, 11));                  // Columns 53-63
        tle.revolutionNumberAtEpoch = std::stoi(l2.substr(63, 5));      // Columns 64-68
        if (l2[68] != checksum(l2))
            return std::nullopt;

        return tle;
    }

} // namespace sgp4_utils

#endif // __linux__