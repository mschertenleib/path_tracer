#include "renderer.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        Renderer_state renderer_state {};
        renderer_init(renderer_state);
        // renderer_load_scene(renderer_state, ...);
        // renderer_render(renderer_state, ...);
        // renderer_write_image(renderer_state, ...);
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
