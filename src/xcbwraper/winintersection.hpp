#pragma once

#include <cstdint>

#include "point.hpp"
#include "windowgeometry.hpp"

namespace xcbwraper {
struct Intersection final {
    Point    leftTopPoint { 0, 0 };
    uint16_t width { 0 };
    uint16_t height { 0 };
    bool     isExist { false };
             operator bool() const { return isExist; }
};

Intersection intersect( WindowGeometry::Info windowOne, WindowGeometry::Info windowTwo );
}   // namespace xcbwraper
