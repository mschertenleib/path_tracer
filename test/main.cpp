#include "geometry.hpp"
#include "renderer.hpp"
#include "utility.hpp"

// Temporary, just so we can use vk::DynamicLoader to load vkGetInstanceProcAddr
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <stdexcept>

#include <cstdlib>

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " <model.obj> <output.png>\n";
            return EXIT_FAILURE;
        }

        vk::DynamicLoader dl;
        const auto vkGetInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>(
                "vkGetInstanceProcAddr");

        auto context = create_vulkan_context(vkGetInstanceProcAddr);
        SCOPE_EXIT([&] { destroy_vulkan_context(context); });

        const auto geometry = load_obj(argv[1]);

        load_scene(context, 1280, 720, geometry);
        SCOPE_EXIT([&] { destroy_scene_resources(context); });

        render(context);

        write_to_png(context, argv[2]);

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
