#include "simd.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        std::cout << "Hello, wanderer!\n";
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
