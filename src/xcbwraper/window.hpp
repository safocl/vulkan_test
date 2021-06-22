#include <bits/stdint-uintn.h>
#include <exception>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/dri3.h>

#include "xcbconnection.hpp"
#include "xcbwindowprop.hpp"
#include "posix-shm.hpp"

namespace xcbwraper {

class PixmapDri3Fd final {
    XcbConnectionShared                   mConnection;
    xcb_dri3_buffer_from_pixmap_reply_t * mReply;
    int                                   mFd;

public:
    struct CreateInfo final {
        XcbConnectionShared                   connection;
        xcb_dri3_buffer_from_pixmap_reply_t * reply;
    };

    explicit PixmapDri3Fd( const CreateInfo & );
    PixmapDri3Fd( const PixmapDri3Fd & ) = default;
    PixmapDri3Fd( PixmapDri3Fd && )      = default;
    ~PixmapDri3Fd();

    int getFd() const;
};

class PixmapData final {
public:
    using DataArr = std::span< std::uint8_t >;

private:
    XcbConnectionShared     mConnection { nullptr };
    xcb_get_image_reply_t * mReply {};
    DataArr                 mDataArr {};

public:
    PixmapData() = default;
    explicit PixmapData( XcbConnectionShared, xcb_get_image_reply_t * );
    //PixmapData(const PixmapData&) = default;
    //PixmapData(PixmapData&&) = default;
    ~PixmapData();

    //PixmapData& operator= (const PixmapData&) = default;
    DataArr getData() const;
    //int            size() const;
};

class Window {
protected:
    XcbConnectionShared mConnection {};
    xcb_window_t        mWindow { XCB_NONE };

public:
    struct CreateInfo final {
        XcbConnectionShared connection {};
        xcb_window_t        window { XCB_NONE };
    };

    Window();
    explicit Window( const CreateInfo & );
    Window( const Window & ) = default;
    Window( Window && )      = default;
    virtual ~Window();
    Window & operator=( const Window & ) = default;
    Window & operator=( Window && ) = default;

    Window & operator=( xcb_window_t );
             operator xcb_window_t() const;
             operator bool() const;

    [[nodiscard]] XCBWindowProp               getProperties() const;
    [[nodiscard]] posix::SharedMemory::Shared getImageData() const;
    [[nodiscard]] posix::SharedMemory::Shared
    getImageData( posix::SharedMemory::Shared shm ) const;
    [[nodiscard]] posix::SharedMemory::Shared getOffscreenShmImageData();
    [[nodiscard]] posix::SharedMemory::Shared
    getOffscreenShmImageData( posix::SharedMemory::Shared shm );
    [[nodiscard]] PixmapDri3Fd getImageDataDri3FD() const;
    [[nodiscard]] PixmapDri3Fd getOffscreenImageDataDri3FD();
    [[nodiscard]] PixmapData   getOffscreenImageData();
    virtual void               redirect();
    virtual void               unredirect();
    virtual void               release();
    virtual void               map();
    virtual void               unmap();
    virtual void               fullDamaged();
    void                       detach();
};

class CompositeWindow : public Window {
public:
    CompositeWindow();
    explicit CompositeWindow( CreateInfo );
    virtual ~CompositeWindow();

    virtual void release() override;
};

using WindowShared = std::shared_ptr< Window >;

}   // namespace xcbwraper
