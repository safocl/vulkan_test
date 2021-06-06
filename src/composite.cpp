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
}

Composite::~Composite() {   //= default;
    //    xcb_composite_release_overlay_window( *mXcbConnection,
    //                                          *mCompositeOverlayWindow );
}

xcbwraper::WindowShared Composite::getCompositeOverleyWindow() const {
    xcb_screen_t * screen {
        xcb_setup_roots_iterator( xcb_get_setup( *mXcbConnection ) ).data
    };
    assert( screen );
    constexpr std::uint32_t XCB_COMPOSITE_MAJOR_VER = 0;
    constexpr std::uint32_t XCB_COMPOSITE_MINOR_VER = 4;

    auto compositeVersion = xcb_composite_query_version(
    *mXcbConnection, XCB_COMPOSITE_MAJOR_VER, XCB_COMPOSITE_MINOR_VER );
    std::unique_ptr< xcb_composite_query_version_reply_t > compositeVersionReply(
    xcb_composite_query_version_reply(
    *mXcbConnection, compositeVersion, nullptr ) );
    if ( compositeVersionReply->major_version < XCB_COMPOSITE_MAJOR_VER ||
         compositeVersionReply->minor_version < XCB_COMPOSITE_MINOR_VER )
        throw std::runtime_error(
        "In Composite::Composite() : X server is not implement needed version of the composite extension" );

    auto overlayWindowReply = xcb_composite_get_overlay_window_reply(
    *mXcbConnection,
    xcb_composite_get_overlay_window( *mXcbConnection, screen->root ),
    nullptr );

    xcbwraper::WindowShared compositeOverlayWindow;

    if ( overlayWindowReply ) {
        xcbwraper::Window::CreateInfo windowsCI { .connection = mXcbConnection,
                                                  .window =
                                                  overlayWindowReply->overlay_win };
        compositeOverlayWindow =
        std::make_shared< xcbwraper::CompositeWindow >( windowsCI );
    } else
        throw std::runtime_error( "Getting overlay windows is failed." );

    return compositeOverlayWindow;
}
xcbwraper::WindowShared Composite::getRootWindow() const {
    xcb_screen_t * screen {
        xcb_setup_roots_iterator( xcb_get_setup( *mXcbConnection ) ).data
    };
    assert( screen );

    xcbwraper::Window::CreateInfo windowsCI { .connection = mXcbConnection,
                                              .window     = screen->root };
    auto rootWindow = std::make_shared< xcbwraper::Window >( windowsCI );

    return rootWindow;
}

}   // namespace core::composite
