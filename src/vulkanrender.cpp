#include "vulkanrender.hpp"
#include "composite.hpp"
#include "xcbwraper/xcbconnection.hpp"
#include "xcbwraper/xcbinternatom.hpp"

#include <algorithm>
#include <array>
#include <cassert>
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

namespace {
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

void fillCmdBuffersForPresentComposite
[[maybe_unused]] ( QueueFamilyIndex    queueFamilyIndex,
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
}   // namespace

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
//mXcbConnect(  ),
//xcbConnect( graphicRenderCreateInfo.xcbConnect ),
mXcbWindow( graphicRenderCreateInfo.xcbWindow ) /*,
mComposite( graphicRenderCreateInfo.xcbConnect )*/
{
    {
        vk::XcbSurfaceCreateInfoKHR surfaceCI { .connection =
                                                *graphicRenderCreateInfo.xcbConnect,
                                                .window = mXcbWindow };
        mSurface = mInstance.createXcbSurfaceKHR( surfaceCI );
    }

    mGpu = getDiscreteGpu( mInstance );
    std::cout << "Discrete GPU is : " << mGpu.getProperties().deviceName << std::endl
              << std::endl;

    printSurfaceExtents();

    mQueueConfigs.emplace_back(
    QueueTypeConfig { .queueFamilyIndex = getGraphicsQueueFamilyIndex( mGpu ),
                      .priorities       = QueuesPrioritiesVec { 1.0f } } );
    {
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
    }
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
    //std::cout << "TEST POINT" << std::endl;
    {
        xcbwraper::AtomNetClientList clientList {};
        auto                         clientsVec = clientList.get();

        for ( auto && client : clientsVec )
            std::cout << "TEST POINT " << client.getID() << std::endl
                      << client.getClass() << std::endl
                      << std::endl;
        xcbwraper::XcbConnectionShared connection =
        std::make_shared< xcbwraper::XCBConnection >();

        xcbwraper::Window window { xcbwraper::Window::CreateInfo {
        .connection = connection, .window = clientsVec.at( 1 ).getID() } };
        mRawOverlayImage = window.getImageData();
    }
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
    } catch ( const vk::OutOfDateKHRError & e ) {
        update();
        std::cout << e.what() << std::endl;
    }
}

void VulkanGraphicRender::update() {
    mLogicDev.waitIdle();

    mSwapchain =
    swapchainInit( mGpu, mLogicDev, mSurface, mQueueConfigs.at( 0 ), mSwapchain );

    mSwapchainImages = mLogicDev.getSwapchainImagesKHR( mSwapchain );

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

namespace {

template < class Renderer > concept HasDrawMethod = requires( Renderer renderer ) {
    { renderer.draw() };
};

template < HasDrawMethod Renderer >
void runRenderLoop( Renderer renderer, xcbwraper::XcbConnectionShared xcbConnect ) {
    for ( bool breakLoop = false; !breakLoop; ) {
        renderer.draw();
        auto event =
        xcb_poll_for_event( static_cast< xcb_connection_t * >( *xcbConnect ) );
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
}
}   // namespace

VulkanRenderInstance::Shared VulkanRenderInstance::init() {
    if ( !mInstance )
        mInstance =
        std::shared_ptr< VulkanRenderInstance > { new VulkanRenderInstance() };
    assert( mInstance && "VulkanRenderInstance is not created" );
    return mInstance;
}

VulkanRenderInstance::Shared VulkanRenderInstance::mInstance {};

VulkanRenderInstance::VulkanRenderInstance() :
mXcbConnect( std::make_shared< xcbwraper::XCBConnection >() ) {
    assert( mXcbConnect && "XCB connection is not created" );
}

VulkanRenderInstance::~VulkanRenderInstance() = default;

void VulkanRenderInstance::run() const {
    auto screen = xcb_setup_roots_iterator(
                  xcb_get_setup( static_cast< xcb_connection_t * >( *mXcbConnect ) ) )
                  .data;
    assert( screen != nullptr && "xcb_setup_roots_iterator return nullptr" );

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

    xcb_window_t window =
    xcb_generate_id( static_cast< xcb_connection_t * >( *mXcbConnect ) );
    xcb_create_window( static_cast< xcb_connection_t * >( *mXcbConnect ),
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

    xcb_map_window( static_cast< xcb_connection_t * >( *mXcbConnect ), window );

    xcb_flush( static_cast< xcb_connection_t * >( *mXcbConnect ) );

    auto appInfo = std::make_unique< vk::ApplicationInfo >(
    vk::ApplicationInfo { .pApplicationName   = "vulkan_xcb",
                          .applicationVersion = VK_MAKE_VERSION( 0, 0, 1 ),
                          .pEngineName        = "vulkan_xcb_engine",
                          .engineVersion      = VK_MAKE_VERSION( 0, 0, 1 ),
                          .apiVersion         = VK_API_VERSION_1_0 } );

    VulkanBase::Extensions extensions { .instance = { VK_KHR_SURFACE_EXTENSION_NAME,
                                                      VK_KHR_XCB_SURFACE_EXTENSION_NAME },
                                        .device   = { VK_KHR_SWAPCHAIN_EXTENSION_NAME } };
    vk::Instance vulkanXCBInstance = vk::createInstance( vk::InstanceCreateInfo {
    .pApplicationInfo        = appInfo.get(),
    .enabledExtensionCount   = static_cast< std::uint32_t >( extensions.instance.size() ),
    .ppEnabledExtensionNames = extensions.instance.data() } );

    auto                            gpu = getDiscreteGpu( vulkanXCBInstance );
    VulkanBase::CreateInfo          vulkanBaseCI { .instance   = vulkanXCBInstance,
                                          .physDev    = gpu,
                                          .extansions = extensions };
    VulkanGraphicRender::CreateInfo vulkanRenderCI { .xcbConnect = mXcbConnect,
                                                     .xcbWindow  = window };

    VulkanGraphicRender renderer( std::move( vulkanBaseCI ),
                                  std::move( vulkanRenderCI ) );
    runRenderLoop< VulkanGraphicRender >( renderer, mXcbConnect );

    xcb_destroy_window( static_cast< xcb_connection_t * >( *mXcbConnect ), window );
    xcb_flush( static_cast< xcb_connection_t * >( *mXcbConnect ) );
}
}   // namespace core::renderer
