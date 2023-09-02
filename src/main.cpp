#include "geometry.hpp"
#include "renderer.hpp"

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
        const auto geometry = load_obj(argv[1]);
        Vulkan_renderer renderer(1280, 720, geometry);
        renderer.render();
        renderer.store_to_png(argv[2]);
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

    return EXIT_SUCCESS;
}
