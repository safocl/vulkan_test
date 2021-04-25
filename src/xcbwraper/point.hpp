#pragma once


#include <cstdint>

namespace xcbwraper {
struct Point final {
    using CoordType = int16_t;
    CoordType x;
    CoordType y;

    bool operator>( Point other ) { return x > other.x && y > other.y; }
    bool operator<( Point other ) { return x < other.x && y < other.y; }
    bool operator>=( Point other ) {
        return ( x > other.x && y > other.y ) || ( x == other.x && y == other.y );
    }
    bool operator<=( Point other ) {
        return ( x < other.x && y < other.y ) || ( x == other.x && y == other.y );
    }
};
}
