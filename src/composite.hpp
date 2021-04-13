#pragma once

#include <xcb/composite.h>
#include <xcb/xcb.h>

namespace core::composite {

class Composite final {
    xcb_connection_t * mXcbConnection;
    xcb_window_t       mCompositeOverlayWindow;

public:
    Composite();
    ~Composite();
    xcb_window_t getCompositeOverleyWindow() const;
};
}   // namespace core::composite
