#include "vulkan_renderer.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
    try
    {
        init();
        run();
        shutdown();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception thrown\n";
        shutdown();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
