#pragma once

#include <cstdint>
#include <string>

#include <xcb/xproto.h>

#include "point.hpp"
#include "xcbconnection.hpp"
#include "windowgeometry.hpp"

namespace xcbwraper {
using XCBWindowClass = std::string;
using WindowIDType   = xcb_window_t;

class XCBWindowProp final {
    std::string          mClassName;
    WindowGeometry::Info mWindowGeometry;
    WindowIDType         mWindow;
    std::uint8_t         mWindowMapState;
    std::uint8_t         mBitPerRgb { 0 };

public:
    explicit XCBWindowProp( WindowIDType windowID );
    XCBWindowProp( XCBWindowProp && ) = default;
    ~XCBWindowProp();
    WindowGeometry::Info getGeometry() const;
    XCBWindowClass       getClass() const;
    WindowIDType         getID() const;
    bool                 isViewable() const;
    std::uint8_t         getBitPerRGB() const;
};

//class XCBWindowID final {
//    WindowIDType windowID;
//
//public:
//    XCBWindowID( WindowIDType windowID ) : windowID { windowID } {};
//    XCBWindowProp params() const { return XCBWindowProp { windowID }; }
//};

}   // namespace xcbwraper
