#pragma once

#include "xcbwraper/xcbconnection.hpp"
#include "xcbwraper/window.hpp"

#include <xcb/composite.h>
#include <xcb/xcb.h>
#include <memory>

namespace core::composite {

class Composite final {
    xcbwraper::XcbConnectionShared mXcbConnection;
    xcbwraper::Window     mCompositeOverlayWindow;
    xcbwraper::Window     mRootWindow;

public:
    explicit Composite( xcbwraper::XcbConnectionShared );
    ~Composite();
    xcbwraper::Window getCompositeOverleyWindow() const;
    xcbwraper::Window getRootWindow() const;
};
}   // namespace core::composite
