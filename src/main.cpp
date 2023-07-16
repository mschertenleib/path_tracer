#include "vulkan_renderer.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
    try
    {
        // FIXME: VK_ERROR_DEVICE_LOST depending on render size
        Vulkan_renderer renderer(1280, 720);
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
