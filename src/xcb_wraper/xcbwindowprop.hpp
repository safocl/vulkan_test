#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <xcb/xproto.h>

#include "point.hpp"
#include "xcbconnect.hpp"
#include "windowgeometry.hpp"

namespace xcbwraper {
using XCBWindowClass = std::string;
using WindowIDType   = u_int32_t;

class XCBWindowProp final {
    WindowIDType mWindowID;

public:
    explicit XCBWindowProp( WindowIDType windowID );
    ~XCBWindowProp();
    WindowGeometry Geometry() const;
    XCBWindowClass Class() const;
    WindowIDType   ID() const;
};

class XCBWindowID final {
    WindowIDType windowID;

public:
    XCBWindowID( WindowIDType windowID ) : windowID { windowID } {};
    XCBWindowProp params() const { return XCBWindowProp { windowID }; }
};

XCBWindowProp::XCBWindowProp( WindowIDType windowID ) : mWindowID( windowID ) {}

XCBWindowProp::~XCBWindowProp() = default;

WindowGeometry XCBWindowProp::Geometry() const {
    XCBConnect connect {};

    std::unique_ptr< xcb_get_geometry_reply_t > geometryRep { xcb_get_geometry_reply(
    connect, xcb_get_geometry( connect, mWindowID ), nullptr ) };

    std::unique_ptr< xcb_query_tree_reply_t > tree { xcb_query_tree_reply(
    connect, xcb_query_tree( connect, mWindowID ), nullptr ) };

    auto screen = xcb_setup_roots_iterator( xcb_get_setup( connect ) ).data;

    std::unique_ptr< xcb_translate_coordinates_reply_t > trans {
        xcb_translate_coordinates_reply(
        connect,
        xcb_translate_coordinates(
        connect, mWindowID, screen->root, geometryRep->x, geometryRep->y ),
        nullptr )
    };

    WindowGeometry::CreateInfo wgci { .leftTopPoint =
                                      Point { .x = trans->dst_x, .y = trans->dst_y },
                                      .width       = geometryRep->width,
                                      .height      = geometryRep->height,
                                      .borderWidth = geometryRep->border_width };
    WindowGeometry             windowGeometry( wgci );
    return windowGeometry;
}

std::string XCBWindowProp::Class() const {
    XCBConnect connect {};

    std::unique_ptr< xcb_get_property_reply_t > nameRep { xcb_get_property_reply(
    connect,
    xcb_get_property(
    connect, false, mWindowID, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 3 ),
    nullptr ) };

    return std::string { static_cast< char * >(
    xcb_get_property_value( nameRep.get() ) ) };
}

WindowIDType XCBWindowProp::ID() const { return mWindowID; }
}   // namespace xcbwraper
