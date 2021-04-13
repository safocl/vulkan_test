#include <cassert>
#include <exception>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string_view>
#include <stdexcept>
//#define VK_USE_PLATFORM_XCB_KHR
//#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
//#include <vulkan/vulkan.hpp>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/composite.h>

#include "vulkanrender.hpp"

int main() {
    auto xcbConnect = xcb_connect( nullptr, nullptr );
    assert( xcbConnect != nullptr );

    auto screen = xcb_setup_roots_iterator( xcb_get_setup( xcbConnect ) ).data;
    assert( screen != nullptr );

    std::uint32_t winValList[] = {
        /*XCB_EVENT_MASK_EXPOSURE |*/ XCB_EVENT_MASK_KEY_PRESS
    };

    //    auto overlayReply = xcb_composite_get_overlay_window_reply(
    //    xcbConnect, xcb_composite_get_overlay_window( xcbConnect, screen->root ), nullptr );
    //
    //    xcb_window_t window /* = xcb_generate_id( xcbConnect )*/;
    //    if ( overlayReply ) {
    //        window = overlayReply->overlay_win;
    //    } else
    //        throw std::runtime_error( "overlay not presented." );
    //
    //    xcb_change_window_attributes( xcbConnect, window, XCB_CW_EVENT_MASK, winValList );

    xcb_window_t window = xcb_generate_id( xcbConnect );
    xcb_create_window( xcbConnect,
                       screen->root_depth,
                       window,
                       screen->root,
                       100,
                       100,
                       600,
                       300,
                       2,
                       XCB_WINDOW_CLASS_COPY_FROM_PARENT,
                       screen->root_visual,
                       XCB_CW_EVENT_MASK,
                       winValList );

    xcb_map_window( xcbConnect, window );

    xcb_flush( xcbConnect );

    auto appInfo = std::make_unique< vk::ApplicationInfo >(
    vk::ApplicationInfo { .pApplicationName   = "vulkan_xcb",
                          .applicationVersion = VK_MAKE_VERSION( 0, 0, 1 ),
                          .pEngineName        = "vulkan_xcb_engine",
                          .engineVersion      = VK_MAKE_VERSION( 0, 0, 1 ),
                          .apiVersion         = VK_API_VERSION_1_0 } );

    core::renderer::VulkanBase::Extensions extensions {
        .instance = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME },
        .device   = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }
    };
    vk::Instance vulkanXCBInstance = vk::createInstance( vk::InstanceCreateInfo {
    .pApplicationInfo        = appInfo.get(),
    .enabledExtensionCount   = static_cast< std::uint32_t >( extensions.instance.size() ),
    .ppEnabledExtensionNames = extensions.instance.data() } );

    auto gpu = core::renderer::getDiscreteGpu( vulkanXCBInstance );
    core::renderer::VulkanBase::CreateInfo vulkanBaseCI { .instance   = vulkanXCBInstance,
                                                          .physDev    = gpu,
                                                          .extansions = extensions };
    core::renderer::VulkanGraphicRender::CreateInfo vulkanRenderCI {
        /*.xcbConnect = xcbConnect,*/ .xcbWindow = window
    };

    core::renderer::VulkanGraphicRender renderer( std::move( vulkanBaseCI ),
                                                  std::move( vulkanRenderCI ) );
    for ( bool breakLoop = false; !breakLoop; ) {
        renderer.draw();
        auto event = xcb_poll_for_event( xcbConnect );
        if ( !event )
            continue;
        //for ( auto event = xcb_poll_for_event( xcbConnect ); event != nullptr;
        //      event      = xcb_poll_for_event( xcbConnect ) ) {
        switch ( event->response_type & ~0x80 ) {
            //case XCB_EXPOSE: renderer.draw(); break;

        case XCB_KEY_PRESS:
            if ( reinterpret_cast< xcb_key_press_event_t * >( event )->detail == 24 ) {
                breakLoop = true;
            }
        }
        delete event;
    }
    xcb_destroy_window( xcbConnect, window );
    xcb_flush( xcbConnect );
    xcb_disconnect( xcbConnect );

    return EXIT_SUCCESS;
}
