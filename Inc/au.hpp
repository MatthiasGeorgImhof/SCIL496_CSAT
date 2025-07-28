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

    struct TemeBaseDim : au::base_dim::BaseDimension<1690384951>
    {
    };
    struct Temes : au::UnitImpl<au::Dimension<TemeBaseDim>>
    {
        static constexpr inline const char label[] = "teme";
    };
    constexpr auto teme = SingularNameFor<Temes>{};
    constexpr auto temes = QuantityMaker<Temes>{};

    typedef decltype(Temes{} * Meters{}) MetersInTemeFrame;
    typedef decltype(Temes{} * MetersPerSecond{}) MetersPerSecondInTemeFrame;

    struct EcefBaseDim : au::base_dim::BaseDimension<1690384953>
    {
    };
    struct Ecefs : au::UnitImpl<au::Dimension<EcefBaseDim>>
    {
        static constexpr inline const char label[] = "ecef";
    };
    constexpr auto ecef = SingularNameFor<Ecefs>{};
    constexpr auto ecefs = QuantityMaker<Ecefs>{};

    typedef decltype(Ecefs{} * Meters{}) MetersInEcefFrame;
    typedef decltype(Ecefs{} * MetersPerSecond{}) MetersPerSecondInEcefFrame;
}
