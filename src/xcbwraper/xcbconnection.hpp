#pragma once

#include <xcb/xcb.h>

#include <memory>

namespace xcbwraper {

struct XCBConnection final {
    xcb_connection_t * mConnection;

    XCBConnection() : mConnection( xcb_connect( nullptr, nullptr ) ) {}
    ~XCBConnection() { xcb_disconnect( mConnection ); }
    operator xcb_connection_t *() { return mConnection; }
};

using XcbConnectionShared = std::shared_ptr< XCBConnection >;
}   // namespace xcbwraper
