#include "geometry.hpp"
#include "renderer.hpp"
#include "utility.hpp"

// Temporary, just so we can use vk::DynamicLoader to load vkGetInstanceProcAddr
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <chrono>
#include <iostream>
#include <stdexcept>

#include <cstdlib>

namespace
{

class Timer
{
public:
    inline void start(const char *name) noexcept
    {
        m_name = name;
        m_start = std::chrono::steady_clock::now();
    }
    inline void stop()
    {
        const auto stop = std::chrono::steady_clock::now();
        std::cout
            << m_name << ": "
            << std::chrono::duration<double, std::milli>(stop - m_start).count()
            << " ms\n";
    }

private:
    const char *m_name {""};
    std::chrono::steady_clock::time_point m_start {};
};

} // namespace

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

        Timer t;

        t.start("create_vulkan_context");
        auto context = create_vulkan_context(vkGetInstanceProcAddr);
        SCOPE_EXIT([&] { destroy_vulkan_context(context); });
        t.stop();

        t.start("load_obj");
        const auto geometry = load_obj(argv[1]);
        t.stop();

        t.start("load_scene");
        load_scene(context, 1280, 720, geometry);
        SCOPE_EXIT([&] { destroy_scene_resources(context); });
        t.stop();

        t.start("render");
        render(context);
        t.stop();

        t.start("write_to_png");
        write_to_png(context, argv[2]);
        t.stop();

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
