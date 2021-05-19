#include "vulkanrender.hpp"
#include "composite.hpp"
#include "xcbwraper/xcbconnection.hpp"
#include "xcbwraper/xcbinternatom.hpp"

#include <algorithm>
#include <array>
#include <bits/c++config.h>
#include <bits/stdint-uintn.h>
#include <cassert>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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
                    std::uint32_t           swapchainImagesCount ) {
    auto commandBufferAI = std::make_unique< vk::CommandBufferAllocateInfo >(
    vk::CommandBufferAllocateInfo { .commandPool = commandPool,
                                    .level       = vk::CommandBufferLevel::ePrimary,
                                    .commandBufferCount = swapchainImagesCount } );

    auto commandBuffers = logicDev.allocateCommandBuffers( *commandBufferAI );

    return commandBuffers;
}

[[nodiscard]] vk::SurfaceFormatKHR matchNeedFormatOrFirst(
vk::PhysicalDevice gpu, vk::SurfaceKHR surface, vk::SurfaceFormatKHR format ) {
    auto                 gpuSurfaceFormats = gpu.getSurfaceFormatsKHR( surface );
    vk::SurfaceFormatKHR gpuSurfaceFormat;
    for ( auto && gpuSurfaceFormat : gpuSurfaceFormats )
        std::cout << vk::to_string( gpuSurfaceFormat.format ) << std::endl
                  << vk::to_string( gpuSurfaceFormat.colorSpace ) << std::endl;

    std::cout << std::endl;

    if ( gpuSurfaceFormats.size() == 1 &&
         gpuSurfaceFormats.at( 0 ).format == vk::Format::eUndefined )
        gpuSurfaceFormat = vk::SurfaceFormatKHR { .format     = format.format,
                                                  .colorSpace = format.colorSpace };
    else
        for ( auto && gpuSF : gpuSurfaceFormats ) {
            if ( gpuSF.format == format.format &&
                 gpuSF.colorSpace == format.colorSpace )
                gpuSurfaceFormat = gpuSF;
        }

    if ( gpuSurfaceFormat.format != vk::Format::eB8G8R8A8Unorm )
        gpuSurfaceFormat = gpuSurfaceFormats.at( 0 );

    return gpuSurfaceFormat;
}

[[nodiscard]] vk::SwapchainKHR
swapchainInit( const vk::PhysicalDevice &          gpu,
               const vk::Device &                  logicDev,
               const vk::SurfaceKHR &              surface,
               const VulkanBase::QueueTypeConfig & queueConf,
               std::size_t                         countSwapchainsOptimal,
               vk::SurfaceFormatKHR                needFormat,
               vk::SwapchainKHR                    oldSwapchain ) {
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
        gpu.getSurfaceCapabilitiesKHR( surface ).maxImageCount,
        countSwapchainsOptimal ),
        .imageFormat      = needFormat.format,
        .imageColorSpace  = needFormat.colorSpace,
        .imageExtent      = gpu.getSurfaceCapabilitiesKHR( surface ).currentExtent,
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment |
                      vk::ImageUsageFlagBits::eTransferDst |
                      vk::ImageUsageFlagBits::eTransferSrc,
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

[[nodiscard]] vk::PhysicalDevice getDiscreteGpu( const vk::Instance & instance ) {
    const std::vector< vk::PhysicalDevice > gpus =
    instance.enumeratePhysicalDevices();

    for ( auto && gpu : gpus ) {
        if ( gpu.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu )
            return gpu;
    }

    throw std::runtime_error( "Not matched discrete GPU" );

    return vk::PhysicalDevice();
}

[[nodiscard]] QueueFamilyIndex
getGraphicsQueueFamilyIndex( const vk::PhysicalDevice & gpu ) {
    const auto queueFamilyProps = gpu.getQueueFamilyProperties();

    constexpr auto flags =
    vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer;
    for ( QueueFamilyIndex queueFamilyIndex = 0;
          queueFamilyIndex < queueFamilyProps.size();
          ++queueFamilyIndex )
        if ( queueFamilyProps[ queueFamilyIndex ].queueFlags & flags )
            return queueFamilyIndex;

    throw std::runtime_error( "Not matched graphycs queue family" );

    return QueueFamilyIndex();
}

