#include "vulkanrender.hpp"
#include "composite.hpp"
#include "xcbwraper/posix-shm.hpp"
#include "xcbwraper/windowgeometry.hpp"
#include "xcbwraper/xcbconnection.hpp"
#include "xcbwraper/xcbinternatom.hpp"

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <bits/c++config.h>
#include <chrono>
#include <mutex>
#include <thread>
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
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace core::renderer {

namespace {

[[nodiscard]] vk::DeviceMemory createMemAndBind
[[maybe_unused]] ( const vk::PhysicalDevice &    gpu,
                   const vk::Device &            logicDev,
                   const vk::Image &             image,
                   const vk::MemoryPropertyFlags memReqFlags ) {
    const auto memRequarements = logicDev.getImageMemoryRequirements( image );
    const auto memTypeBitsRequarements = memRequarements.memoryTypeBits;

    const auto gpuMemPropArr = gpu.getMemoryProperties().memoryTypes;
    const auto countMemProps = gpu.getMemoryProperties().memoryTypeCount;

    std::cout << "Image Mem types arr size is : " << gpuMemPropArr.size()
              << std::endl
              << "Image Mem types count is : "
              << gpu.getMemoryProperties().memoryTypeCount << std::endl;

    std::cout << "Image Mem requarements flags is : " << vk::to_string( memReqFlags )
              << std::endl
              << "Image Mem type bits req is : " << memTypeBitsRequarements
              << std::endl
              << std::endl;

    std::uint32_t                                 memTypeIndex = 0;
    constexpr decltype( memTypeBitsRequarements ) memTypeBit   = 1;
    for ( ; memTypeIndex < countMemProps &&
            !( static_cast< bool >(
               memReqFlags ==
               ( memReqFlags & gpuMemPropArr.at( memTypeIndex ).propertyFlags ) ) &&
               static_cast< bool >( memTypeBitsRequarements &
                                    ( memTypeBit << memTypeIndex ) ) );
          ++memTypeIndex )
        ;

    std::cout << "Property flags is : "
              << vk::to_string( gpuMemPropArr.at( memTypeIndex ).propertyFlags )
              << std::endl
              << "Type bit is : " << ( memTypeBit << memTypeIndex ) << std::endl
              << std::endl;

    if ( memTypeIndex == countMemProps )
        throw std::runtime_error(
        "In Render constructor Requared memory index not found!" );

    vk::MemoryAllocateInfo memAllocInfo { .allocationSize  = memRequarements.size,
                                          .memoryTypeIndex = memTypeIndex };
    vk::DeviceMemory       imageRam = logicDev.allocateMemory( memAllocInfo );

    logicDev.bindImageMemory( image, imageRam, 0 );

    return imageRam;
}

[[nodiscard]] vk::DeviceMemory createMemAndBind
[[maybe_unused]] ( const vk::PhysicalDevice &    gpu,
                   const vk::Device &            logicDev,
                   const vk::Buffer &            buffer,
                   const vk::MemoryPropertyFlags memReqFlags ) {
    const auto memRequarements = logicDev.getBufferMemoryRequirements( buffer );
    const auto memTypeBitsRequarements = memRequarements.memoryTypeBits;

    const auto gpuMemPropArr = gpu.getMemoryProperties().memoryTypes;
    const auto countMemProps = gpu.getMemoryProperties().memoryTypeCount;

    std::cout << "Mem types arr size is : " << gpuMemPropArr.size() << std::endl
              << "Mem types count is : " << gpu.getMemoryProperties().memoryTypeCount
              << std::endl;

    std::cout << "Mem requarements flags is : " << vk::to_string( memReqFlags )
              << std::endl
              << "Mem type bits req is : " << memTypeBitsRequarements << std::endl
              << std::endl;

    std::uint32_t                                 memTypeIndex = 0;
    constexpr decltype( memTypeBitsRequarements ) memTypeBit   = 1;
    for ( ; memTypeIndex < countMemProps &&
            !( static_cast< bool >(
               memReqFlags ==
               ( memReqFlags & gpuMemPropArr.at( memTypeIndex ).propertyFlags ) ) &&
               static_cast< bool >( memTypeBitsRequarements &
                                    ( memTypeBit << memTypeIndex ) ) );
          ++memTypeIndex )
        ;

    std::cout << "Property flags is : "
              << vk::to_string( gpuMemPropArr.at( memTypeIndex ).propertyFlags )
              << std::endl
              << "Type bit is : " << ( memTypeBit << memTypeIndex ) << std::endl
              << std::endl;

    if ( memTypeIndex == countMemProps )
        throw std::runtime_error(
        "In Render constructor Requared memory index not found!" );

    vk::MemoryAllocateInfo memAllocInfo { .allocationSize  = memRequarements.size,
                                          .memoryTypeIndex = memTypeIndex };
    vk::DeviceMemory       bufferRam = logicDev.allocateMemory( memAllocInfo );

    logicDev.bindBufferMemory( buffer, bufferRam, 0 );

    return bufferRam;
}

[[nodiscard]] std::vector< vk::CommandBuffer >
commandBuffersInit( const vk::Device &      logicDev,
                    const vk::CommandPool & commandPool,
                    std::uint32_t           swapchainImagesCount ) {
    vk::CommandBufferAllocateInfo commandBufferAI { .commandPool = commandPool,
                                                    .level =
                                                    vk::CommandBufferLevel::ePrimary,
                                                    .commandBufferCount =
                                                    swapchainImagesCount };

    return logicDev.allocateCommandBuffers( commandBufferAI );
}

[[nodiscard]] vk::SurfaceFormatKHR matchNeedFormatOrFirst(
vk::PhysicalDevice gpu, vk::SurfaceKHR surface, vk::SurfaceFormatKHR format ) {
    auto gpuSurfaceFormats = gpu.getSurfaceFormatsKHR( surface );
    for ( auto && gpuSurfaceFormat : gpuSurfaceFormats )
        std::cout << vk::to_string( gpuSurfaceFormat.format ) << std::endl
                  << vk::to_string( gpuSurfaceFormat.colorSpace ) << std::endl;

    std::cout << std::endl;

    vk::SurfaceFormatKHR gpuSurfaceFormat;
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

    if ( gpuSurfaceFormat.format != format.format )
        gpuSurfaceFormat = gpuSurfaceFormats.at( 0 );

    return gpuSurfaceFormat;
}

struct CreatedSwapchainInfo final {
    vk::SwapchainKHR swapchain { VK_NULL_HANDLE };
    vk::Extent2D     swapchainExtent {};
};

[[nodiscard]] CreatedSwapchainInfo
swapchainInit( const vk::PhysicalDevice &          gpu,
               const vk::Device &                  logicDev,
               const vk::SurfaceKHR &              surface,
               const VulkanBase::QueueTypeConfig & queueConf,
               std::size_t                         countSwapchainsOptimal,
               vk::SurfaceFormatKHR                needFormat,
               vk::SwapchainKHR                    oldSwapchain ) {
    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
    for ( auto && pm : gpu.getSurfacePresentModesKHR( surface ) ) {
        //        std::cout << vk::to_string( pm ) << std::endl;
        if ( pm == vk::PresentModeKHR::eMailbox ) {
            presentMode = pm;
            break;
        }
    }

    //    std::cout << "Present mode is " << vk::to_string( presentMode ) << std::endl;

    //std::cout << "Min image count is "
    //          << gpu.getSurfaceCapabilitiesKHR( surface ).minImageCount << std::endl;
    //std::cout << "Max image count is "
    //          << gpu.getSurfaceCapabilitiesKHR( surface ).maxImageCount << std::endl;

    vk::SwapchainCreateInfoKHR swapchainCI {
        //        .flags =
        //        vk::SwapchainCreateFlagBitsKHR::eMutableFormat,
        .surface       = surface,
        .minImageCount = std::max< std::uint32_t >(
        std::min< std::uint32_t >(
        gpu.getSurfaceCapabilitiesKHR( surface ).maxImageCount,
        countSwapchainsOptimal ),
        gpu.getSurfaceCapabilitiesKHR( surface ).minImageCount ),
        .imageFormat      = needFormat.format,
        .imageColorSpace  = needFormat.colorSpace,
        .imageExtent      = gpu.getSurfaceCapabilitiesKHR( surface ).currentExtent,
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment |
                      vk::ImageUsageFlagBits::eTransferDst |
                      vk::ImageUsageFlagBits::eTransferSrc,
        .imageSharingMode      = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = queueConf.queueFamilyIndex,
        //        .preTransform = gpu.getSurfaceCapabilitiesKHR( surface ).currentTransform,
        .presentMode  = presentMode,
        .clipped      = VK_FALSE,
        .oldSwapchain = oldSwapchain
    };

    //    std::cout << std::endl << "Swapchain is created" << std::endl;

    return CreatedSwapchainInfo { .swapchain =
                                  logicDev.createSwapchainKHR( swapchainCI ),
                                  .swapchainExtent = swapchainCI.imageExtent };
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

std::vector< vk::ImageBlit >
initImageBlitRegions( xcbwraper::WindowShared srcWindow,
                      xcbwraper::WindowShared dstWindow ) {
    vk::ArrayWrapper1D< vk::Offset3D, 2 > srcBound {
        { vk::Offset3D { .x = 0, .y = 0, .z = 0 },
          vk::Offset3D { .x = srcWindow->getProperties().getGeometry().width,
                         .y = srcWindow->getProperties().getGeometry().height,
                         .z = 1 } }
    };

    vk::ArrayWrapper1D< vk::Offset3D, 2 > dstBound {
        { vk::Offset3D { .x = 0, .y = 0, .z = 0 },
          vk::Offset3D { .x = dstWindow->getProperties().getGeometry().width,
                         .y = dstWindow->getProperties().getGeometry().height,
                         .z = 1 } }
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

    return std::vector< vk::ImageBlit > { { vk::ImageBlit {
    .srcSubresource = srcSubresource,
    .srcOffsets     = srcBound,
    .dstSubresource = dstSubresource,
    .dstOffsets     = dstBound } } };
}

[[nodiscard]] std::vector< vk::BufferImageCopy > initBufferImageCopyRegions
[[maybe_unused]] ( xcbwraper::WindowShared srcWindow ) {
    vk::ImageSubresourceLayers srcSubresource { .aspectMask =
                                                vk::ImageAspectFlagBits::eColor,
                                                .mipLevel       = 0,
                                                .baseArrayLayer = 0,
                                                .layerCount     = 1 };

    return std::vector< vk::BufferImageCopy > { { vk::BufferImageCopy {
    .bufferOffset     = 0,
    .imageSubresource = srcSubresource,
    .imageOffset      = { 0, 0, 0 },
    .imageExtent =
    vk::Extent3D { .width  = srcWindow->getProperties().getGeometry().width,
                   .height = srcWindow->getProperties().getGeometry().height,
                   .depth  = 1 } } } };
}

void fillCmdBuffersForPresentComposite
[[maybe_unused]] ( QueueFamilyIndex                           queueFamilyIndex,
                   CommandBuffersVec &                        commandBuffers,
                   ImageVec &                                 swapchainImages,
                   VulkanGraphicRender::CmdBufferInitConfig & cmdBufferConf,
                   const vk::Buffer &                   srcBuffer [[maybe_unused]],
                   const vk::Image &                    srcImage,
                   const std::vector< vk::ImageBlit > & srcToDstBlitRegions,
                   const std::vector< vk::BufferImageCopy > & srcBufferImgCopyRegions
                   [[maybe_unused]] ) {
    cmdBufferConf.srcImageInitMemoryBarier.srcQueueFamilyIndex = queueFamilyIndex;
    cmdBufferConf.srcImageInitMemoryBarier.dstQueueFamilyIndex = queueFamilyIndex;
    cmdBufferConf.srcImageInitMemoryBarier.image               = srcImage;
    cmdBufferConf.srcImageInitMemoryBarier.subresourceRange =
    cmdBufferConf.ranges.at( 0 );

    cmdBufferConf.bufferImageCopyMemoryBarier.srcQueueFamilyIndex = queueFamilyIndex;
    cmdBufferConf.bufferImageCopyMemoryBarier.dstQueueFamilyIndex = queueFamilyIndex;
    cmdBufferConf.bufferImageCopyMemoryBarier.image               = srcImage;
    cmdBufferConf.bufferImageCopyMemoryBarier.subresourceRange =
    cmdBufferConf.ranges.at( 0 );

    cmdBufferConf.imageMemoryBarierPresentToClear.srcQueueFamilyIndex =
    queueFamilyIndex;
    cmdBufferConf.imageMemoryBarierPresentToClear.dstQueueFamilyIndex =
    queueFamilyIndex;
    cmdBufferConf.imageMemoryBarierPresentToClear.subresourceRange =
    cmdBufferConf.ranges.at( 0 );

    cmdBufferConf.imageMemoryBarierClearToPresent.srcQueueFamilyIndex =
    queueFamilyIndex;
    cmdBufferConf.imageMemoryBarierClearToPresent.dstQueueFamilyIndex =
    queueFamilyIndex;
    cmdBufferConf.imageMemoryBarierClearToPresent.subresourceRange =
    cmdBufferConf.ranges.at( 0 );

    const vk::ClearColorValue clearColorValue(
    std::array< float, 4 > { 0.5f, 0.5f, 0.5f, 1.0f } );

    const std::array< const vk::ImageSubresourceRange, 1 > ranges {
        vk::ImageSubresourceRange { .aspectMask   = vk::ImageAspectFlagBits::eColor,
                                    .baseMipLevel = 0,
                                    .levelCount   = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1 }
    };

    for ( std::uint32_t i = 0; i < swapchainImages.size(); ++i ) {
        cmdBufferConf.imageMemoryBarierClearToPresent.image =
        swapchainImages.at( i );
        cmdBufferConf.imageMemoryBarierPresentToClear.image =
        swapchainImages.at( i );

        commandBuffers.at( i ).begin( cmdBufferConf.cmdBufferBI );

        commandBuffers.at( i ).pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlags(),
        {},
        {},
        { cmdBufferConf.srcImageInitMemoryBarier,
          cmdBufferConf.imageMemoryBarierPresentToClear } );

        //        commandBuffers.at( i ).waitEvents( { cmdBufferConf.bufferImageCopied },
        //                                           vk::PipelineStageFlagBits::eHost,
        //                                           vk::PipelineStageFlagBits::eTransfer,
        //                                           {},
        //                                           {},
        //                                           {} );
        //
        //        commandBuffers.at( i ).copyBufferToImage(
        //        srcBuffer,
        //        srcImage,
        //        vk::ImageLayout::eTransferDstOptimal,
        //        srcBufferImgCopyRegions );
        //
        //        commandBuffers.at( i ).resetEvent( cmdBufferConf.bufferImageCopied,
        //                                           vk::PipelineStageFlagBits::eTransfer );

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
        { //        { cmdBufferConf.bufferImageCopyMemoryBarier,
          cmdBufferConf.imageMemoryBarierPresentToClear } );

        //        commandBuffers.at( i ).setEvent( cmdBufferConf.needRefrashExtent,
        //                                         vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eHost );
        //
        //        commandBuffers.at( i ).waitEvents( { cmdBufferConf.extentIsActual },
        //                                           vk::PipelineStageFlagBits::eHost,
        //                                           vk::PipelineStageFlagBits::eTransfer,
        //                                           {},
        //                                           {},
        //                                           {} );

        commandBuffers.at( i ).blitImage( srcImage,
                                          vk::ImageLayout::eTransferSrcOptimal,
                                          swapchainImages.at( i ),
                                          vk::ImageLayout::eTransferDstOptimal,
                                          srcToDstBlitRegions,
                                          vk::Filter::eLinear );

        //        commandBuffers.at( i ).resetEvent( cmdBufferConf.extentIsActual,
        //                                           vk::PipelineStageFlagBits::eTransfer );

        commandBuffers.at( i ).pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::DependencyFlags(),
        {},
        {},
        { cmdBufferConf.imageMemoryBarierClearToPresent } );

        commandBuffers.at( i ).end();
    }
}

}   // namespace

