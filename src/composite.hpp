#pragma once

#include "xcbwraper/xcbconnection.hpp"
#include "xcbwraper/window.hpp"
#include "defines.hpp"

#include <xcb/composite.h>
#include <xcb/xcb.h>
#include <memory>
#include <list>

namespace core::composite {

class Composite final {
    xcbwraper::XcbConnectionShared mXcbConnection;
    //    xcbwraper::WindowShared        mCompositeOverlayWindow;
    //    xcbwraper::WindowShared        mRootWindow;

public:
    explicit Composite( xcbwraper::XcbConnectionShared );
    ~Composite();
    xcbwraper::WindowShared getCompositeOverleyWindow() const;
    xcbwraper::WindowShared getCompositeOverleyWindowWithoutInputEvents() const;
    xcbwraper::WindowShared getRootWindow() const;
    std::list< xcbwraper::WindowShared > getRedirectedWindows() const;
};
}   // namespace core::composite
