#include "vulkanrender.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <xcb/xcb.h>
#include <thread>
#include <chrono>

namespace core::renderer {

static std::vector< vk::CommandBuffer >
commandBuffersInit( const Vulkan::QueueTypeConfig & graphicConf,
                    std::uint32_t swapchainImagesCount ) {
    auto commandBufferAI =
    std::make_unique< vk::CommandBufferAllocateInfo >(
    vk::CommandBufferAllocateInfo {
    .commandPool        = graphicConf.commandPool,
    .level              = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = swapchainImagesCount } );

    auto commandBuffers =
    graphicConf.logicDev.allocateCommandBuffers( *commandBufferAI );

    return commandBuffers;
}

static vk::SwapchainKHR
swapchainInit( const vk::PhysicalDevice &      gpu,
               const vk::SurfaceKHR &          surface,
               const Vulkan::QueueTypeConfig & graphicConf ) {
    auto gpuSurfaceFormats = gpu.getSurfaceFormatsKHR( surface );
    auto gpuSurfaceFormat =
    std::make_unique< vk::SurfaceFormatKHR >();

    for ( auto && gpuSurfaceFormat : gpuSurfaceFormats )
        std::cout << vk::to_string( gpuSurfaceFormat.format )
                  << std::endl
                  << vk::to_string( gpuSurfaceFormat.colorSpace )
                  << std::endl;

    std::cout << std::endl;

    if ( gpuSurfaceFormats.size() == 1 &&
         gpuSurfaceFormats[ 0 ].format == vk::Format::eUndefined )
        *gpuSurfaceFormat =
        vk::SurfaceFormatKHR { .format = vk::Format::eB8G8R8A8Unorm,
                               .colorSpace =
                               vk::ColorSpaceKHR::eSrgbNonlinear };
    else
        for ( auto && gpuSF : gpuSurfaceFormats ) {
            if ( gpuSF.format == vk::Format::eB8G8R8A8Unorm &&
                 gpuSF.colorSpace ==
                 vk::ColorSpaceKHR::eSrgbNonlinear )
                *gpuSurfaceFormat = gpuSF;
        }

    if ( gpuSurfaceFormat->format != vk::Format::eB8G8R8A8Unorm )
        *gpuSurfaceFormat = gpuSurfaceFormats.at( 0 );

    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
    for ( auto && pm : gpu.getSurfacePresentModesKHR( surface ) ) {
        std::cout << vk::to_string( pm ) << std::endl;
        if ( pm == vk::PresentModeKHR::eMailbox ) {
            presentMode = pm;
            break;
        }
    }
    std::cout << "Present mode is " << vk::to_string( presentMode )
              << std::endl;

    std::cout
    << gpu.getSurfaceCapabilitiesKHR( surface ).currentExtent.height
    << std::endl
    << gpu.getSurfaceCapabilitiesKHR( surface ).currentExtent.width
    << std::endl;

    vk::SwapchainCreateInfoKHR swapchainCI {
        //        .flags =
        //        vk::SwapchainCreateFlagBitsKHR::eMutableFormat,
        .surface = surface,
        .minImageCount =
        gpu.getSurfaceCapabilitiesKHR( surface ).maxImageCount < 3
        ? gpu.getSurfaceCapabilitiesKHR( surface ).maxImageCount
        : 3,
        .imageFormat     = gpuSurfaceFormat->format,
        .imageColorSpace = gpuSurfaceFormat->colorSpace,
        .imageExtent =
        gpu.getSurfaceCapabilitiesKHR( surface ).currentExtent,
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment |
                      vk::ImageUsageFlagBits::eTransferDst,
        .imageSharingMode      = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = graphicConf.queueFamilyIndex,
        .preTransform =
        gpu.getSurfaceCapabilitiesKHR( surface ).currentTransform,
        .presentMode = presentMode,
        .clipped     = VK_TRUE
    };

    vk::SwapchainKHR swapchain =
    graphicConf.logicDev.createSwapchainKHR( swapchainCI );
    std::cout << std::endl << "Swapchain is created" << std::endl;
    return swapchain;
}

static vk::PhysicalDevice
getDiscreteGpu( const vk::Instance & instance ) {
    const std::vector< vk::PhysicalDevice > gpus =
    instance.enumeratePhysicalDevices();

    for ( auto && gpu : gpus ) {
        if ( gpu.getProperties().deviceType ==
             vk::PhysicalDeviceType::eDiscreteGpu )
            return gpu;
    }

    throw std::runtime_error( "Not matched discrete GPU" );

    return vk::PhysicalDevice();
}

static Vulkan::QueueFamilyIndex
getGraphicsQueueFamilyIndex( const vk::PhysicalDevice & gpu ) {
    const auto queueFamilyProps = gpu.getQueueFamilyProperties();

    for ( Vulkan::QueueFamilyIndex queueFamilyIndex = 0;
          queueFamilyIndex < queueFamilyProps.size();
          ++queueFamilyIndex )
        if ( queueFamilyProps[ queueFamilyIndex ].queueFlags &
             vk::QueueFlagBits::eGraphics )
            return queueFamilyIndex;

    throw std::runtime_error( "Not matched graphycs queue family" );

    return Vulkan::QueueFamilyIndex();
}

Vulkan::Vulkan( xcb_connection_t & xcbConnect,
                xcb_window_t       xcbWindow ) {
    auto appInfo =
    std::make_unique< vk::ApplicationInfo >( vk::ApplicationInfo {
    .pApplicationName   = "vulkan_xcb",
    .applicationVersion = VK_MAKE_VERSION( 0, 0, 1 ),
    .pEngineName        = "vulkan_xcb_engine",
    .engineVersion      = VK_MAKE_VERSION( 0, 0, 1 ),
    .apiVersion         = VK_API_VERSION_1_0 } );

    instance = vk::createInstance( vk::InstanceCreateInfo {
    .pApplicationInfo = appInfo.get(),
    .enabledExtensionCount =
    static_cast< std::uint32_t >( instanseExtensions.size() ),
    .ppEnabledExtensionNames = instanseExtensions.data() } );

    gpu = getDiscreteGpu( instance );
    std::cout << "Discrete GPU is : "
              << gpu.getProperties().deviceName << std::endl;

    graphicConf.queueFamilyIndex = getGraphicsQueueFamilyIndex( gpu );

    std::vector< vk::DeviceQueueCreateInfo > deviceQueueCreateInfos;
    float                                    priorities[] = { 1.0f };

    deviceQueueCreateInfos.push_back( vk::DeviceQueueCreateInfo {
    .queueFamilyIndex = graphicConf.queueFamilyIndex,
    .queueCount       = 1,
    .pQueuePriorities = priorities } );

    auto gpuFeatures = gpu.getFeatures();

    vk::DeviceCreateInfo deviceCreateInfo {
        .queueCreateInfoCount =
        static_cast< std::uint32_t >( deviceQueueCreateInfos.size() ),
        .pQueueCreateInfos = deviceQueueCreateInfos.data(),
        .enabledExtensionCount =
        static_cast< std::uint32_t >( deviceExtensions.size() ),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures        = &gpuFeatures
    };

    graphicConf.logicDev = gpu.createDevice( deviceCreateInfo );

    graphicConf.queue =
    graphicConf.logicDev.getQueue( graphicConf.queueFamilyIndex, 0 );

    vk::XcbSurfaceCreateInfoKHR surfaceCI { .connection = &xcbConnect,
                                            .window     = xcbWindow };

    surface = instance.createXcbSurfaceKHR( surfaceCI );

    if ( !gpu.getSurfaceSupportKHR( graphicConf.queueFamilyIndex,
                                    surface ) )
        throw std::runtime_error(
        "Gpu is not support surface with queue family index is " +
        std::to_string( graphicConf.queueFamilyIndex ) );

    graphicConf.commandPool = graphicConf.logicDev.createCommandPool(
    vk::CommandPoolCreateInfo {
    .flags            = vk::CommandPoolCreateFlagBits::eTransient,
    .queueFamilyIndex = graphicConf.queueFamilyIndex } );

    swapchain = swapchainInit( gpu, surface, graphicConf );

    swapchainImages =
    graphicConf.logicDev.getSwapchainImagesKHR( swapchain );

    graphicConf.commandBuffers =
    commandBuffersInit( graphicConf, swapchainImages.size() );

    vk::CommandBufferBeginInfo cmdBufferBI {
        .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse
    };

    const vk::ClearColorValue clearColorValue(
    std::array< float, 4 > { 1.0f, 0.5f, 0.0f, 1.0f } );

    const vk::ArrayProxy< const vk::ImageSubresourceRange > ranges {
        vk::ImageSubresourceRange { .aspectMask =
                                    vk::ImageAspectFlagBits::eColor,
                                    .baseMipLevel   = 0,
                                    .levelCount     = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1 }
    };

    for ( std::uint32_t i = 0; i < swapchainImages.size(); ++i ) {
        const vk::ArrayProxy< const vk::ImageMemoryBarrier >
        imageMemoryBarierPresentToClear { vk::ImageMemoryBarrier {
        .srcAccessMask       = vk::AccessFlagBits::eMemoryRead,
        .dstAccessMask       = vk::AccessFlagBits::eTransferWrite,
        .oldLayout           = vk::ImageLayout::eUndefined,
        .newLayout           = vk::ImageLayout::eTransferDstOptimal,
        .srcQueueFamilyIndex = graphicConf.queueFamilyIndex,
        .dstQueueFamilyIndex = graphicConf.queueFamilyIndex,
        .image               = swapchainImages[ i ],
        .subresourceRange    = ranges.front() } };

        const vk::ArrayProxy< const vk::ImageMemoryBarrier >
        imageMemoryBarierClearToPresent { vk::ImageMemoryBarrier {
        .srcAccessMask       = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask       = vk::AccessFlagBits::eMemoryWrite,
        .oldLayout           = vk::ImageLayout::eTransferDstOptimal,
        .newLayout           = vk::ImageLayout::ePresentSrcKHR,
        .srcQueueFamilyIndex = graphicConf.queueFamilyIndex,
        .dstQueueFamilyIndex = graphicConf.queueFamilyIndex,
        .image               = swapchainImages[ i ],
        .subresourceRange    = ranges.front() } };

        graphicConf.commandBuffers[ i ].begin( cmdBufferBI );

        graphicConf.commandBuffers[ i ].pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlags(),
        {},
        {},
        imageMemoryBarierPresentToClear );

        graphicConf.commandBuffers[ i ].clearColorImage(
        swapchainImages[ i ],
        vk::ImageLayout::eTransferDstOptimal,
        clearColorValue,
        ranges );

        graphicConf.commandBuffers[ i ].pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::DependencyFlags(),
        {},
        {},
        imageMemoryBarierClearToPresent  );

