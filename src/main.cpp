#include "scope_guard.hpp"
#include "vulkan_renderer.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
    try
    {
        init();
        SCOPE_EXIT(shutdown);
        run();
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
