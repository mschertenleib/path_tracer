#include "application.hpp"

#include <iostream>
#include <stdexcept>

#include <cstdlib>

int main(int argc, char *argv[])
{
    try
    {
        if (argc > 2 || (argc == 2 && argv[1][0] == '-'))
        {
            std::cout << "Usage: " << argv[0] << " [<file>]\n";
            return EXIT_SUCCESS;
        }

        const char *file_name {nullptr};
        if (argc == 2)
        {
            file_name = argv[1];
        }

        application_main(file_name);

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        std::cout << std::flush;
        std::cerr << "Exception thrown: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cout << std::flush;
        std::cerr << "Unknown exception thrown\n";
        return EXIT_FAILURE;
    }
}
