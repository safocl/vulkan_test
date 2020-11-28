#include <cassert>
#include <exception>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string_view>
#include <stdexcept>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "vulkanrender.hpp"

int main() {
    auto xcbConnect = xcb_connect( nullptr, nullptr );
    assert( xcbConnect != nullptr );

    auto screen =
    xcb_setup_roots_iterator( xcb_get_setup( xcbConnect ) ).data;
    assert( screen != nullptr );

    std::uint32_t winValList[] = { XCB_EVENT_MASK_EXPOSURE |
                                   XCB_EVENT_MASK_KEY_PRESS };

    xcb_window_t window = xcb_generate_id( xcbConnect );
    xcb_create_window( xcbConnect,
                       screen->root_depth,
                       window,
                       screen->root,
                       100,
                       100,
                       300,
                       300,
                       2,
                       XCB_WINDOW_CLASS_COPY_FROM_PARENT,
                       screen->root_visual,
                       XCB_CW_EVENT_MASK,
                       winValList );

    xcb_map_window( xcbConnect, window );

    xcb_flush( xcbConnect );

    core::renderer::Vulkan renderer( *xcbConnect, window );

    for ( auto event = xcb_wait_for_event( xcbConnect );
          event != nullptr;
          event = xcb_wait_for_event( xcbConnect ) ) {
        switch ( event->response_type & ~0x80 ) {
        case XCB_EXPOSE:
            renderer.draw();
            //xcb_flush( xcbConnect );
            break;
        case XCB_KEY_PRESS:
            if ( reinterpret_cast< xcb_key_press_event_t * >( event )
                 ->detail == 24 ){
                xcb_destroy_window(xcbConnect, window);
                xcb_disconnect(xcbConnect);
                std::abort();
            }
        }
        delete event;
    }


    return EXIT_SUCCESS;
}
