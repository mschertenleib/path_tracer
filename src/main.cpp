#include "geometry.hpp"
#include "vulkan_renderer.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
    try
    {
        const auto scene = load_scene("../resources/bunny.obj");

        Vulkan_renderer renderer(1280, 720, scene);
        renderer.render();
        renderer.store_to_png("image.png");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception thrown\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
