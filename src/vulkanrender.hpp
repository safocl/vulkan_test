#pragma once

#include <cstdint>
#include <vector>
#include <thread>
#include <mutex>

//#define VULKAN_HPP_NO_EXCEPTIONS
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_XLIB_XRANDR_EXT
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_xlib_xrandr.h>

#include "xcbwraper/posix-shm.hpp"
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

    explicit VulkanBase( const CreateInfo & );
    explicit VulkanBase( const VulkanBase & ) = delete;
    explicit VulkanBase( VulkanBase && )      = default;
    VulkanBase & operator=( const VulkanBase & ) = delete;
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
        xcbwraper::WindowShared        dstXcbWindow;
    };

    struct RamImageMapInfo final {
        vk::Buffer                  imageBuffer;
        vk::Image                   image;
        vk::DeviceMemory            imageRam;
        vk::DeviceMemory            imageBufferRam;
        void *                      imageDataBridge;
        posix::SharedMemory::Shared imageShm;
        xcbwraper::WindowShared     window;
    };

    struct CmdBufferInitConfig final {
        vk::CommandBufferBeginInfo cmdBufferBI { .flags = {} };

        std::array< vk::ImageSubresourceRange, 1 > ranges {
            vk::ImageSubresourceRange { .aspectMask =
                                        vk::ImageAspectFlagBits::eColor,
                                        .baseMipLevel   = 0,
                                        .levelCount     = 1,
                                        .baseArrayLayer = 0,
                                        .layerCount     = 1 }
        };

        vk::ImageMemoryBarrier srcImageInitMemoryBarier {
            .srcAccessMask = vk::AccessFlagBits::eTransferRead,
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout     = vk::ImageLayout::eUndefined,
            .newLayout     = vk::ImageLayout::eTransferDstOptimal,
        };

        vk::ImageMemoryBarrier bufferImageCopyMemoryBarier {
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout     = vk::ImageLayout::eTransferDstOptimal,
            .newLayout     = vk::ImageLayout::eTransferSrcOptimal,
        };

        vk::ImageMemoryBarrier imageMemoryBarierPresentToClear {
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout     = vk::ImageLayout::eUndefined,
            .newLayout     = vk::ImageLayout::eTransferDstOptimal,
        };

        vk::ImageMemoryBarrier imageMemoryBarierClearToPresent {
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eMemoryWrite,
            .oldLayout     = vk::ImageLayout::eTransferDstOptimal,
            .newLayout     = vk::ImageLayout::ePresentSrcKHR,
        };

        vk::ClearColorValue clearColorValue { std::array< float, 4 > {
        0.9f, 0.2f, 0.2f, 0.5f } };

        vk::Event needRefrashExtent;
        vk::Event extentIsActual;
        vk::Event bufferImageCopied;
    };

    struct DrawConfig final {
        std::array< vk::Flags< vk::PipelineStageFlagBits >, 1 > pipelineStageFlags {
            vk::PipelineStageFlagBits::eTransfer
        };

        std::array< vk::SubmitInfo, 1 > subInfo { vk::SubmitInfo {
        .waitSemaphoreCount   = 1,
        .pWaitDstStageMask    = pipelineStageFlags.data(),
        .commandBufferCount   = 1,
        .signalSemaphoreCount = 1,
        } };

        vk::PresentInfoKHR present { .waitSemaphoreCount = 1, .swapchainCount = 1 };
    };

    struct ThreadPool final {
        std::thread blitRegionsResizer;
    };

    VulkanGraphicRender(
    const VulkanBase::CreateInfo &          baseInfo,
    const VulkanGraphicRender::CreateInfo & graphicRenderCreateInfo );
    VulkanGraphicRender( const VulkanGraphicRender & ) = delete;
    VulkanGraphicRender( VulkanGraphicRender && )      = default;

    VulkanGraphicRender & operator=( const VulkanGraphicRender & ) = delete;
    VulkanGraphicRender & operator=( VulkanGraphicRender && ) = default;

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

    xcbwraper::XcbConnectionShared mXcbConnect;
    xcbwraper::WindowShared        mDstWindow;

    ImageVec                           mSwapchainImages;
    SemaphoresVec                      mSemaphores;
    RamImageMapInfo                    mSrcRamInfo;
    std::vector< vk::ImageBlit >       mSrcToDstBlitRegions;
    std::vector< vk::BufferImageCopy > mSrcBufferImgCopyRegions;

    DrawConfig          mDrawConf {};
    CmdBufferInitConfig mCmdConfig {};
    vk::Extent2D        mCurrentSurfaceExtent;
    vk::Extent2D        mCurrentSwapchainExtent;
    ThreadPool          mThreadPool;

    std::unique_ptr< std::mutex > mCurrentSwapchainExtentMutex;

    Display * mDpy;

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
