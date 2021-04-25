#include "window.hpp"
#include "xcbwindowprop.hpp"
#include <bits/c++config.h>
#include <bits/stdint-uintn.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

namespace xcbwraper {

Window::Window() : mWindow( XCB_NONE ) {}

Window::Window( Window::CreateInfo ci ) :
mConnection( ci.connection ), mWindow( ci.window ) {}

Window::~Window() { xcb_destroy_window( *mConnection, mWindow ); }
Window & Window::operator=( xcb_window_t & window ) {
    mWindow = window;
    return *this;
}

Window::operator xcb_window_t() const { return mWindow; }

XCBWindowProp Window::getProperties() const {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error( "Using Window::getProperties() for null window id" );
    XCBWindowProp prop( mWindow );
    return prop;
}

std::vector< uint8_t > Window::getImageData() const {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error( "Using Window::getImageData() for null window id" );
    auto windowGeometry = getProperties().getGeometry().getInfo();
    auto imageCoockie   = xcb_get_image( *mConnection,
                                       XCB_IMAGE_FORMAT_Z_PIXMAP,
                                       mWindow,
                                       windowGeometry.leftTopPoint.x,
                                       windowGeometry.leftTopPoint.y,
                                       windowGeometry.width,
                                       windowGeometry.height,
                                       0 );
    auto imageReply     = xcb_get_image_reply( *mConnection, imageCoockie, nullptr );

    if ( imageReply )
        throw std::runtime_error(
        "In Window::getImageData(): xcb_get_image_reply() return nullptr!" );
    if ( !xcb_get_image_data( imageReply ) )
        throw std::runtime_error(
        "In Window::getImageData(): xcb_get_image_data() return nullptr!" );
    std::vector< uint8_t > imageData(
    static_cast< std::size_t >( xcb_get_image_data_length( imageReply ) ) );
    std::memcpy( imageData.data(), xcb_get_image_data( imageReply ), imageData.size() );

    return imageData;
}

}   // namespace xcbwraper
