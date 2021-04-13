#include "composite.hpp"
#include <cassert>
#include <stdexcept>
#include <xcb/composite.h>
#include <xcb/xcb.h>

namespace core::composite {

Composite::Composite() {
    mXcbConnection = xcb_connect( nullptr, nullptr );
    assert( mXcbConnection != nullptr );
    auto screen = xcb_setup_roots_iterator( xcb_get_setup( mXcbConnection ) ).data;
    assert( screen != nullptr );

    auto overlayWindowReply = xcb_composite_get_overlay_window_reply(
    mXcbConnection,
    xcb_composite_get_overlay_window( mXcbConnection, screen->root ),
    nullptr );

    if ( overlayWindowReply || overlayWindowReply->overlay_win != XCB_NONE )
        mCompositeOverlayWindow = overlayWindowReply->overlay_win;
    else
        throw std::runtime_error( "Getting overlay windows is failed." );
    xcb_disconnect( mXcbConnection );
}

Composite::~Composite() {
    mCompositeOverlayWindow = XCB_NONE;
}

xcb_window_t Composite::getCompositeOverleyWindow() const {
    return mCompositeOverlayWindow;
}

}   // namespace core::composite
