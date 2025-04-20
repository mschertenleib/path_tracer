#include "application.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char *argv[])
{
    try
    {
        if (argc > 2 || (argc == 2 && argv[1][0] == '-'))
        {
            std::cout << "Usage: "
                      << std::filesystem::path(argv[0]).filename().string()
                      << " [<file>]\n";
            return EXIT_FAILURE;
        }

        run(argc == 2 ? argv[1] : nullptr);

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        std::cout << std::flush;
        std::cerr << "Exception thrown: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cout << std::flush;
        std::cerr << "Unknown exception thrown" << std::endl;
        return EXIT_FAILURE;
    }
}