VulkanBase::VulkanBase( const CreateInfo & info ) :
mInstance( info.instance ), mGpu( info.physDev ),
mExtansions(
info.extansions ) /* swapchainImages( info.swapchain == vk::SwapchainKHR()
                 ? ImageVec()
                 : info.queueConf.logicDev.getSwapchainImagesKHR( info.swapchain ) ),
queueConf( info.queueConf ) */
{}

VulkanBase::~VulkanBase() = default;

VulkanGraphicRender::VulkanGraphicRender(
const VulkanBase::CreateInfo &          baseInfo,
const VulkanGraphicRender::CreateInfo & graphicRenderCreateInfo ) :
VulkanBase( baseInfo ),
//mXcbConnect(  ),
mXcbConnect( graphicRenderCreateInfo.xcbConnect ),
mDstWindow( graphicRenderCreateInfo.dstXcbWindow ), /*
mComposite( graphicRenderCreateInfo.xcbConnect )*/
mCurrentSwapchainExtentMutex( std::make_unique< std::mutex >() ),
mDpy( XOpenDisplay( nullptr ) ) {
    {
        vk::XcbSurfaceCreateInfoKHR surfaceCI { .connection = *mXcbConnect,
                                                .window     = *mDstWindow };
        mSurface = mInstance.createXcbSurfaceKHR( surfaceCI );
    }

    //    mGpu = getDiscreteGpu( mInstance );
    std::cout << "Discrete GPU is : " << mGpu.getProperties().deviceName << std::endl
              << std::endl;

    //    {
    //        auto dpysProps = mGpu.getDisplayPropertiesKHR();
    //
    //        std::cout << "Dysplays number is " << dpysProps.size() << std::endl
    //                  << std::endl;
    //
    //        for ( auto && dpyProp : dpysProps ) {
    //            std::cout << "Display name is " << dpyProp.displayName << std::endl
    //                      << "Display resolution is " << dpyProp.physicalResolution.width
    //                      << "x" << dpyProp.physicalResolution.height << std::endl;
    //        }
    //
    //        std::cout << "Display supported transforms is "
    //                  << vk::to_string( dpysProps.at( 0 ).supportedTransforms )
    //                  << std::endl;
    //
    //        auto planesProps = mGpu.getDisplayPlanePropertiesKHR();
    //
    //        std::cout << "Planes count is " << planesProps.size() << std::endl;
    //
    //        std::cout << std::endl;
    //
    //        auto dpy0 = dpysProps.at( 0 ).display;
    //
    //        std::uint32_t stackIndex {};
    //        std::uint32_t planeIndex {};
    //        for ( ; planeIndex < planesProps.size(); ++planeIndex ) {
    //            if ( planesProps.at( planeIndex ).currentDisplay == dpy0 ) {
    //                std::cout << planesProps.at( planeIndex ).currentStackIndex
    //                          << std::endl;
    //                stackIndex = planesProps.at( planeIndex ).currentStackIndex;
    //                break;
    //            }
    //        }
    //
    //        //if (planeIndex >= planesProps.size())
    //        //    throw std::runtime_error("Plane is not found!!!");
    //
    //        std::cout << std::endl;
    //
    //        std::cout << "Plane index is " << planeIndex << std::endl << std::endl;
    //        std::cout << "Plane stack index is " << stackIndex << std::endl << std::endl;
    //
    //        auto dpy0ModeProps = mGpu.getDisplayModePropertiesKHR( dpy0 );
    //
    //        for ( auto && dpy0ModeProp : dpy0ModeProps )
    //            std::cout << "Mode " << dpy0ModeProp.parameters.visibleRegion.width
    //                      << "x" << dpy0ModeProp.parameters.visibleRegion.height << " "
    //                      << dpy0ModeProp.parameters.refreshRate << std::endl;
    //
    //        //auto axd = PFN_vkAcquireXlibDisplayEXT(
    //        //vkGetInstanceProcAddr( mInstance, "vkAcquireXlibDisplayEXT" ) );
    //        //auto res = axd( mGpu, mDpy, dpy0 );
    //        //if ( res != VK_SUCCESS )
    //        //    throw std::runtime_error( "Display is not allowed " +
    //        //                              std::to_string( res ) );
    //
    //        //if ( mGpu.acquireXlibDisplayEXT( mDpy, dpy0 ) != vk::Result::eSuccess )
    //        //    throw std::runtime_error( "Display is not allowed" );
    //        //
    //        //RROutput rr{};
    //        //dpy0 = mGpu.getRandROutputDisplayEXT( *mDpy, rr ) ;
    //
    //        vk::DisplaySurfaceCreateInfoKHR displaySurfaceCI {
    //            .flags           = {},
    //            .displayMode     = dpy0ModeProps.at( 0 ).displayMode,
    //            .planeIndex      = planeIndex,
    //            .planeStackIndex = 8,
    //            .transform       = vk::SurfaceTransformFlagBitsKHR::eIdentity,
    //            .globalAlpha     = 0.5f,
    //            .alphaMode       = vk::DisplayPlaneAlphaFlagBitsKHR::eGlobal,
    //            .imageExtent     = dpysProps.at( 0 ).physicalResolution
    //        };
    //
    //        mSurface = mInstance.createDisplayPlaneSurfaceKHR( displaySurfaceCI );
    //    }

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
    .flags = vk::CommandPoolCreateFlagBits::eTransient /*|
             vk::CommandPoolCreateFlagBits::eResetCommandBuffer*/
    ,
    .queueFamilyIndex = mQueueConfigs.at( 0 ).queueFamilyIndex } );

    {
        vk::SurfaceFormatKHR needFormat { .format = vk::Format::eB8G8R8A8Unorm,
                                          .colorSpace =
                                          vk::ColorSpaceKHR::eSrgbNonlinear };
        mSurfaceFormat = matchNeedFormatOrFirst( mGpu, mSurface, needFormat );
    }

    std::cout << "TEST" << std::endl;
    auto [ swapchain, currentSwapchainExtent ] =
    swapchainInit( mGpu,
                   mLogicDev,
                   mSurface,
                   mQueueConfigs.at( 0 ),
                   countSwapChainBuffers,
                   mSurfaceFormat,
                   {} );
    mSwapchain              = swapchain;
    mCurrentSwapchainExtent = currentSwapchainExtent;

    mSwapchainImages = mLogicDev.getSwapchainImagesKHR( mSwapchain );

    mSwapchainCmdBuffers =
    commandBuffersInit( mLogicDev, mCommandPool, mSwapchainImages.size() );

    mSemaphores.push_back( mLogicDev.createSemaphore( {} ) );
    mSemaphores.push_back( mLogicDev.createSemaphore( {} ) );

    {
        composite::Composite srcComposite( graphicRenderCreateInfo.xcbConnect );
        mSrcRamInfo.window = srcComposite.getRootWindow();
        if ( !mSrcRamInfo.window->getProperties().isViewable() )
            throw std::runtime_error( "Selected overlay window id not viewvable" );
        std::cout << "Selected window: "
                  << mSrcRamInfo.window->getProperties().getClass() << std::endl
                  << "with id: " << mSrcRamInfo.window->getProperties().getID()
                  << std::endl;
    }

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

        constexpr std::string_view searchWindowClass( "Alacritty" );

        for ( auto && client : clientsVec )
            if ( client.getClass() == searchWindowClass )
                windowCI.window = client.getID();
        mSrcRamInfo.window = std::make_shared< xcbwraper::Window >( windowCI );
    }

    {
        auto bitPerRgb = mSrcRamInfo.window->getProperties().getBitPerRGB() * 3;

        std::cout << "SrcWindow has " << static_cast< std::uint32_t >( bitPerRgb )
                  << " bit per rgb." << std::endl;

        auto srcWindowGeometry = mSrcRamInfo.window->getProperties().getGeometry();

        mSrcBufferImgCopyRegions = initBufferImageCopyRegions( mSrcRamInfo.window );

        vk::ExternalMemoryImageCreateInfo externalMemCI {
            .handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT
        };

        auto srcImageCI = vk::ImageCreateInfo {
            .pNext       = &externalMemCI,
            .flags       = {},
            .imageType   = vk::ImageType::e2D,
            //.format      = mSurfaceFormat.format,
            .format      = vk::Format::eR8G8B8A8Unorm,
            .extent      = vk::Extent3D { .width  = srcWindowGeometry.width,
                                     .height = srcWindowGeometry.height,
                                     .depth  = 1 },
            .mipLevels   = 1,
            .arrayLayers = 1,
            .tiling      = vk::ImageTiling::eOptimal,
            .usage       = vk::ImageUsageFlagBits::eTransferDst |
                     vk::ImageUsageFlagBits::eTransferSrc,
            .sharingMode           = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices   = &mQueueConfigs.at( 0 ).queueFamilyIndex,
            .initialLayout         = vk::ImageLayout::eUndefined
        };
        mSrcRamInfo.image = mLogicDev.createImage( srcImageCI );

        auto dri3Fd = mSrcRamInfo.window->getImageDataDri3FD();
            throw std::runtime_error( "********* TEST ********" );
        auto dmaFd  = dri3Fd.getFd();

        auto getMemFdProps = PFN_vkGetMemoryFdPropertiesKHR(
        vkGetDeviceProcAddr( mLogicDev, "vkGetMemoryFdPropertiesKHR" ) );

        VkMemoryFdPropertiesKHR memFdProps;
        getMemFdProps( mLogicDev,
                       VkExternalMemoryHandleTypeFlagBits::
                       VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                       dmaFd,
                       &memFdProps );

        //auto memFdProps = mLogicDev.getMemoryFdPropertiesKHR(
        //vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT, dmaFd );

        auto srcImageMemReq =
        mLogicDev.getImageMemoryRequirements( mSrcRamInfo.image );

        vk::MemoryDedicatedAllocateInfo memDedicatedAllocInfo { .image =
                                                                mSrcRamInfo.image };

        vk::ImportMemoryFdInfoKHR importMemFdInfo {
            .pNext      = &memDedicatedAllocInfo,
            .handleType = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT,
            .fd         = dmaFd
        };

        std::uint32_t memTypeIndex {};
        for ( ; memTypeIndex < 32; ++memTypeIndex )
            if ( memFdProps.memoryTypeBits & ( 1 << memTypeIndex ) )
                break;

        if ( memTypeIndex >= 32 )
            throw std::runtime_error( "memFdTypeIndex is bad!!!" );

        vk::MemoryAllocateInfo memAllocInfo { .pNext           = &importMemFdInfo,
                                              .allocationSize  = srcImageMemReq.size,
                                              .memoryTypeIndex = memTypeIndex };

        mSrcRamInfo.imageRam = mLogicDev.allocateMemory( memAllocInfo );

        mLogicDev.bindImageMemory(mSrcRamInfo.image, mSrcRamInfo.imageRam, 0);

        const vk::DeviceSize bufferSize =
        srcWindowGeometry.height * srcWindowGeometry.width * bitPerRgb;

        const vk::BufferUsageFlags bufferUsageFlags {
            vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eTransferSrc
        };

        vk::BufferCreateInfo srcBufferCI { .size  = bufferSize,
                                           .usage = bufferUsageFlags,
                                           .sharingMode =
                                           vk::SharingMode::eExclusive,
                                           .queueFamilyIndexCount = 1,
                                           .pQueueFamilyIndices =
                                           &mQueueConfigs.at( 0 ).queueFamilyIndex };
        mSrcRamInfo.imageBuffer = mLogicDev.createBuffer( srcBufferCI );
    }

    //    {
    //        constexpr vk::MemoryPropertyFlags memReqFlags {
    //            vk::MemoryPropertyFlagBits::eDeviceLocal
    //        };
    //
    //        mSrcRamInfo.imageRam =
    //        createMemAndBind( mGpu, mLogicDev, mSrcRamInfo.image, memReqFlags );
    //    }
    //
    //    {
    //        constexpr vk::MemoryPropertyFlags memReqFlags {
    //            vk::MemoryPropertyFlagBits::eHostVisible |
    //            vk::MemoryPropertyFlagBits::eDeviceLocal
    //        };
    //
    //        const auto memRequarements =
    //        mLogicDev.getBufferMemoryRequirements( mSrcRamInfo.imageBuffer );
    //
    //        mSrcRamInfo.imageBufferRam =
    //        createMemAndBind( mGpu, mLogicDev, mSrcRamInfo.imageBuffer, memReqFlags );
    //
    //        mSrcRamInfo.imageDataBridge =
    //        mLogicDev.mapMemory( mSrcRamInfo.imageBufferRam, 0, memRequarements.size );
    //        //
    //        //        mLogicDev.unmapMemory( mSrcRamInfo.imageBufferRam );
    //    }

    mSrcToDstBlitRegions = initImageBlitRegions( mSrcRamInfo.window, mDstWindow );

    auto & blitRegion0DstOffset1 = mSrcToDstBlitRegions.at( 0 ).dstOffsets.at( 1 );
    auto   surfaceExtents = mGpu.getSurfaceCapabilitiesKHR( mSurface ).currentExtent;
    blitRegion0DstOffset1.x = surfaceExtents.width;
    blitRegion0DstOffset1.y = surfaceExtents.height;

    mSrcRamInfo.imageShm = std::make_shared< posix::SharedMemory >();

    vk::EventCreateInfo eventCI { .flags = {} };

    mCmdConfig.bufferImageCopied = mLogicDev.createEvent( eventCI );
    mCmdConfig.extentIsActual    = mLogicDev.createEvent( eventCI );
    mCmdConfig.needRefrashExtent = mLogicDev.createEvent( eventCI );

    //    mThreadPool.blitRegionsResizer = std::thread( [ this ]() {
    //        auto & blitRegion0DstOffset1 =
    //        mSrcToDstBlitRegions.at( 0 ).dstOffsets.at( 1 );
    //
    //        while ( true ) {
    //            while ( mLogicDev.getEventStatus( mCmdConfig.needRefrashExtent ) !=
    //                    vk::Result::eEventSet ) {
    //                using namespace std::chrono_literals;
    //                std::this_thread::sleep_for( 100us );
    //            }
    //
    //            mCurrentSurfaceExtent =
    //            mGpu.getSurfaceCapabilitiesKHR( mSurface ).currentExtent;
    //
    //            {
    //                const std::scoped_lock< std::mutex > lockSwapchainExtent(
    //                *mCurrentSwapchainExtentMutex );
    //
    //                blitRegion0DstOffset1.y = std::min( mCurrentSurfaceExtent.height,
    //                                                    mCurrentSwapchainExtent.height );
    //
    //                blitRegion0DstOffset1.x = std::min( mCurrentSurfaceExtent.width,
    //                                                    mCurrentSwapchainExtent.width );
    //
    //                mLogicDev.setEvent( mCmdConfig.extentIsActual );
    //
    //                mLogicDev.resetEvent( mCmdConfig.needRefrashExtent );
    //
    //                mCurrentSwapchainExtent = mCurrentSurfaceExtent;
    //            }
    //            //            using namespace std::chrono_literals;
    //            //            std::this_thread::sleep_for( 100us );
    //        }
    //    } );
    //
    //    mThreadPool.blitRegionsResizer.detach();

    fillCmdBuffersForPresentComposite( mQueueConfigs.at( 0 ).queueFamilyIndex,
                                       mSwapchainCmdBuffers,
                                       mSwapchainImages,
                                       mCmdConfig,
                                       mSrcRamInfo.imageBuffer,
                                       mSrcRamInfo.image,
                                       mSrcToDstBlitRegions,
                                       mSrcBufferImgCopyRegions );

    //    mDstWindow->unmap();

    std::cout << std::endl
              << "Command buffer count :" << mSwapchainCmdBuffers.size()
              << std::endl;
    std::cout << std::endl
              << "Image count : " << mSwapchainImages.size() << std::endl;
}

