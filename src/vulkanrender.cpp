#include "vulkanrender.hpp"
#include "composite.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
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

std::vector< vk::CommandBuffer >
commandBuffersInit( const vk::Device &      logicDev,
                    const vk::CommandPool & commandPool,
                    std::uint32_t           swapchainImagesCount ) {
    auto commandBufferAI = std::make_unique< vk::CommandBufferAllocateInfo >(
    vk::CommandBufferAllocateInfo { .commandPool = commandPool,
                                    .level       = vk::CommandBufferLevel::ePrimary,
                                    .commandBufferCount = swapchainImagesCount } );

    auto commandBuffers = logicDev.allocateCommandBuffers( *commandBufferAI );

    return commandBuffers;
}

vk::SwapchainKHR swapchainInit( const vk::PhysicalDevice &          gpu,
                                const vk::Device &                  logicDev,
                                const vk::SurfaceKHR &              surface,
                                const VulkanBase::QueueTypeConfig & queueConf,
                                vk::SwapchainKHR                    oldSwapchain ) {
    auto gpuSurfaceFormats = gpu.getSurfaceFormatsKHR( surface );
    auto gpuSurfaceFormat  = std::make_unique< vk::SurfaceFormatKHR >();
    for ( auto && gpuSurfaceFormat : gpuSurfaceFormats )
        std::cout << vk::to_string( gpuSurfaceFormat.format ) << std::endl
                  << vk::to_string( gpuSurfaceFormat.colorSpace ) << std::endl;

    std::cout << std::endl;

    if ( gpuSurfaceFormats.size() == 1 &&
         gpuSurfaceFormats.at( 0 ).format == vk::Format::eUndefined )
        *gpuSurfaceFormat =
        vk::SurfaceFormatKHR { .format     = vk::Format::eB8G8R8A8Unorm,
                               .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear };
    else
        for ( auto && gpuSF : gpuSurfaceFormats ) {
            if ( gpuSF.format == vk::Format::eB8G8R8A8Unorm &&
                 gpuSF.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear )
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
    std::cout << "Present mode is " << vk::to_string( presentMode ) << std::endl;

    //    std::cout << gpu.getSurfaceCapabilitiesKHR( surface ).currentExtent.height
    //              << std::endl
    //              << gpu.getSurfaceCapabilitiesKHR( surface ).currentExtent.width
    //              << std::endl;

    vk::SwapchainCreateInfoKHR swapchainCI {
        //        .flags =
        //        vk::SwapchainCreateFlagBitsKHR::eMutableFormat,
        .surface       = surface,
        .minImageCount = std::min< std::uint32_t >(
        gpu.getSurfaceCapabilitiesKHR( surface ).maxImageCount, 3 ),
        .imageFormat      = gpuSurfaceFormat->format,
        .imageColorSpace  = gpuSurfaceFormat->colorSpace,
        .imageExtent      = gpu.getSurfaceCapabilitiesKHR( surface ).currentExtent,
        .imageArrayLayers = 1,
        .imageUsage =
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
        .imageSharingMode      = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = queueConf.queueFamilyIndex,
        .preTransform = gpu.getSurfaceCapabilitiesKHR( surface ).currentTransform,
        .presentMode  = presentMode,
        .clipped      = VK_FALSE,
        .oldSwapchain = oldSwapchain
    };

    vk::SwapchainKHR swapchain = logicDev.createSwapchainKHR( swapchainCI );
    std::cout << std::endl << "Swapchain is created" << std::endl;
    return swapchain;
}

vk::PhysicalDevice getDiscreteGpu( const vk::Instance & instance ) {
    const std::vector< vk::PhysicalDevice > gpus = instance.enumeratePhysicalDevices();

    for ( auto && gpu : gpus ) {
        if ( gpu.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu )
            return gpu;
    }

    throw std::runtime_error( "Not matched discrete GPU" );

    return vk::PhysicalDevice();
}

QueueFamilyIndex getGraphicsQueueFamilyIndex( const vk::PhysicalDevice & gpu ) {
    const auto queueFamilyProps = gpu.getQueueFamilyProperties();

    for ( QueueFamilyIndex queueFamilyIndex = 0;
          queueFamilyIndex < queueFamilyProps.size();
          ++queueFamilyIndex )
        if ( queueFamilyProps[ queueFamilyIndex ].queueFlags &
             vk::QueueFlagBits::eGraphics )
            return queueFamilyIndex;

    throw std::runtime_error( "Not matched graphycs queue family" );

    return QueueFamilyIndex();
}

void fillCmdBuffers( QueueFamilyIndex    queueFamilyIndex,
                     CommandBuffersVec & commandBuffers,
                     ImageVec &          swapchainImages ) {
    vk::CommandBufferBeginInfo cmdBufferBI {
        .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse
    };

    const vk::ClearColorValue clearColorValue(
    std::array< float, 4 > { 0.8f, 0.5f, 0.0f, 0.5f } );

    const std::array< const vk::ImageSubresourceRange, 1 > ranges {
        vk::ImageSubresourceRange { .aspectMask     = vk::ImageAspectFlagBits::eColor,
                                    .baseMipLevel   = 0,
                                    .levelCount     = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1 }
    };

    auto imageMemoryBarierPresentToClear = std::make_unique< vk::ImageMemoryBarrier >(
    vk::ImageMemoryBarrier { .srcAccessMask       = vk::AccessFlagBits::eMemoryRead,
                             .dstAccessMask       = vk::AccessFlagBits::eTransferWrite,
                             .oldLayout           = vk::ImageLayout::eUndefined,
                             .newLayout           = vk::ImageLayout::eTransferDstOptimal,
                             .srcQueueFamilyIndex = queueFamilyIndex,
                             .dstQueueFamilyIndex = queueFamilyIndex,
                             .subresourceRange    = ranges.at( 0 ) } );

    auto imageMemoryBarierClearToPresent = std::make_unique< vk::ImageMemoryBarrier >(
    vk::ImageMemoryBarrier { .srcAccessMask       = vk::AccessFlagBits::eTransferWrite,
                             .dstAccessMask       = vk::AccessFlagBits::eMemoryWrite,
                             .oldLayout           = vk::ImageLayout::eTransferDstOptimal,
                             .newLayout           = vk::ImageLayout::ePresentSrcKHR,
                             .srcQueueFamilyIndex = queueFamilyIndex,
                             .dstQueueFamilyIndex = queueFamilyIndex,
                             .subresourceRange    = ranges.at( 0 ) } );

    for ( std::uint32_t i = 0; i < swapchainImages.size(); ++i ) {
        imageMemoryBarierClearToPresent->image = swapchainImages.at( i );
        imageMemoryBarierPresentToClear->image = swapchainImages.at( i );

        commandBuffers.at( i ).begin( cmdBufferBI );

        commandBuffers.at( i ).pipelineBarrier( vk::PipelineStageFlagBits::eTransfer,
                                                vk::PipelineStageFlagBits::eTransfer,
                                                vk::DependencyFlags(),
                                                {},
                                                {},
                                                { *imageMemoryBarierPresentToClear } );

        commandBuffers.at( i ).clearColorImage( swapchainImages.at( i ),
                                                vk::ImageLayout::eTransferDstOptimal,
                                                clearColorValue,
                                                ranges );

        commandBuffers.at( i ).pipelineBarrier( vk::PipelineStageFlagBits::eTransfer,
                                                vk::PipelineStageFlagBits::eBottomOfPipe,
                                                vk::DependencyFlags(),
                                                {},
                                                {},
                                                { *imageMemoryBarierClearToPresent } );

        commandBuffers.at( i ).end();
    }
}

void fillCmdBuffersForPresentComposite( QueueFamilyIndex    queueFamilyIndex,
                                        CommandBuffersVec & commandBuffers,
                                        ImageVec &          swapchainImages ) {
    vk::CommandBufferBeginInfo cmdBufferBI {
        .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse
    };

    const std::array< const vk::ImageSubresourceRange, 1 > ranges {
        vk::ImageSubresourceRange { .aspectMask = vk::ImageAspectFlagBits::eColor |
                                                  vk::ImageAspectFlagBits::eDepth,
                                    .baseMipLevel   = 0,
                                    .levelCount     = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 2 }
    };

    auto imageMemoryBarierPresentToClear = std::make_unique< vk::ImageMemoryBarrier >(
    vk::ImageMemoryBarrier { .srcAccessMask       = vk::AccessFlagBits::eMemoryRead,
                             .dstAccessMask       = vk::AccessFlagBits::eTransferWrite,
                             .oldLayout           = vk::ImageLayout::eUndefined,
                             .newLayout           = vk::ImageLayout::eTransferDstOptimal,
                             .srcQueueFamilyIndex = queueFamilyIndex,
                             .dstQueueFamilyIndex = queueFamilyIndex,
                             .subresourceRange    = ranges.at( 0 ) } );

    auto imageMemoryBarierClearToPresent = std::make_unique< vk::ImageMemoryBarrier >(
    vk::ImageMemoryBarrier { .srcAccessMask       = vk::AccessFlagBits::eTransferWrite,
                             .dstAccessMask       = vk::AccessFlagBits::eMemoryWrite,
                             .oldLayout           = vk::ImageLayout::eTransferDstOptimal,
                             .newLayout           = vk::ImageLayout::ePresentSrcKHR,
                             .srcQueueFamilyIndex = queueFamilyIndex,
                             .dstQueueFamilyIndex = queueFamilyIndex,
                             .subresourceRange    = ranges.at( 0 ) } );

    for ( std::uint32_t i = 0; i < swapchainImages.size(); ++i ) {
        imageMemoryBarierClearToPresent->image = swapchainImages.at( i );
        imageMemoryBarierPresentToClear->image = swapchainImages.at( i );

        commandBuffers.at( i ).begin( cmdBufferBI );

        commandBuffers.at( i ).pipelineBarrier( vk::PipelineStageFlagBits::eTransfer,
                                                vk::PipelineStageFlagBits::eTransfer,
                                                vk::DependencyFlags(),
                                                {},
                                                {},
                                                { *imageMemoryBarierPresentToClear } );

        commandBuffers.at( i ).pipelineBarrier( vk::PipelineStageFlagBits::eTransfer,
                                                vk::PipelineStageFlagBits::eBottomOfPipe,
                                                vk::DependencyFlags(),
                                                {},
                                                {},
                                                { *imageMemoryBarierClearToPresent } );

        commandBuffers.at( i ).end();
    }
}
VulkanBase::VulkanBase( CreateInfo && info ) :
mInstance( info.instance ), mGpu( info.physDev ),
mExtansions( info.extansions ) /* swapchainImages( info.swapchain == vk::SwapchainKHR()
                 ? ImageVec()
                 : info.queueConf.logicDev.getSwapchainImagesKHR( info.swapchain ) ),
queueConf( info.queueConf ) */
{}

VulkanBase::~VulkanBase() = default;

VulkanGraphicRender::VulkanGraphicRender(
VulkanBase::CreateInfo &&          baseInfo,
VulkanGraphicRender::CreateInfo && graphicRenderCreateInfo ) :
VulkanBase( std::move( baseInfo ) ),
mXcbConnect( xcb_connect( nullptr, nullptr ) ),
//xcbConnect( graphicRenderCreateInfo.xcbConnect ),
mXcbWindow( graphicRenderCreateInfo.xcbWindow ), mComposite() {
    vk::XcbSurfaceCreateInfoKHR surfaceCI { .connection = mXcbConnect,
                                            .window     = mXcbWindow };
    mSurface = mInstance.createXcbSurfaceKHR( surfaceCI );

    mGpu = getDiscreteGpu( mInstance );
    std::cout << "Discrete GPU is : " << mGpu.getProperties().deviceName << std::endl
              << std::endl;

    printSurfaceExtents();

    mQueueConfigs.emplace_back(
    QueueTypeConfig { .queueFamilyIndex = getGraphicsQueueFamilyIndex( mGpu ),
                      .priorities       = QueuesPrioritiesVec { 1.0f } } );

    std::vector< vk::DeviceQueueCreateInfo > deviceQueueCreateInfos;
    deviceQueueCreateInfos.push_back( vk::DeviceQueueCreateInfo {
    .queueFamilyIndex = mQueueConfigs.at( 0 ).queueFamilyIndex,
    .queueCount       = 1,
    .pQueuePriorities = mQueueConfigs.at( 0 ).priorities.data() } );
    auto gpuFeatures = mGpu.getFeatures();

    vk::DeviceCreateInfo deviceCreateInfo {
        .queueCreateInfoCount =
        static_cast< std::uint32_t >( deviceQueueCreateInfos.size() ),
        .pQueueCreateInfos = deviceQueueCreateInfos.data(),
        .enabledExtensionCount =
        static_cast< std::uint32_t >( mExtansions.device.size() ),
        .ppEnabledExtensionNames = mExtansions.device.data(),
        .pEnabledFeatures        = &gpuFeatures
    };

    mLogicDev = mGpu.createDevice( deviceCreateInfo );

    mQueues.push_back( mLogicDev.getQueue( mQueueConfigs.at( 0 ).queueFamilyIndex, 0 ) );

    if ( !mGpu.getSurfaceSupportKHR( mQueueConfigs.at( 0 ).queueFamilyIndex, mSurface ) )
        throw std::runtime_error(
        "VulkanGraphicRender::VulkanGraphicRender(): Surface cann't support familyIndex." );

    mCommandPool = mLogicDev.createCommandPool( vk::CommandPoolCreateInfo {
    .flags = vk::CommandPoolCreateFlagBits::eTransient |
             vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    .queueFamilyIndex = mQueueConfigs.at( 0 ).queueFamilyIndex } );

    mSwapchain = swapchainInit( mGpu, mLogicDev, mSurface, mQueueConfigs.at( 0 ) );

    mSwapchainImages = mLogicDev.getSwapchainImagesKHR( mSwapchain );

    mCommandBuffers =
    commandBuffersInit( mLogicDev, mCommandPool, mSwapchainImages.size() );

    fillCmdBuffers(
    mQueueConfigs.at( 0 ).queueFamilyIndex, mCommandBuffers, mSwapchainImages );

    mSemaphores.push_back( mLogicDev.createSemaphore( {} ) );
    mSemaphores.push_back( mLogicDev.createSemaphore( {} ) );

    std::cout << std::endl
              << "Command buffer count :" << mCommandBuffers.size() << std::endl;
    std::cout << std::endl << "Image count : " << mSwapchainImages.size() << std::endl;
}

VulkanGraphicRender::~VulkanGraphicRender() = default;

void VulkanGraphicRender::draw() {
    mLogicDev.waitIdle();
    const auto asqNextImgIndex = mLogicDev.acquireNextImageKHR(
    mSwapchain, std::numeric_limits< std::uint64_t >::max(), mSemaphores.at( 0 ) );

    //        if (asqNextImgIndex.result == vk::Result::eErrorOutOfDateKHR)
    //            update();

    //        std::cout << std::endl
    //                  << "asqNextImgIndex number : " << asqNextImgIndex
    //                  << std::endl;

    const std::array< const vk::Flags< vk::PipelineStageFlagBits >, 1 >
    pipelineStageFlags { vk::PipelineStageFlagBits::eTransfer };

    const std::array< const vk::SubmitInfo, 1 > subInfo { vk::SubmitInfo {
    .waitSemaphoreCount   = 1,
    .pWaitSemaphores      = &mSemaphores.at( 0 ),
    .pWaitDstStageMask    = pipelineStageFlags.data(),
    .commandBufferCount   = 1,
    .pCommandBuffers      = &mCommandBuffers.at( asqNextImgIndex.value ),
    .signalSemaphoreCount = 1,
    .pSignalSemaphores    = &mSemaphores.at( 1 ) } };

    mQueues.at( 0 ).submit( subInfo );
    //       std::cout << "Submit is success" << std::endl;

    vk::PresentInfoKHR present { .waitSemaphoreCount = 1,
                                 .pWaitSemaphores    = &mSemaphores.at( 1 ),
                                 .swapchainCount     = 1,
                                 .pSwapchains        = &mSwapchain,
                                 .pImageIndices      = &asqNextImgIndex.value };

    try {
        [[maybe_unused]] auto result = mQueues.at( 0 ).presentKHR( present );
        //    switch ( queues.at( 0 ).presentKHR( present ) ) {
        //    case vk::Result::eErrorOutOfDateKHR:
        //        //    update();
        //        //std::cout << "Present is success" << std::endl;
        //        break;
        //    default: break;
        //    }
    } catch ( const vk::OutOfDateKHRError & e ) {
        update();
        std::cout << e.what() << std::endl;
    }
}

void VulkanGraphicRender::update() {
    mLogicDev.waitIdle();

    //queueConf.logicDev.freeCommandBuffers( graphicConf.commandPool,
    //                                         queueConf.commandBuffers );
    //queueConf.logicDev.destroySwapchainKHR( swapchain );

    mSwapchain =
    swapchainInit( mGpu, mLogicDev, mSurface, mQueueConfigs.at( 0 ), mSwapchain );

    mSwapchainImages = mLogicDev.getSwapchainImagesKHR( mSwapchain );

    //queueConf.commandBuffers =
    //commandBuffersInit( queueConf, swapchainImages.size() );

    for ( auto cmdBuffer : mCommandBuffers )
        cmdBuffer.reset();

    fillCmdBuffers(
    mQueueConfigs.at( 0 ).queueFamilyIndex, mCommandBuffers, mSwapchainImages );
}

void VulkanGraphicRender::printSurfaceExtents() const {
    std::cout << mGpu.getSurfaceCapabilitiesKHR( mSurface ).currentExtent.width << "X"
              << mGpu.getSurfaceCapabilitiesKHR( mSurface ).currentExtent.height
              << std::endl
              << std::endl;
}

// namespace
}   // namespace core::renderer
