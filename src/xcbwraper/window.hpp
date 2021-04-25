#include <bits/stdint-uintn.h>
#include <memory>
#include <vector>
#include <xcb/xproto.h>
#include "xcbconnection.hpp"
#include "xcbwindowprop.hpp"

namespace xcbwraper {

using XcbConnectionShared = std::shared_ptr< XCBConnection >;

class Window final {
    XcbConnectionShared mConnection;
    xcb_window_t        mWindow;

public:
    struct CreateInfo final {
        const XcbConnectionShared connection;
        const xcb_window_t        window;
    };

    Window();
    explicit Window( CreateInfo );
    ~Window();
    Window & operator=( xcb_window_t & );
             operator xcb_window_t() const;

    XCBWindowProp          getProperties() const;
    std::vector< uint8_t > getImageData() const;
};

}   // namespace xcbwraper
