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
    auto vulkanRenderInstance = core::renderer::VulkanRenderInstance::init();
    vulkanRenderInstance->run();

    return EXIT_SUCCESS;
}
