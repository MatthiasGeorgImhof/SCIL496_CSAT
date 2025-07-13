#pragma once

#include "au.hh"

namespace au
{
    typedef decltype(Meters{} / Seconds{}) MetersPerSecond;
    typedef decltype(Meters{} / (Seconds{} * Seconds{})) MetersPerSecondSquared;
    typedef decltype(Degrees{} / Seconds{}) DegreesPerSecond;
    typedef decltype(Degrees{} / (Seconds{} * Seconds{})) DegreesPerSecondSquared;
    typedef decltype(Radians{} / Seconds{}) RadiansPerSecond;
    typedef decltype(Radians{} / (Seconds{} * Seconds{})) RadiansPerSecondSquared;
}
