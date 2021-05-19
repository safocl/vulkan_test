#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

//#define VULKAN_HPP_NO_EXCEPTIONS
#define VK_USE_PLATFORM_XCB_KHR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS

#include <vulkan/vulkan.hpp>

#include "composite.hpp"
#include "xcbwraper/xcbconnection.hpp"
#include "render.hpp"

namespace core::renderer {

using ExtensionsVec          = std::vector< const char * >;
using DeviceQueueCreateInfos = std::vector< vk::DeviceQueueCreateInfo >;
using QueueFamilyIndex       = std::uint32_t;
using SemaphoresVec          = std::vector< vk::Semaphore >;
using ImageVec               = std::vector< vk::Image >;
using QueuesVec              = std::vector< vk::Queue >;
using QueuesPriority         = float;
using QueuesPrioritiesVec    = std::vector< QueuesPriority >;
using CommandBuffersVec      = std::vector< vk::CommandBuffer >;

class VulkanBase {
public:
    struct QueueTypeConfig final {
        QueueFamilyIndex    queueFamilyIndex;
        QueuesPrioritiesVec priorities;
    };

    struct Extensions final {
        ExtensionsVec instance;
        ExtensionsVec device;
    };

    struct CreateInfo final {
        vk::Instance       instance;
        vk::PhysicalDevice physDev;
        Extensions         extansions;
    };

    using QueueTypeConfigsVec = std::vector< QueueTypeConfig >;

    explicit VulkanBase( CreateInfo && );
    explicit VulkanBase( const VulkanBase & ) = default;
    explicit VulkanBase( VulkanBase && )      = default;
    VulkanBase & operator=( const VulkanBase & ) = default;
    VulkanBase & operator=( VulkanBase && ) = default;
    virtual ~VulkanBase();

protected:
    //ExtensionsVec instanseExtensions {
    //    VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME /*,
    //    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    //    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME */
    //};
    //ExtensionsVec deviceExtensions {
    //    VK_KHR_SWAPCHAIN_EXTENSION_NAME /*,
    //    VK_KHR_SHARED_PRESENTABLE_IMAGE_EXTENSION_NAME */
    //};

    vk::Instance       mInstance;
    vk::PhysicalDevice mGpu;
    vk::CommandPool    mCommandPool;

    Extensions        mExtansions;
    CommandBuffersVec mCommandBuffers;
    QueuesVec         mQueues;

    QueueTypeConfigsVec mQueueConfigs;
};

class VulkanGraphicRender : public Renderer, public VulkanBase {
public:
    struct CreateInfo final {
        xcbwraper::XcbConnectionShared xcbConnect;
        xcb_window_t                   xcbWindow;
    };

    VulkanGraphicRender(
    VulkanBase::CreateInfo &&          baseInfo,
    VulkanGraphicRender::CreateInfo && graphicRenderCreateInfo );

    virtual ~VulkanGraphicRender();
    void draw() override;
    void update();
    void printSurfaceExtents() const;

protected:
    static constexpr std::uint8_t countSwapChainBuffers = 3;

    vk::SurfaceKHR       mSurface;
    vk::SurfaceFormatKHR mSurfaceFormat;
    vk::SwapchainKHR     mSwapchain;
    vk::Device           mLogicDev;
    CommandBuffersVec    mSwapchainCmdBuffers;

    //    xcbwraper::XCBConnection mXcbConnect;
    xcbwraper::WindowShared mDstWindow;

    ImageVec      mSwapchainImages;
    SemaphoresVec mSemaphores;
    //vk::Buffer                         mSrcImageBuffer;
    vk::Image                    mSrcImage;
    vk::DeviceMemory             mSrcRawImage;
    std::vector< vk::ImageBlit > mSrcToDstRegions;
    xcbwraper::WindowShared      mSrcWindow;
    //    composite::Composite mComposite;
};

class VulkanRenderInstance final {
    VulkanRenderInstance();

public:
    using Shared = std::shared_ptr< VulkanRenderInstance >;

    ~VulkanRenderInstance();

    static Shared init();
    void          run() const;

private:
    static Shared mInstance;

    xcbwraper::XcbConnectionShared mXcbConnect;
};

}   // namespace core::renderer
