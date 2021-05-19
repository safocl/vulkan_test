#include <bits/stdint-uintn.h>
#include <memory>
#include <vector>
#include <xcb/xproto.h>
#include "xcbconnection.hpp"
#include "xcbwindowprop.hpp"

namespace xcbwraper {

class Window final {
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
    ~Window();
    Window & operator=( const Window & ) = default;
    Window & operator=( Window && ) = default;
    Window & operator=( xcb_window_t & );
             operator xcb_window_t() const;
             operator bool() const;

    [[nodiscard]] XCBWindowProp getProperties() const;
    std::vector< uint8_t >      getImageData() const;
};

using WindowShared = std::shared_ptr< Window >;

}   // namespace xcbwraper
