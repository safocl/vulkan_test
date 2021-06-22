#include <cassert>
#include <exception>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string_view>
#include <stdexcept>

#include "vulkanrender.hpp"

int main() {
    auto vulkanRenderInstance = core::renderer::VulkanRenderInstance::init();
    vulkanRenderInstance->run();

    return EXIT_SUCCESS;
}
