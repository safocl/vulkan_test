#include "window.hpp"
#include "xcbwindowprop.hpp"
#include <bits/c++config.h>
#include <bits/stdint-uintn.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <xcb/composite.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

namespace xcbwraper {

Window::Window() {}

Window::Window( Window::CreateInfo ci ) :
mConnection( ci.connection ), mWindow( ci.window ) {}

Window::~Window() {
    if ( mWindow != XCB_NONE )
        xcb_destroy_window( *mConnection, mWindow );
}

Window & Window::operator=( xcb_window_t & window ) {
    mWindow = window;
    return *this;
}

Window::operator xcb_window_t() const { return mWindow; }

Window::operator bool() const { return mWindow != XCB_NONE; }

XCBWindowProp Window::getProperties() const {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error(
        "Using Window::getProperties() for null window id" );

    return XCBWindowProp( mWindow );
}

std::vector< uint8_t > Window::getImageData() const {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error(
        "Using Window::getImageData() for null window id" );
    if ( !getProperties().isViewable() )
        throw std::runtime_error(
        "Using Window::getImageData() for not viewvable window" );

    //    constexpr std::uint32_t XCB_COMPOSITE_MAJOR_VER = 0;
    //    constexpr std::uint32_t XCB_COMPOSITE_MINOR_VER = 4;
    //
    //    auto compositeVersion = xcb_composite_query_version(
    //    *mConnection, XCB_COMPOSITE_MAJOR_VER, XCB_COMPOSITE_MINOR_VER );
    //    std::unique_ptr< xcb_composite_query_version_reply_t > compositeVersionReply(
    //    xcb_composite_query_version_reply( *mConnection, compositeVersion, nullptr ) );
    //    if ( compositeVersionReply->major_version < XCB_COMPOSITE_MAJOR_VER ||
    //         compositeVersionReply->minor_version < XCB_COMPOSITE_MINOR_VER )
    //        throw std::runtime_error(
    //        "In Window::getImageData() : X server is not implement needed version of the composite extension" );
    //
    //    xcb_composite_redirect_window(
    //    *mConnection, mWindow, XCB_COMPOSITE_REDIRECT_AUTOMATIC );
    //
    //    xcb_pixmap_t winPixmap = xcb_generate_id( *mConnection );
    //    xcb_composite_name_window_pixmap( *mConnection, mWindow, winPixmap );

    auto windowGeometry = getProperties().getGeometry();

    auto imageCoockie = xcb_get_image( *mConnection,
                                       XCB_IMAGE_FORMAT_Z_PIXMAP,
                                       mWindow,
                                       0,
                                       0,
                                       windowGeometry.width,
                                       windowGeometry.height,
                                       ~0 );

    std::unique_ptr< xcb_get_image_reply_t > imageReply(
    xcb_get_image_reply( *mConnection, imageCoockie, nullptr ) );

    if ( !imageReply )
        throw std::runtime_error(
        "In Window::getImageData(): xcb_get_image_reply() return nullptr!" );
    std::uint8_t * startData = xcb_get_image_data( imageReply.get() );
    if ( !startData )
        throw std::runtime_error(
        "In Window::getImageData(): xcb_get_image_data() return nullptr!" );

    std::vector< uint8_t > imageData(
    static_cast< std::size_t >( xcb_get_image_data_length( imageReply.get() ) ) );
    std::uint8_t * endData = startData + imageData.size();
    std::copy( startData, endData, imageData.begin() );
    //std::memcpy( imageData.data(), xcb_get_image_data( imageReply ), imageData.size() );

    //xcb_free_pixmap( *mConnection, winPixmap );
    //xcb_composite_unredirect_window(
    //*mConnection, mWindow, XCB_COMPOSITE_REDIRECT_AUTOMATIC );

    return imageData;
}

}   // namespace xcbwraper