void fillCmdBuffers [[maybe_unused]] ( QueueFamilyIndex    queueFamilyIndex,
                                       CommandBuffersVec & commandBuffers,
                                       ImageVec &          swapchainImages ) {
    vk::CommandBufferBeginInfo cmdBufferBI {
        .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse
    };

    const vk::ClearColorValue clearColorValue(
    std::array< float, 4 > { 0.8f, 0.5f, 0.0f, 0.5f } );

    const std::array< const vk::ImageSubresourceRange, 1 > ranges {
        vk::ImageSubresourceRange { .aspectMask   = vk::ImageAspectFlagBits::eColor,
                                    .baseMipLevel = 0,
                                    .levelCount   = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1 }
    };

    auto imageMemoryBarierPresentToClear =
    std::make_unique< vk::ImageMemoryBarrier >(
    vk::ImageMemoryBarrier { .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
                             .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                             .oldLayout     = vk::ImageLayout::eUndefined,
                             .newLayout     = vk::ImageLayout::eTransferDstOptimal,
                             .srcQueueFamilyIndex = queueFamilyIndex,
                             .dstQueueFamilyIndex = queueFamilyIndex,
                             .subresourceRange    = ranges.at( 0 ) } );

    auto imageMemoryBarierClearToPresent =
    std::make_unique< vk::ImageMemoryBarrier >(
    vk::ImageMemoryBarrier { .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                             .dstAccessMask = vk::AccessFlagBits::eMemoryWrite,
                             .oldLayout     = vk::ImageLayout::eTransferDstOptimal,
                             .newLayout     = vk::ImageLayout::ePresentSrcKHR,
                             .srcQueueFamilyIndex = queueFamilyIndex,
                             .dstQueueFamilyIndex = queueFamilyIndex,
                             .subresourceRange    = ranges.at( 0 ) } );

    for ( std::uint32_t i = 0; i < swapchainImages.size(); ++i ) {
        imageMemoryBarierClearToPresent->image = swapchainImages.at( i );
        imageMemoryBarierPresentToClear->image = swapchainImages.at( i );

        commandBuffers.at( i ).begin( cmdBufferBI );

        commandBuffers.at( i ).pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlags(),
        {},
        {},
        { *imageMemoryBarierPresentToClear } );

        commandBuffers.at( i ).clearColorImage( swapchainImages.at( i ),
                                                vk::ImageLayout::eTransferDstOptimal,
                                                clearColorValue,
                                                ranges );

        commandBuffers.at( i ).pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::DependencyFlags(),
        {},
        {},
        { *imageMemoryBarierClearToPresent } );

        commandBuffers.at( i ).end();
    }
}

