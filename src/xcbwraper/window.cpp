#include "window.hpp"
#include "errno-exception.hpp"
#include "posix-shm.hpp"
#include "xcbwindowprop.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <exception>
#include <xcb/composite.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/shm.h>

namespace xcbwraper {

Window::Window() {}

Window::Window( Window::CreateInfo ci ) :
mConnection( ci.connection ), mWindow( ci.window ) {}

Window::~Window() {
    if ( mWindow != XCB_NONE ) {
        xcb_destroy_window( *mConnection, mWindow );
        xcb_flush( *mConnection );
    }
}

Window & Window::operator=( xcb_window_t window ) {
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

posix::SharedMemory::Shared Window::getImageData() const {
    posix::SharedMemory::Shared shm = std::make_shared< posix::SharedMemory >();
    //shm                             = getImageData( shm );

    return getImageData( shm );
}
posix::SharedMemory::Shared
Window::getImageData( posix::SharedMemory::Shared shm ) const {
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

    auto                    windowGeometry = getProperties().getGeometry();
    constexpr std::uint32_t plane          = ~0;
    const std::uint32_t imageSize = windowGeometry.height * windowGeometry.width * 4;

        //    shm->release();
        shm->reinit( imageSize );

    //shm->init(imageSize);

    const xcb_shm_seg_t shmSeg = xcb_generate_id( *mConnection );

    xcb_shm_attach( *mConnection, shmSeg, shm->getId(), false );

    xcb_generic_error_t * error = nullptr;

    //auto shmSegCreateCoockie =
    //xcb_shm_create_segment( *mConnection, shmSeg, imageSize, false);

    //std::unique_ptr< xcb_shm_create_segment_reply_t > shmSegReply {
    //    xcb_shm_create_segment_reply( *mConnection, shmSegCreateCoockie, &error )
    //};

    //if ( error )
    //    throw std::runtime_error(
    //    "In Window::getImageData(): xcb_shm_create_segment_reply() return error code " +
    //    std::to_string( static_cast< std::uint32_t >( error->error_code ) ) );

    const auto shmGetImgCoockie = xcb_shm_get_image( *mConnection,
                                                     mWindow,
                                                     0,
                                                     0,
                                                     windowGeometry.width,
                                                     windowGeometry.height,
                                                     plane,
                                                     XCB_IMAGE_FORMAT_Z_PIXMAP,
                                                     shmSeg,
                                                     0 );

    std::unique_ptr< xcb_shm_get_image_reply_t > shmGetImgReply {
        xcb_shm_get_image_reply( *mConnection, shmGetImgCoockie, &error )
    };

    if ( error ) {
        throw std::runtime_error(
        "In Window::getImageData(): xcb_shm_get_image_reply() return error code " +
        std::to_string( static_cast< std::uint32_t >( error->error_code ) ) );
    }

    xcb_shm_detach( *mConnection, shmSeg );

    //    auto imageCoockie = xcb_get_image( *mConnection,
    //                                       XCB_IMAGE_FORMAT_Z_PIXMAP,
    //                                       mWindow,
    //                                       0,
    //                                       0,
    //                                       windowGeometry.width,
    //                                       windowGeometry.height,
    //                                       plane );
    //
    //    std::unique_ptr< xcb_get_image_reply_t > imageReply(
    //    xcb_get_image_reply( *mConnection, imageCoockie, nullptr ) );
    //
    //    if ( !imageReply )
    //        throw std::runtime_error(
    //        "In Window::getImageData(): xcb_get_image_reply() return nullptr!" );
    //    std::uint8_t * startData = xcb_get_image_data( imageReply.get() );
    //    if ( !startData )
    //        throw std::runtime_error(
    //        "In Window::getImageData(): xcb_get_image_data() return nullptr!" );
    //
    //    std::vector< uint8_t > imageData(
    //    static_cast< std::size_t >( xcb_get_image_data_length( imageReply.get() ) ) );
    //    std::uint8_t * endData = startData + imageData.size();
    //    std::copy( startData, endData, imageData.begin() );
    //std::memcpy( imageData.data(), xcb_get_image_data( imageReply ), imageData.size() );

    //xcb_free_pixmap( *mConnection, winPixmap );
    //xcb_composite_unredirect_window(
    //*mConnection, mWindow, XCB_COMPOSITE_REDIRECT_AUTOMATIC );

    return shm;
}

void Window::release() {
    if ( mWindow != XCB_NONE )
        xcb_destroy_window( *mConnection, mWindow );
    mWindow = XCB_NONE;
}

void Window::map() {
    if ( !getProperties().isViewable() )
        xcb_map_window( *mConnection, mWindow );
}

void Window::unmap() {
    if ( getProperties().isViewable() )
        xcb_unmap_window( *mConnection, mWindow );
}

CompositeWindow::CompositeWindow() : CompositeWindow::Window() {}
CompositeWindow::CompositeWindow( CompositeWindow::CreateInfo ci ) :
CompositeWindow::Window( ci ) {}
CompositeWindow::~CompositeWindow() = default;

void CompositeWindow::release() {
    if ( mWindow != XCB_NONE )
        xcb_composite_release_overlay_window( *mConnection, mWindow );
    mWindow = XCB_NONE;
}
}   // namespace xcbwraper
