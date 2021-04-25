#include <memory>
#include "xcbwindowprop.hpp"

namespace xcbwraper {
XCBWindowProp::XCBWindowProp( WindowIDType windowID ) : mWindowID( windowID ) {}

XCBWindowProp::~XCBWindowProp() = default;

WindowGeometry XCBWindowProp::getGeometry() const {
    XCBConnection connection {};

    std::unique_ptr< xcb_get_geometry_reply_t > geometryRep { xcb_get_geometry_reply(
    connection, xcb_get_geometry( connection, mWindowID ), nullptr ) };

    std::unique_ptr< xcb_query_tree_reply_t > tree { xcb_query_tree_reply(
    connection, xcb_query_tree( connection, mWindowID ), nullptr ) };

    auto screen = xcb_setup_roots_iterator( xcb_get_setup( connection ) ).data;

    std::unique_ptr< xcb_translate_coordinates_reply_t > trans {
        xcb_translate_coordinates_reply(
        connection,
        xcb_translate_coordinates(
        connection, mWindowID, screen->root, geometryRep->x, geometryRep->y ),
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

std::string XCBWindowProp::getClass() const {
    XCBConnection connection {};

    std::unique_ptr< xcb_get_property_reply_t > nameRep { xcb_get_property_reply(
    connection,
    xcb_get_property(
    connection, false, mWindowID, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 3 ),
    nullptr ) };

    std::string className { static_cast< char * >(
    xcb_get_property_value( nameRep.get() ) ) };
    return className;
}

WindowIDType XCBWindowProp::getID() const { return mWindowID; }

}   // namespace xcbwraper