void fillCmdBuffersForPresentComposite
[[maybe_unused]] ( QueueFamilyIndex                           queueFamilyIndex,
                   CommandBuffersVec &                        commandBuffers,
                   ImageVec &                                 swapchainImages,
                   const vk::Image &                               srcImage,
                   const std::vector< vk::ImageBlit > & regions /*,
                   std::size_t                                imageDataSize*/ ) {
    vk::CommandBufferBeginInfo cmdBufferBI {
        .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse
    };

    const std::array< const vk::ImageSubresourceRange, 1 > ranges {
        vk::ImageSubresourceRange { .aspectMask   = vk::ImageAspectFlagBits::eColor,
                                    .baseMipLevel = 0,
                                    .levelCount   = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1 }
    };

    auto imageMemoryBarierPresentToClear =
    std::make_unique< vk::ImageMemoryBarrier >(
    vk::ImageMemoryBarrier { .srcAccessMask = vk::AccessFlagBits::eTransferRead,
                             .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                             .oldLayout     = vk::ImageLayout::eUndefined,
                             .newLayout     = vk::ImageLayout::eTransferDstOptimal,
                             .srcQueueFamilyIndex = queueFamilyIndex,
                             .dstQueueFamilyIndex = queueFamilyIndex,
                             .subresourceRange    = ranges.at( 0 ) } );

    auto imageMemoryBarierClearToPresent =
    std::make_unique< vk::ImageMemoryBarrier >(
    vk::ImageMemoryBarrier { .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                             .dstAccessMask = vk::AccessFlagBits::eMemoryWrite,
                             .oldLayout     = vk::ImageLayout::eTransferDstOptimal,
                             .newLayout     = vk::ImageLayout::ePresentSrcKHR,
                             .srcQueueFamilyIndex = queueFamilyIndex,
                             .dstQueueFamilyIndex = queueFamilyIndex,
                             .subresourceRange    = ranges.at( 0 ) } );

    const vk::ClearColorValue clearColorValue(
    std::array< float, 4 > { 0.2f, 0.2f, 0.2f, 0.5f } );

    for ( std::uint32_t i = 0; i < swapchainImages.size(); ++i ) {
        imageMemoryBarierClearToPresent->image = swapchainImages.at( i );
        imageMemoryBarierPresentToClear->image = swapchainImages.at( i );

        commandBuffers.at( i ).begin( cmdBufferBI );

        commandBuffers.at( i ).pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlags(),
        {},
        {},
        { *imageMemoryBarierPresentToClear } );

        commandBuffers.at( i ).clearColorImage( swapchainImages.at( i ),
                                                vk::ImageLayout::eTransferDstOptimal,
                                                clearColorValue,
                                                ranges );

        commandBuffers.at( i ).pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlags(),
        {},
        {},
        { *imageMemoryBarierPresentToClear } );

        commandBuffers.at( i ).blitImage( srcImage,
                                          vk::ImageLayout::eTransferSrcOptimal,
                                          swapchainImages.at( i ),
                                          vk::ImageLayout::eSharedPresentKHR,
                                          regions,
                                          vk::Filter::eLinear );

        commandBuffers.at( i ).pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::DependencyFlags(),
        {},
        {},
        { *imageMemoryBarierClearToPresent } );

        commandBuffers.at( i ).end();
    }
}

std::vector< vk::ImageBlit >
initImageBlitRegions( xcbwraper::WindowShared srcWindow,
                      xcbwraper::WindowShared dstWindow ) {
    auto srcWindowGeometry = srcWindow->getProperties().getGeometry();
    vk::ArrayWrapper1D< vk::Offset3D, 2 > srcBound {
        { vk::Offset3D { .x = 0, .y = 0, .z = 0 },
          vk::Offset3D { .x = srcWindowGeometry.rightBotPoint.x,
                         .y = srcWindowGeometry.rightBotPoint.y,
                         .z = 0 } }
    };

    auto dstWindowGeometry = dstWindow->getProperties().getGeometry();
    vk::ArrayWrapper1D< vk::Offset3D, 2 > dstBound {
        { vk::Offset3D { .x = 0, .y = 0, .z = 0 },
          vk::Offset3D { .x = dstWindowGeometry.rightBotPoint.x,
                         .y = dstWindowGeometry.rightBotPoint.y,
                         .z = 0 } }
    };

    vk::ImageSubresourceLayers srcSubresource { .aspectMask =
                                                vk::ImageAspectFlagBits::eColor,
                                                .mipLevel       = 0,
                                                .baseArrayLayer = 0,
                                                .layerCount     = 1 };

    vk::ImageSubresourceLayers dstSubresource { .aspectMask =
                                                vk::ImageAspectFlagBits::eColor,
                                                .mipLevel       = 0,
                                                .baseArrayLayer = 0,
                                                .layerCount     = 1 };

    std::vector< vk::ImageBlit > srcToDstRegions;
    srcToDstRegions.push_back( vk::ImageBlit { .srcSubresource = srcSubresource,
                                               .srcOffsets     = srcBound,
                                               .dstSubresource = dstSubresource,
                                               .dstOffsets     = dstBound } );
    return srcToDstRegions;
}
}   // namespace

