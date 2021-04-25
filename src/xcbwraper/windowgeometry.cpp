#include "windowgeometry.hpp"

namespace xcbwraper {

WindowGeometry::WindowGeometry( WindowGeometry::CreateInfo wgci ) :
wgInfo( Info { .leftTopPoint = wgci.leftTopPoint,
               .width        = wgci.width,
               .height       = wgci.height,
               .borderWidth  = wgci.borderWidth } ) {
    compute();
}

WindowGeometry::Info WindowGeometry::getInfo() const { return wgInfo; }

void WindowGeometry::reinit( CreateInfo wgci ) {
    wgInfo.leftTopPoint = wgci.leftTopPoint;
    wgInfo.width        = wgci.width;
    wgInfo.height       = wgci.height;
    wgInfo.borderWidth  = wgci.borderWidth;

    compute();
}

void WindowGeometry::compute() {
    wgInfo.rightBotPoint =
    Point { static_cast< Point::CoordType >( wgInfo.leftTopPoint.x + wgInfo.width ),
            static_cast< Point::CoordType >( wgInfo.leftTopPoint.y + wgInfo.height ) };

    wgInfo.rightTopPoint =
    Point { static_cast< Point::CoordType >( wgInfo.leftTopPoint.x + wgInfo.width ),
            static_cast< Point::CoordType >( wgInfo.leftTopPoint.y ) };

    wgInfo.leftBotPoint =
    Point { static_cast< Point::CoordType >( wgInfo.leftTopPoint.x ),
            static_cast< Point::CoordType >( wgInfo.leftTopPoint.y + wgInfo.height ) };
}

}   // namespace xcbwraper
