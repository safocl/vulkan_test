#include <bits/stdint-uintn.h>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <xcb/xproto.h>

#include "xcbconnection.hpp"
#include "xcbwindowprop.hpp"
#include "posix-shm.hpp"

namespace xcbwraper {

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
    explicit Window( CreateInfo );
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
    virtual void release();
    virtual void map();
    virtual void unmap();
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