VulkanGraphicRender::~VulkanGraphicRender() {}

void VulkanGraphicRender::draw() {
    try {
        mLogicDev.waitIdle();

        constexpr std::uint64_t waitTimeNanoSeconds = 10;
        const auto              asqNextImgIndex     = mLogicDev.acquireNextImageKHR(
        mSwapchain, waitTimeNanoSeconds, mSemaphores.at( 0 ) );

        if ( asqNextImgIndex.result != vk::Result::eSuccess )
            std::cout << std::endl << "TEST" << std::endl;

        auto & subInfo0 = mDrawConf.subInfo.at( 0 );

        subInfo0.pWaitSemaphores   = &mSemaphores.at( 0 );
        subInfo0.pWaitDstStageMask = mDrawConf.pipelineStageFlags.data();
        subInfo0.pCommandBuffers = &mSwapchainCmdBuffers.at( asqNextImgIndex.value );
        subInfo0.pSignalSemaphores = &mSemaphores.at( 1 );

        //        mLogicDev.resetEvent( mCmdConfig.bufferImageCopied );

        auto & queue0 = mQueues.at( 0 );
        queue0.submit( mDrawConf.subInfo );

        {
            mSrcRamInfo.imageShm =
            mSrcRamInfo.window->getImageData( mSrcRamInfo.imageShm );

            std::copy(
            mSrcRamInfo.imageShm->getData< std::uint8_t >().begin(),
            mSrcRamInfo.imageShm->getData< std::uint8_t >().end(),
            static_cast< std::uint8_t * >( mSrcRamInfo.imageDataBridge ) );
        }

        mLogicDev.setEvent( mCmdConfig.bufferImageCopied );

        //        while ( mLogicDev.getEventStatus( mCmdConfig.needRefrashExtent ) ==
        //                vk::Result::eEventReset ) {
        //            //using namespace std::chrono_literals;
        //            //std::this_thread::sleep_for( 100us );
        //        }
        //
        //        mLogicDev.setEvent( mCmdConfig.extentIsActual );
        //
        //        mLogicDev.resetEvent( mCmdConfig.needRefrashExtent );

        //mCurrentSwapchainExtent = mCurrentSurfaceExtent;

        mDrawConf.present.pWaitSemaphores = &mSemaphores.at( 1 );
        mDrawConf.present.pSwapchains     = &mSwapchain;
        mDrawConf.present.pImageIndices   = &asqNextImgIndex.value;

        queue0.waitIdle();

        [[maybe_unused]] auto result = queue0.presentKHR( mDrawConf.present );

    } catch ( const vk::OutOfDateKHRError & e ) {
        update();

        mLogicDev.resetEvent( mCmdConfig.extentIsActual );

        std::cout << e.what() << std::endl;
    }
}

