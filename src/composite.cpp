#include "composite.hpp"
#include <cassert>
#include <stdexcept>
#include <xcb/composite.h>
#include <xcb/xcb.h>

namespace core::composite {

Composite::Composite( xcbwraper::XcbConnectionShared xcbConnection ) : mXcbConnection( xcbConnection ) {
    if ( !mXcbConnection )
        throw std::runtime_error(
        "In Composite::Composite(XcbConnectionShared xcbConnection) xcbConnection is nullptr" );
    auto screen = xcb_setup_roots_iterator( xcb_get_setup( *mXcbConnection ) ).data;
    assert( screen != nullptr );

    mRootWindow = screen->root;

    auto overlayWindowReply = xcb_composite_get_overlay_window_reply(
    *mXcbConnection,
    xcb_composite_get_overlay_window( *mXcbConnection, screen->root ),
    nullptr );

    if ( overlayWindowReply || overlayWindowReply->overlay_win != XCB_NONE )
        mCompositeOverlayWindow = overlayWindowReply->overlay_win;
    else
        throw std::runtime_error( "Getting overlay windows is failed." );
}

Composite::~Composite() = default;

xcbwraper::Window Composite::getCompositeOverleyWindow() const {
    return mCompositeOverlayWindow;
}
xcbwraper::Window Composite::getRootWindow() const { return mRootWindow; }

}   // namespace core::composite
