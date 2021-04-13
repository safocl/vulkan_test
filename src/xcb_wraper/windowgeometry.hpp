#pragma once

#include <bits/stdint-uintn.h>
#include <cstdint>

#include "point.hpp"

namespace xcbwraper {
class WindowGeometry final {
public:
    struct CreateInfo final {
        Point    leftTopPoint;
        uint16_t width;
        uint16_t height;
        uint16_t borderWidth;
    };

    struct Info final {
        Point    leftTopPoint;
        Point    rightTopPoint;
        Point    leftBotPoint;
        Point    rightBotPoint;
        uint16_t width;
        uint16_t height;
        uint16_t borderWidth;
    };

    explicit WindowGeometry( WindowGeometry::CreateInfo wgci ) :
    wgInfo( Info { .leftTopPoint = wgci.leftTopPoint,
                   .width        = wgci.width,
                   .height       = wgci.height,
                   .borderWidth  = wgci.borderWidth } ) {
        compute();
    }

    Info getInfo() const { return wgInfo; }

    void reinit( CreateInfo wgci ) {
        wgInfo.leftTopPoint = wgci.leftTopPoint;
        wgInfo.width        = wgci.width;
        wgInfo.height       = wgci.height;
        wgInfo.borderWidth  = wgci.borderWidth;

        compute();
    }

private:
    void compute() {
        wgInfo.rightBotPoint = Point {
            static_cast< Point::CoordType >( wgInfo.leftTopPoint.x + wgInfo.width ),
            static_cast< Point::CoordType >( wgInfo.leftTopPoint.y + wgInfo.height )
        };

        wgInfo.rightTopPoint =
        Point { static_cast< Point::CoordType >( wgInfo.leftTopPoint.x + wgInfo.width ),
                static_cast< Point::CoordType >( wgInfo.leftTopPoint.y ) };

        wgInfo.leftBotPoint = Point {
            static_cast< Point::CoordType >( wgInfo.leftTopPoint.x ),
            static_cast< Point::CoordType >( wgInfo.leftTopPoint.y + wgInfo.height )
        };
    }

private:
    Info wgInfo;
};
}   // namespace xcbwraper
