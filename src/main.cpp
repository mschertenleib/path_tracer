#include "geometry.hpp"
#include "renderer.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " <model.obj> <output.png>\n";
            return EXIT_FAILURE;
        }

        auto renderer = create_renderer();
        const auto geometry = load_obj(argv[1]);
        load_scene(renderer, 1280, 720, geometry);
        render(renderer);
        write_to_png(renderer, argv[2]);
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