void VulkanGraphicRender::update() {
    mLogicDev.waitIdle();

    {
        const std::scoped_lock< std::mutex > lockSwapchainExtent(
        *mCurrentSwapchainExtentMutex );

        auto [ swapchain, currentSwapchainExtent ] =
        swapchainInit( mGpu,
                       mLogicDev,
                       mSurface,
                       mQueueConfigs.at( 0 ),
                       countSwapChainBuffers,
                       mSurfaceFormat,
                       mSwapchain );
        mSwapchain              = swapchain;
        mCurrentSwapchainExtent = currentSwapchainExtent;
    }

    mSwapchainImages = mLogicDev.getSwapchainImagesKHR( mSwapchain );

    //    for ( auto cmdBuffer : mSwapchainCmdBuffers )
    //        cmdBuffer.reset();

    mLogicDev.resetCommandPool( mCommandPool, {} );

    //    blitRegion0DstOffset1.y =
    //    mGpu.getSurfaceCapabilitiesKHR( mSurface ).currentExtent.height;
    //    blitRegion0DstOffset1.x =
    //    mGpu.getSurfaceCapabilitiesKHR( mSurface ).currentExtent.width;

    fillCmdBuffersForPresentComposite( mQueueConfigs.at( 0 ).queueFamilyIndex,
                                       mSwapchainCmdBuffers,
                                       mSwapchainImages,
                                       mCmdConfig,
                                       mSrcRamInfo.imageBuffer,
                                       mSrcRamInfo.image,
                                       mSrcToDstBlitRegions,
                                       mSrcBufferImgCopyRegions );
}

