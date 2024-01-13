#include "application.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <iostream>
#include <stdexcept>

#include <cstdlib>

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 4)
        {
            std::cerr << "Usage: " << argv[0]
                      << " <model.obj> <width> <height>\n";
            return EXIT_FAILURE;
        }
        const auto obj_file_name = argv[1];
        const std::uint32_t render_width {std::stoul(argv[2])};
        const std::uint32_t render_height {std::stoul(argv[3])};

        Assimp::Importer importer;
        const auto *scene = importer.ReadFile(
            obj_file_name,
            aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
        if (scene == nullptr)
        {
            std::cerr << importer.GetErrorString() << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "Loaded file \"" << obj_file_name << "\" has "
                  << scene->mNumMeshes << " meshes\n";

        // application_main(obj_file_name, render_width, render_height);

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
