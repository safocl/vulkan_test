#include "composite.hpp"
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <xcb/composite.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

namespace core::composite {

Composite::Composite( xcbwraper::XcbConnectionShared xcbConnection ) :
mXcbConnection( xcbConnection ) {
    if ( !mXcbConnection )
        throw std::runtime_error(
        "In Composite::Composite(XcbConnectionShared xcbConnection) xcbConnection is nullptr" );
    std::unique_ptr< xcb_screen_t > screen {
        xcb_setup_roots_iterator( xcb_get_setup( *mXcbConnection ) ).data
    };
    assert( screen != nullptr );

    xcbwraper::Window::CreateInfo windowsCI { .connection = mXcbConnection,
                                              .window     = screen->root };
    mRootWindow = std::make_shared< xcbwraper::Window >();

    constexpr std::uint32_t XCB_COMPOSITE_MAJOR_VER = 0;
    constexpr std::uint32_t XCB_COMPOSITE_MINOR_VER = 4;

    auto compositeVersion = xcb_composite_query_version(
    *mXcbConnection, XCB_COMPOSITE_MAJOR_VER, XCB_COMPOSITE_MINOR_VER );
    std::unique_ptr< xcb_composite_query_version_reply_t > compositeVersionReply(
    xcb_composite_query_version_reply( *mXcbConnection, compositeVersion, nullptr ) );
    if ( compositeVersionReply->major_version < XCB_COMPOSITE_MAJOR_VER ||
         compositeVersionReply->minor_version < XCB_COMPOSITE_MINOR_VER )
        throw std::runtime_error(
        "In Composite::Composite() : X server is not implement needed version of the composite extension" );

    auto overlayWindowReply = xcb_composite_get_overlay_window_reply(
    *mXcbConnection,
    xcb_composite_get_overlay_window( *mXcbConnection, screen->root ),
    nullptr );

    if ( overlayWindowReply && overlayWindowReply->overlay_win != XCB_NONE ) {
        xcbwraper::Window::CreateInfo windowsCI { .connection = mXcbConnection,
                                                  .window =
                                                  overlayWindowReply->overlay_win };
        mCompositeOverlayWindow = std::make_shared< xcbwraper::Window >( windowsCI );
    } else
        throw std::runtime_error( "Getting overlay windows is failed." );
}

Composite::~Composite() {   //= default;
    xcb_composite_release_overlay_window( *mXcbConnection, *mCompositeOverlayWindow );
}

xcbwraper::WindowShared Composite::getCompositeOverleyWindow() const {
    return mCompositeOverlayWindow;
}
xcbwraper::WindowShared Composite::getRootWindow() const { return mRootWindow; }

}   // namespace core::composite
