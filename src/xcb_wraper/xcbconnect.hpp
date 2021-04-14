#pragma once

#include <xcb/xcb.h>


namespace xcbwraper {
struct XCBConnect final {
    xcb_connection_t * mConnect;

    XCBConnect() : mConnect( xcb_connect( nullptr, nullptr ) ) {}
    ~XCBConnect() { xcb_disconnect( mConnect ); }
    operator xcb_connection_t *() { return mConnect; }
};
}