void VulkanGraphicRender::printSurfaceExtents() const {
    auto surfaceCapabilities = mGpu.getSurfaceCapabilitiesKHR( mSurface );

    auto surfaceExtents = surfaceCapabilities.currentExtent;

    std::cout << std::endl
              << "Surface resolution is " << surfaceExtents.width << "x"
              << surfaceExtents.height << std::endl
              << std::endl;
}

namespace {

template < class Renderer > concept HasDrawMethod = requires( Renderer renderer ) {
    { renderer.draw() };
};

template < HasDrawMethod Renderer >
void runRenderLoop( std::unique_ptr< Renderer >    renderer,
                    xcbwraper::XcbConnectionShared xcbConnect ) {
    for ( bool breakLoop = false; !breakLoop; ) {
        renderer->draw();

        std::unique_ptr< xcb_generic_event_t > event { xcb_poll_for_event(
        static_cast< xcb_connection_t * >( *xcbConnect ) ) };

        if ( !event )
            continue;

        switch ( event->response_type & ~0x80 ) {
        case XCB_KEY_PRESS:
            if ( reinterpret_cast< xcb_key_press_event_t * >( event.get() )
                 ->detail == 24 ) {
                breakLoop = true;
            }
        }
        //        delete event;
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
                       0,
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
                          .apiVersion         = VK_API_VERSION_1_2 } );

