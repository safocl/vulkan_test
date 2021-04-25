#pragma once

#include <string>

#include <xcb/xproto.h>

#include "point.hpp"
#include "xcbconnection.hpp"
#include "windowgeometry.hpp"

namespace xcbwraper {
using XCBWindowClass = std::string;
using WindowIDType   = xcb_window_t;

class XCBWindowProp final {
    WindowIDType mWindowID;

public:
    explicit XCBWindowProp( WindowIDType windowID );
    ~XCBWindowProp();
    WindowGeometry getGeometry() const;
    XCBWindowClass getClass() const;
    WindowIDType   getID() const;
};

class XCBWindowID final {
    WindowIDType windowID;

public:
    XCBWindowID( WindowIDType windowID ) : windowID { windowID } {};
    XCBWindowProp params() const { return XCBWindowProp { windowID }; }
};

}   // namespace xcbwraper