VulkanBase::VulkanBase( CreateInfo && info ) :
mInstance( info.instance ), mGpu( info.physDev ),
mExtansions(
info.extansions ) /* swapchainImages( info.swapchain == vk::SwapchainKHR()
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
mDstWindow( std::make_shared< xcbwraper::Window >(
xcbwraper::Window::CreateInfo { .window = graphicRenderCreateInfo.xcbWindow } ) ) /*,
mComposite( graphicRenderCreateInfo.xcbConnect )*/
{
    {
        vk::XcbSurfaceCreateInfoKHR surfaceCI { .connection =
                                                *graphicRenderCreateInfo.xcbConnect,
                                                .window = *mDstWindow };
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
    mQueues.push_back(
    mLogicDev.getQueue( mQueueConfigs.at( 0 ).queueFamilyIndex, 0 ) );

    if ( !mGpu.getSurfaceSupportKHR( mQueueConfigs.at( 0 ).queueFamilyIndex,
                                     mSurface ) )
        throw std::runtime_error(
        "VulkanGraphicRender::VulkanGraphicRender(): Surface cann't support familyIndex." );

    mCommandPool = mLogicDev.createCommandPool( vk::CommandPoolCreateInfo {
    .flags = vk::CommandPoolCreateFlagBits::eTransient |
             vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    .queueFamilyIndex = mQueueConfigs.at( 0 ).queueFamilyIndex } );

    {
        vk::SurfaceFormatKHR needFormat { .format = vk::Format::eB8G8R8A8Unorm,
                                          .colorSpace =
                                          vk::ColorSpaceKHR::eSrgbNonlinear };
        mSurfaceFormat = matchNeedFormatOrFirst( mGpu, mSurface, needFormat );
    }

    mSwapchain = swapchainInit( mGpu,
                                mLogicDev,
                                mSurface,
                                mQueueConfigs.at( 0 ),
                                countSwapChainBuffers,
                                mSurfaceFormat,
                                {} );

    mSwapchainImages = mLogicDev.getSwapchainImagesKHR( mSwapchain );

    mSwapchainCmdBuffers =
    commandBuffersInit( mLogicDev, mCommandPool, mSwapchainImages.size() );

    //    fillCmdBuffers(
    //    mQueueConfigs.at( 0 ).queueFamilyIndex, mSwapchainCmdBuffers, mSwapchainImages );

    mSemaphores.push_back( mLogicDev.createSemaphore( {} ) );
    mSemaphores.push_back( mLogicDev.createSemaphore( {} ) );

    {
        xcbwraper::AtomNetClientList clientList {};
        auto                         clientsVec = clientList.get();

        for ( auto && client : clientsVec )
            std::cout << "TEST POINT " << client.getID() << std::endl
                      << client.getClass() << std::endl
                      << std::endl;
        xcbwraper::XcbConnectionShared connection =
        std::make_shared< xcbwraper::XCBConnection >();
        xcbwraper::Window::CreateInfo windowCI { .connection = connection };

        constexpr std::string_view searchWindowClass( "Watcher" );

        for ( auto && client : clientsVec )
            if ( client.getClass() == searchWindowClass )
                windowCI.window = client.getID();

        mSrcWindow = std::make_shared< xcbwraper::Window >( windowCI );
        if ( !mSrcWindow->getProperties().isViewable() )
            throw std::runtime_error( "Selected overlay window id not viewvable" );
        std::cout << "Selected window: " << mSrcWindow->getProperties().getClass()
                  << std::endl
                  << "with id: " << mSrcWindow->getProperties().getID() << std::endl;
    }

    {
        mSrcToDstRegions = initImageBlitRegions( mSrcWindow, mDstWindow );

        auto bitPerRgb = mSrcWindow->getProperties().getBitPerRGB() * 3;
        std::cout << "Overlay Window has "
                  << static_cast< std::uint32_t >( bitPerRgb ) << " bit per rgb."
                  << std::endl;

        auto srcWindowGeometry = mSrcWindow->getProperties().getGeometry();
        auto srcImageCI        = vk::ImageCreateInfo {
            .flags       = {},
            .imageType   = vk::ImageType::e2D,
            .format      = mSurfaceFormat.format,
            .extent      = vk::Extent3D { .width  = srcWindowGeometry.width,
                                     .height = srcWindowGeometry.height,
                                     .depth  = 1 },
            .mipLevels   = 1,
            .arrayLayers = 1,
            .samples     = vk::SampleCountFlagBits::e1,
            .tiling      = vk::ImageTiling::eOptimal,
            .usage       = vk::ImageUsageFlagBits::eTransferDst |
                     vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eSampled,
            .sharingMode           = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices   = &mQueueConfigs.at( 0 ).queueFamilyIndex,
            .initialLayout         = vk::ImageLayout::eUndefined
        };
        mSrcImage = mLogicDev.createImage( srcImageCI );
    }

    {
        constexpr vk::MemoryPropertyFlags memReqFlags {
            vk::MemoryPropertyFlagBits::eHostVisible
        };

        const auto memRequarements =
        mLogicDev.getImageMemoryRequirements( mSrcImage );
        const auto memTypeBitsRequarements = memRequarements.memoryTypeBits;

        const auto gpuMemPropArr = mGpu.getMemoryProperties().memoryTypes;
        const auto countMemProps = mGpu.getMemoryProperties().memoryTypeCount;

        std::cout << "Mem types arr size is : " << gpuMemPropArr.size() << std::endl
                  << "Mem types count is : "
                  << mGpu.getMemoryProperties().memoryTypeCount << std::endl;

        std::cout << "Mem requarements flags is : " << vk::to_string( memReqFlags )
                  << std::endl
                  << "Mem type bits req is : " << memTypeBitsRequarements
                  << std::endl
                  << std::endl;

        std::uint32_t                                 memTypeIndex = 0;
        constexpr decltype( memTypeBitsRequarements ) memTypeBit   = 1;
        for ( ; memTypeIndex < countMemProps &&
                !( static_cast< bool >(
                   gpuMemPropArr.at( memTypeIndex ).propertyFlags & memReqFlags ) &&
                   static_cast< bool >( memTypeBitsRequarements &
                                        ( memTypeBit << memTypeIndex ) ) );
              ++memTypeIndex )
            std::cout << "Property flags is : "
                      << vk::to_string(
                         gpuMemPropArr.at( memTypeIndex ).propertyFlags )
                      << std::endl
                      << "Type bit is : " << ( memTypeBit << memTypeIndex )
                      << std::endl
                      << std::endl;

        if ( memTypeIndex == countMemProps )
            throw std::runtime_error(
            "In Render constructor Requared memory index not found!" );

        vk::MemoryAllocateInfo memAllocInfo { .allocationSize = memRequarements.size,
                                              .memoryTypeIndex = memTypeIndex };
        mSrcRawImage = mLogicDev.allocateMemory( memAllocInfo );

        mLogicDev.bindImageMemory( mSrcImage, mSrcRawImage, 0 );
    }

    //    fillCmdBuffersForPresentComposite( mQueueConfigs.at( 0 ).queueFamilyIndex,
    //                                       mSwapchainCmdBuffers,
    //                                       mSwapchainImages,
    //                                       mSrcImageBuffer,
    //                                       mSrcToDstRegions );
    std::cout << std::endl
              << "Command buffer count :" << mSwapchainCmdBuffers.size()
              << std::endl;
    std::cout << std::endl
              << "Image count : " << mSwapchainImages.size() << std::endl;
}

VulkanGraphicRender::~VulkanGraphicRender() {
    //mLogicDev.waitIdle();
    //mLogicDev.freeMemory( mSrcRawImage );
}

void VulkanGraphicRender::draw() {
    try {
        auto memRequarements = mLogicDev.getImageMemoryRequirements( mSrcImage );

        auto imageDataVec = mSrcWindow->getImageData();
        //std::cout << "Image size: " << imageDataVec.size() << std::endl;
        //std::cout << "Mem req size: " << memRequarements.size << std::endl;
        auto srcBufferData =
        mLogicDev.mapMemory( mSrcRawImage, 0, memRequarements.size );
        std::copy( imageDataVec.begin(),
                   imageDataVec.end(),
                   static_cast< std::uint8_t * >( srcBufferData ) );
        mLogicDev.unmapMemory( mSrcRawImage );

        for ( auto cmdBuffer : mSwapchainCmdBuffers )
            cmdBuffer.reset();

        mLogicDev.waitIdle();

        mSrcToDstRegions = initImageBlitRegions( mSrcWindow, mDstWindow );

        fillCmdBuffersForPresentComposite( mQueueConfigs.at( 0 ).queueFamilyIndex,
                                           mSwapchainCmdBuffers,
                                           mSwapchainImages,
                                           mSrcImage,
                                           mSrcToDstRegions );

    } catch ( const std::runtime_error & err ) { std::cout << err.what(); }

    const auto asqNextImgIndex = mLogicDev.acquireNextImageKHR(
    mSwapchain, std::numeric_limits< std::uint64_t >::max(), mSemaphores.at( 0 ) );

    const std::array< const vk::Flags< vk::PipelineStageFlagBits >, 1 >
    pipelineStageFlags { vk::PipelineStageFlagBits::eTransfer };

    const std::array< const vk::SubmitInfo, 1 > subInfo { vk::SubmitInfo {
    .waitSemaphoreCount   = 1,
    .pWaitSemaphores      = &mSemaphores.at( 0 ),
    .pWaitDstStageMask    = pipelineStageFlags.data(),
    .commandBufferCount   = 1,
    .pCommandBuffers      = &mSwapchainCmdBuffers.at( asqNextImgIndex.value ),
    .signalSemaphoreCount = 1,
    .pSignalSemaphores    = &mSemaphores.at( 1 ) } };

    mLogicDev.waitIdle();
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

    mSwapchain = swapchainInit( mGpu,
                                mLogicDev,
                                mSurface,
                                mQueueConfigs.at( 0 ),
                                countSwapChainBuffers,
                                mSurfaceFormat,
                                mSwapchain );

    mSwapchainImages = mLogicDev.getSwapchainImagesKHR( mSwapchain );

    for ( auto cmdBuffer : mSwapchainCmdBuffers )
        cmdBuffer.reset();

    mSrcToDstRegions = initImageBlitRegions( mSrcWindow, mDstWindow );
    fillCmdBuffersForPresentComposite( mQueueConfigs.at( 0 ).queueFamilyIndex,
                                       mSwapchainCmdBuffers,
                                       mSwapchainImages,
                                       mSrcImage,
                                       mSrcToDstRegions );

    //    fillCmdBuffers(
    //    mQueueConfigs.at( 0 ).queueFamilyIndex, mSwapchainCmdBuffers, mSwapchainImages );
}

void VulkanGraphicRender::printSurfaceExtents() const {
    std::cout << mGpu.getSurfaceCapabilitiesKHR( mSurface ).currentExtent.width
              << "X"
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
            if ( reinterpret_cast< xcb_key_press_event_t * >( event )->detail ==
                 24 ) {
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
    auto screen =
    xcb_setup_roots_iterator(
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

    VulkanBase::Extensions extensions {
        .instance = { VK_KHR_SURFACE_EXTENSION_NAME,
                      VK_KHR_XCB_SURFACE_EXTENSION_NAME },
        .device   = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }
    };
    vk::Instance vulkanXCBInstance = vk::createInstance( vk::InstanceCreateInfo {
    .pApplicationInfo = appInfo.get(),
    .enabledExtensionCount =
    static_cast< std::uint32_t >( extensions.instance.size() ),
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
