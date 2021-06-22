#include "composite.hpp"
#include "xcbwraper/xcbinternatom.hpp"
#include "xcbwraper/extension_query_version.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <xcb/composite.h>
#include <xcb/shape.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xfixes.h>

namespace core::composite {

Composite::Composite( xcbwraper::XcbConnectionShared xcbConnection ) :
mXcbConnection( xcbConnection ) {
    if ( !mXcbConnection )
        throw std::runtime_error(
        "In Composite::Composite(XcbConnectionShared xcbConnection) xcbConnection is nullptr" );

    xcbwraper::compositeCheckQueryVersion(*mXcbConnection, 0, 4);    
    xcbwraper::xfixesCheckQueryVersion(*mXcbConnection, 5, 0);    
    xcbwraper::shapeCheckQueryVersion(*mXcbConnection);    
}

Composite::~Composite() {   //= default;
    //    xcb_composite_release_overlay_window( *mXcbConnection,
    //                                          *mCompositeOverlayWindow );
}

xcbwraper::WindowShared Composite::getCompositeOverleyWindow() const {
    xcb_screen_t * screen {
        xcb_setup_roots_iterator( xcb_get_setup( *mXcbConnection ) ).data
    };

    if ( !screen )
        throw std::runtime_error( "Getting the screen is failed." );

    auto overlayWindowReply = xcb_composite_get_overlay_window_reply(
    *mXcbConnection,
    xcb_composite_get_overlay_window( *mXcbConnection, screen->root ),
    nullptr );

    if ( !overlayWindowReply )
        throw std::runtime_error( "Getting overlay windows is failed." );

    xcbwraper::WindowShared compositeOverlayWindow;

    xcbwraper::Window::CreateInfo windowsCI { .connection = mXcbConnection,
                                              .window =
                                              overlayWindowReply->overlay_win };
    compositeOverlayWindow =
    std::make_shared< xcbwraper::CompositeWindow >( windowsCI );

    return compositeOverlayWindow;
}

xcbwraper::WindowShared
Composite::getCompositeOverleyWindowWithoutInputEvents() const {
    xcb_screen_t * screen {
        xcb_setup_roots_iterator( xcb_get_setup( *mXcbConnection ) ).data
    };

    if ( !screen )
        throw std::runtime_error( "Getting the screen is failed." );

    std::unique_ptr< xcb_composite_get_overlay_window_reply_t > overlayWindowReply {
        xcb_composite_get_overlay_window_reply(
        *mXcbConnection,
        xcb_composite_get_overlay_window( *mXcbConnection, screen->root ),
        nullptr )
    };

    if ( !overlayWindowReply )
        throw std::runtime_error( "Getting overlay windows is failed." );

    xcbwraper::WindowShared compositeOverlayWindow;

    xcbwraper::Window::CreateInfo windowsCI { .connection = mXcbConnection,
                                              .window =
                                              overlayWindowReply->overlay_win };
    compositeOverlayWindow =
    std::make_shared< xcbwraper::CompositeWindow >( windowsCI );

    //    std::array< u32, 1 > vaList { { XCB_EVENT_MASK_EXPOSURE } };
    //    xcb_change_window_attributes(
    //    *mXcbConnection, *compositeOverlayWindow, XCB_CW_EVENT_MASK, vaList.data() );

    xcb_xfixes_region_t region = xcb_generate_id( *mXcbConnection );

    auto xfixesRegionCoockie =
    xcb_xfixes_create_region_checked( *mXcbConnection, region, 0, nullptr );

    xcb_generic_error_t * xfixesCreateErr =
    xcb_request_check( *mXcbConnection, xfixesRegionCoockie );

    if ( xfixesCreateErr )
        throw std::runtime_error(
        "In Composite::getCompositeOverleyWindowWithoutInputEvents : Xfixes creation failed with code." +
        std::to_string( xfixesCreateErr->error_code ) );

    xcb_discard_reply( *mXcbConnection, xfixesRegionCoockie.sequence );

    xcb_xfixes_set_window_shape_region( *mXcbConnection,
                                        *compositeOverlayWindow,
                                        XCB_SHAPE_SK_BOUNDING,
                                        0,
                                        0,
                                        XCB_XFIXES_REGION_NONE );
    xcb_xfixes_set_window_shape_region(
    *mXcbConnection, *compositeOverlayWindow, XCB_SHAPE_SK_INPUT, 0, 0, region );

    xcb_xfixes_destroy_region( *mXcbConnection, region );

    xcb_flush( *mXcbConnection );

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

std::list< xcbwraper::WindowShared > Composite::getRedirectedWindows() const {
    const auto netClientList = xcbwraper::AtomNetClientList().get();
    std::list< xcbwraper::WindowShared > redirectedWindows {};
    xcbwraper::Window::CreateInfo        windowCI { .connection = mXcbConnection };
    for ( auto && windowProp : netClientList )
        if ( windowProp.isViewable() ) {
            windowCI.window = windowProp.getID();

            redirectedWindows.emplace_back(
            std::make_shared< xcbwraper::Window >( windowCI ) );

            redirectedWindows.back()->redirect();
        }

    return redirectedWindows;
}
}   // namespace core::composite
