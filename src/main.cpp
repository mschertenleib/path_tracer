#include "geometry.hpp"
#include "vulkan_renderer.hpp"

#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " <input.obj> <output.png>\n";
            return EXIT_FAILURE;
        }
        const auto scene = load_scene(argv[1]);
        Vulkan_renderer renderer(1280, 720, scene);
        renderer.render();
        renderer.store_to_png(argv[2]);
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
