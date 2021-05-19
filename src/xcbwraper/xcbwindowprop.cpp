#include <iostream>
#include <memory>
#include <stdexcept>
#include <xcb/xproto.h>
#include "windowgeometry.hpp"
#include "xcbwindowprop.hpp"

namespace xcbwraper {
XCBWindowProp::XCBWindowProp( WindowIDType windowID ) : mWindow( windowID ) {
    XCBConnection connection {};

    std::unique_ptr< xcb_get_geometry_reply_t > geometryRep { xcb_get_geometry_reply(
    connection, xcb_get_geometry( connection, windowID ), nullptr ) };

    std::unique_ptr< xcb_query_tree_reply_t > tree { xcb_query_tree_reply(
    connection, xcb_query_tree( connection, windowID ), nullptr ) };

    auto screen = xcb_setup_roots_iterator( xcb_get_setup( connection ) ).data;

    std::unique_ptr< xcb_translate_coordinates_reply_t > trans {
        xcb_translate_coordinates_reply(
        connection,
        xcb_translate_coordinates(
        connection, windowID, screen->root, geometryRep->x, geometryRep->y ),
        nullptr )
    };

    WindowGeometry::CreateInfo wgci { .leftTopPoint =
                                      Point { .x = trans->dst_x, .y = trans->dst_y },
                                      .width       = geometryRep->width,
                                      .height      = geometryRep->height,
                                      .borderWidth = geometryRep->border_width };
    mWindowGeometry = WindowGeometry( wgci ).getInfo();

    std::unique_ptr< xcb_get_property_reply_t > nameReply { xcb_get_property_reply(
    connection,
    xcb_get_property(
    connection, false, windowID, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 3 ),
    nullptr ) };
    mClassName = static_cast< char * >( xcb_get_property_value( nameReply.get() ) );

    std::unique_ptr< xcb_get_window_attributes_reply_t > windowAttrs(
    xcb_get_window_attributes_reply(
    connection, xcb_get_window_attributes( connection, mWindow ), nullptr ) );
    mWindowMapState = windowAttrs->map_state;

    auto depthIter = std::make_unique< xcb_depth_iterator_t >(
    xcb_screen_allowed_depths_iterator( screen ) );

    for ( ; depthIter->rem; xcb_depth_next( depthIter.get() ) ) {
        auto visualIter = std::make_unique< xcb_visualtype_iterator_t >(
        xcb_depth_visuals_iterator( depthIter->data ) );
        for ( ; visualIter->rem; xcb_visualtype_next( visualIter.get() ) )
            if ( windowAttrs->visual == visualIter->data->visual_id ) {
                mBitPerRgb = visualIter->data->bits_per_rgb_value;
                break;
            }
    }
}

XCBWindowProp::~XCBWindowProp() = default;

WindowGeometry::Info XCBWindowProp::getGeometry() const { return mWindowGeometry; }

std::string XCBWindowProp::getClass() const { return mClassName; }

WindowIDType XCBWindowProp::getID() const { return mWindow; }

bool XCBWindowProp::isViewable() const {
    return mWindowMapState & XCB_MAP_STATE_VIEWABLE;
}

std::uint8_t XCBWindowProp::getBitPerRGB() const {
    if ( mBitPerRgb == 0 )
        throw std::runtime_error( "In XCBWindowProp::getBitPerRGB() mBitPerRgb is null" );
    return mBitPerRgb;
}

}   // namespace xcbwraper
