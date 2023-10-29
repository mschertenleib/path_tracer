#include "renderer.hpp"

// Temporary, just so we can use vk::DynamicLoader to load vkGetInstanceProcAddr
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <stdexcept>

#include <cstdlib>

int main()
{
    try
    {
        vk::DynamicLoader dl;
        const auto vkGetInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>(
                "vkGetInstanceProcAddr");

        auto context = create_vulkan_context(vkGetInstanceProcAddr);
        destroy_vulkan_context(context);

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception thrown: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception thrown\n";
        return EXIT_FAILURE;
    }
}
