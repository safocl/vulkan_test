#pragma once

#include <cstdint>
#include <vector>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define VK_USE_PLATFORM_XCB_KHR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS

#include <vulkan/vulkan.hpp>

namespace core::renderer {

class Vulkan final {
public:
    using ExtensionsVec    = std::vector< const char * >;
    using QueueFamilyIndex = std::uint32_t;
    using DeviceQueueCreateInfos =
    std::vector< vk::DeviceQueueCreateInfo >;

    struct QueueTypeConfig final {
        vk::Device                       logicDev;
        vk::Queue                        queue;
        vk::CommandPool                  commandPool;
        QueueFamilyIndex                 queueFamilyIndex = 0;
        std::vector< vk::CommandBuffer > commandBuffers;
    };


    Vulkan(xcb_connection_t &, xcb_window_t  );
    Vulkan( const Vulkan & ) = default;
    Vulkan( Vulkan && )      = default;
    Vulkan & operator=( const Vulkan & ) = default;
    Vulkan & operator=( Vulkan && ) = default;
    ~Vulkan()                       = default;

    void draw();

private:
    static constexpr std::uint8_t nBuffers = 3;

    ExtensionsVec instanseExtensions {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XCB_SURFACE_EXTENSION_NAME
    };
    ExtensionsVec deviceExtensions {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    vk::Instance       instance;
    vk::PhysicalDevice gpu;
    vk::SurfaceKHR     surface;
    vk::SwapchainKHR swapchain;
    vk::Semaphore semImageAvaible;
    vk::Semaphore semRenderingFinished;

    QueueTypeConfig graphicConf;

    std::vector <vk::Image> swapchainImages;

};

}   // namespace core::renderer