        graphicConf.commandBuffers[ i ].end();
    }

    semImageAvaible      = graphicConf.logicDev.createSemaphore( {} );
    semRenderingFinished = graphicConf.logicDev.createSemaphore( {} );
}

void Vulkan::draw() {
    const vk::ArrayProxy<
    const vk::Flags< vk::PipelineStageFlagBits > >
    pipelineStageFlags { vk::PipelineStageFlagBits::eTransfer };

    std::uint32_t asqNextImgIndex =
    graphicConf.logicDev
    .acquireNextImageKHR( swapchain,
                          std::numeric_limits< std::uint32_t >::max(),
                          semImageAvaible )
    .value;

    const vk::ArrayProxy< const vk::SubmitInfo > subInfo {
        vk::SubmitInfo {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &semImageAvaible,
        .pWaitDstStageMask  = pipelineStageFlags.begin(),
        .commandBufferCount = 1,
        .pCommandBuffers =
        &graphicConf.commandBuffers[ asqNextImgIndex ],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &semRenderingFinished }
    };

    graphicConf.queue.submit( subInfo );

    vk::PresentInfoKHR present { .waitSemaphoreCount = 1,
                                 .pWaitSemaphores =
                                 &semRenderingFinished,
                                 .swapchainCount = 1,
                                 .pSwapchains    = &swapchain,
                                 .pImageIndices  = &asqNextImgIndex };
    if ( !( graphicConf.queue.presentKHR( present ) ==
            vk::Result::eSuccess ) )
        throw std::runtime_error( "Fail presenting" );
}

}   // namespace core::renderer
