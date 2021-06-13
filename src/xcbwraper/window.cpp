#include "window.hpp"
#include "errno-exception.hpp"
#include "posix-shm.hpp"
#include "xcbwindowprop.hpp"
#include "extension_query_version.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <exception>

#include <xcb/composite.h>
#include <xcb/shape.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xcb/xproto.h>
#include <xcb/shm.h>
#include <xcb/dri3.h>
#include <xcb/damage.h>

namespace xcbwraper {

PixmapDri3Fd::PixmapDri3Fd( const CreateInfo & ci ) :
mConnection( ci.connection ), mReply( ci.reply ) {
    dri3CheckQueryVersion( *mConnection, 1, 2 );

    mFd = xcb_dri3_buffer_from_pixmap_reply_fds( *mConnection, mReply )[ 0 ];

    xcb_flush( *mConnection );
}

PixmapDri3Fd::~PixmapDri3Fd() {
    if ( mReply )
        delete mReply;
}

int PixmapDri3Fd::getFd() const { return mFd; }

namespace {
void windowXcbExtensionsQueryVersion( xcb_connection_t * connection ) {
    compositeCheckQueryVersion( connection, 0, 4 );
    shmCheckQueryVersion( connection );
    xfixesCheckQueryVersion( connection, 5, 0 );
    shapeCheckQueryVersion( connection );
    dri3CheckQueryVersion( connection, 1, 2 );
    damageCheckQueryVersion( connection, 1, 1 );

    xcb_flush( connection );
}
}   // namespace

Window::Window() { windowXcbExtensionsQueryVersion( *mConnection ); }

Window::Window( const Window::CreateInfo & ci ) :
mConnection( ci.connection ), mWindow( ci.window ) {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error(
        "Window is null window id" );

    windowXcbExtensionsQueryVersion( *mConnection );
}

Window::~Window() {
    if ( *this ) {
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
    if ( mWindow == XCB_NONE )
        throw std::runtime_error(
        "Using Window::getImageData() for null window id" );

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

    xcb_flush( *mConnection );

    return shm;
}

PixmapDri3Fd Window::getImageDataDri3FD() const {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error(
        "Using Window::getImageData() for null window id" );

    if ( !getProperties().isViewable() )
        throw std::runtime_error(
        "Using Window::getImageData() for not viewvable window" );

    auto windowGeometry = getProperties().getGeometry();

    xcb_pixmap_t winPixmap = xcb_generate_id( *mConnection );

    auto screen = xcb_setup_roots_iterator( xcb_get_setup( *mConnection ) ).data;

    if ( !screen )
        throw std::runtime_error(
        "Using Window::getImageDataDri3FD() : xcb_setup_roots_iterator().data is nullptr" );

    xcb_create_pixmap( *mConnection,
                       screen->root_depth,
                       winPixmap,
                       mWindow,
                       windowGeometry.width,
                       windowGeometry.height );
    xcb_flush( *mConnection );

    auto coockie = xcb_dri3_buffer_from_pixmap( *mConnection, winPixmap );

    xcb_generic_error_t * err {};
    auto                  dri3BufferReply =
    xcb_dri3_buffer_from_pixmap_reply( *mConnection, coockie, &err );

    if ( err )
        throw std::runtime_error(
        "Using Window::getImageDataDri3FD() : ERROR with code is " +
        std::to_string( err->error_code ) );

    if ( !dri3BufferReply )
        throw std::runtime_error(
        "Using Window::getImageDataDri3FD() : xcb_dri3_buffer_from_pixmap_reply() return is nullptr" );

    return PixmapDri3Fd( PixmapDri3Fd::CreateInfo { .connection = mConnection,
                                                    .reply = dri3BufferReply } );
}

PixmapDri3Fd Window::getOffscreenImageDataDri3FD() {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error(
        "Using Window::getOffscreenImageDataDri3FD() for null window id" );

    if ( !getProperties().isViewable() )
        throw std::runtime_error(
        "Using Window::getOffscreenImageDataDri3FD() for not viewvable window" );

    xcb_pixmap_t pixmap = xcb_generate_id( *mConnection );

    redirect();

    xcb_composite_name_window_pixmap( *mConnection, *this, pixmap );

    auto coockie = xcb_dri3_buffer_from_pixmap( *mConnection, pixmap );

    xcb_generic_error_t * err {};
    auto                  dri3BufferReply =
    xcb_dri3_buffer_from_pixmap_reply( *mConnection, coockie, &err );

    if ( err )
        throw std::runtime_error(
        "Using Window::getImageDataDri3FD() : ERROR with code is " +
        std::to_string( err->error_code ) );

    if ( !dri3BufferReply )
        throw std::runtime_error(
        "Using Window::getOffscreenImageDataDri3FD() : xcb_dri3_buffer_from_pixmap_reply() return is nullptr" );

    xcb_flush( *mConnection );

    return PixmapDri3Fd( PixmapDri3Fd::CreateInfo { .connection = mConnection,
                                                    .reply = dri3BufferReply } );
}

void Window::redirect() {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error( "Using Window::release() for null window id" );

    xcb_composite_redirect_window(
    *mConnection, *this, XCB_COMPOSITE_REDIRECT_MANUAL );

    xcb_flush( *mConnection );
}

void Window::unredirect() {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error( "Using Window::release() for null window id" );

    xcb_composite_unredirect_window(
    *mConnection, *this, XCB_COMPOSITE_REDIRECT_MANUAL );

    xcb_flush( *mConnection );
}

void Window::release() {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error( "Using Window::release() for null window id" );

    xcb_destroy_window( *mConnection, mWindow );
    mWindow = XCB_NONE;
    xcb_flush( *mConnection );
}

void Window::map() {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error( "Using Window::map() for null window id" );

    if ( !getProperties().isViewable() )
        xcb_map_window( *mConnection, mWindow );
    xcb_flush( *mConnection );
}

void Window::unmap() {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error( "Using Window::unmap() for null window id" );

    if ( getProperties().isViewable() )
        xcb_unmap_window( *mConnection, mWindow );
    xcb_flush( *mConnection );
}

void Window::fullDamaged() {
    if ( mWindow == XCB_NONE )
        throw std::runtime_error( "Using Window::fullDamaged() for null window id" );

    xcb_xfixes_region_t region = xcb_generate_id( *mConnection );

    xcb_xfixes_create_region_from_window(
    *mConnection, region, *this, XCB_SHAPE_SK_BOUNDING );

    xcb_damage_damage_t damage = xcb_generate_id( *mConnection );

    xcb_damage_create(
    *mConnection, damage, *this, XCB_DAMAGE_REPORT_LEVEL_BOUNDING_BOX );

    xcb_damage_add( *mConnection, *this, region );

    xcb_damage_destroy( *mConnection, damage );

    xcb_xfixes_destroy_region( *mConnection, region );

    xcb_flush( *mConnection );
}

CompositeWindow::CompositeWindow() : CompositeWindow::Window() {}
CompositeWindow::CompositeWindow( CompositeWindow::CreateInfo ci ) :
CompositeWindow::Window( ci ) {}
CompositeWindow::~CompositeWindow() = default;

void CompositeWindow::release() {
    if ( mWindow != XCB_NONE )
        xcb_composite_release_overlay_window( *mConnection, mWindow );
    mWindow = XCB_NONE;
    xcb_flush( *mConnection );
}
}   // namespace xcbwraper
