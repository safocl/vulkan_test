#pragma once

#include <xcb/xcb.h>


namespace xcbwraper {
struct XCBConnect final {
    xcb_connection_t * connect;

    XCBConnect() : connect( xcb_connect( nullptr, nullptr ) ) {}
    ~XCBConnect() { xcb_disconnect( connect ); }
    operator xcb_connection_t *() { return connect; }
};
}