    VulkanBase::Extensions extensions {
        .instance = { VK_KHR_SURFACE_EXTENSION_NAME,
                      VK_KHR_XCB_SURFACE_EXTENSION_NAME,
                      VK_KHR_DISPLAY_EXTENSION_NAME,
                      VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
                      VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME },
        .device   = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME }
    };

    vk::Instance vulkanXCBInstance = vk::createInstance( vk::InstanceCreateInfo {
    .pApplicationInfo = appInfo.get(),
    .enabledExtensionCount =
    static_cast< std::uint32_t >( extensions.instance.size() ),
    .ppEnabledExtensionNames = extensions.instance.data() } );

    auto                   gpu = getDiscreteGpu( vulkanXCBInstance );
    VulkanBase::CreateInfo vulkanBaseCI { .instance   = vulkanXCBInstance,
                                          .physDev    = gpu,
                                          .extansions = extensions };

    auto composite = std::make_shared< composite::Composite >( mXcbConnect );

    auto windowPtr = composite->getCompositeOverleyWindowWithoutInputEvents();

    //    auto windowPtr = std::make_shared< xcbwraper::Window >(
    //    xcbwraper::Window::CreateInfo { mXcbConnect, window } );

    VulkanGraphicRender::CreateInfo vulkanRenderCI { .xcbConnect   = mXcbConnect,
                                                     .dstXcbWindow = windowPtr };

    //    auto renderer =
    //    std::make_unique< VulkanGraphicRender >( vulkanBaseCI, vulkanRenderCI );

    runRenderLoop< VulkanGraphicRender >(
    std::make_unique< VulkanGraphicRender >( vulkanBaseCI, vulkanRenderCI ),
    mXcbConnect );

    //    xcb_destroy_window( static_cast< xcb_connection_t * >( *mXcbConnect ), window );
    //    xcb_flush( static_cast< xcb_connection_t * >( *mXcbConnect ) );
}
}   // namespace core::renderer
