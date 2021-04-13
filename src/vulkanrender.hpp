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

    VulkanBase( CreateInfo && );
    VulkanBase( const VulkanBase & ) = default;
    VulkanBase( VulkanBase && )      = default;
    VulkanBase & operator=( const VulkanBase & ) = default;
    VulkanBase & operator=( VulkanBase && ) = default;
    virtual ~VulkanBase();

protected:
    static constexpr std::uint8_t nBuffers = 3;

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

class VulkanGraphicRender : public VulkanBase {
public:
    struct CreateInfo final {
        //        xcb_connection_t * xcbConnect;
        xcb_window_t xcbWindow;
    };

    VulkanGraphicRender( VulkanBase::CreateInfo &&          baseInfo,
                         VulkanGraphicRender::CreateInfo && graphicRenderCreateInfo );

    virtual ~VulkanGraphicRender();
    void draw();
    void update();
    void printSurfaceExtents() const;

protected:
    vk::SurfaceKHR   mSurface;
    vk::SwapchainKHR mSwapchain;
    vk::Device       mLogicDev;

    xcb_connection_t * mXcbConnect;
    xcb_window_t       mXcbWindow;

    ImageVec             mSwapchainImages;
    SemaphoresVec        mSemaphores;
    composite::Composite mComposite;
};

[[nodiscard]] std::vector< vk::CommandBuffer >
commandBuffersInit( const vk::Device &      logicDev,
                    const vk::CommandPool & commandPool,
                    std::uint32_t           swapchainImagesCount );

[[nodiscard]] vk::SwapchainKHR
swapchainInit( const vk::PhysicalDevice &          gpu,
               const vk::Device &                  logicDev,
               const vk::SurfaceKHR &              surface,
               const VulkanBase::QueueTypeConfig & graphicConf,
               vk::SwapchainKHR                    oldSwapchain = nullptr );

[[nodiscard]] vk::PhysicalDevice getDiscreteGpu( const vk::Instance & instance );

[[nodiscard]] QueueFamilyIndex
getGraphicsQueueFamilyIndex( const vk::PhysicalDevice & gpu );

void fillCmdBuffers( QueueFamilyIndex, CommandBuffersVec &, ImageVec & );

}   // namespace core::renderer
