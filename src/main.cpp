#include "vulkan_renderer.hpp"

#include "tiny_obj_loader.h"

#include <cstdlib>
#include <iostream>

int main()
{
    try
    {

        tinyobj::ObjReader reader;
        reader.ParseFromFile("../resources/bunny.obj");
        if (!reader.Valid())
        {
            throw std::runtime_error(reader.Error());
        }

        const auto &shapes = reader.GetShapes();
        if (shapes.size() != 1)
        {
            throw std::runtime_error("OBJ file contains more than one shape");
        }

        const auto &indices = shapes.front().mesh.indices;

        std::vector<std::uint32_t> vertex_indices(indices.size());
        for (std::size_t i {}; i < indices.size(); ++i)
        {
            vertex_indices[i] =
                static_cast<std::uint32_t>(indices[i].vertex_index);
        }

        const auto &obj_vertices = reader.GetAttrib().vertices;
        const auto &obj_normals = reader.GetAttrib().normals;
        std::vector<float> normals(reader.GetAttrib().vertices.size());
        for (const auto index : indices)
        {
            const auto vertex_index =
                static_cast<std::size_t>(index.vertex_index);
            const auto normal_index =
                static_cast<std::size_t>(index.normal_index);
            normals[vertex_index * 3 + 0] = obj_normals[normal_index * 3 + 0];
            normals[vertex_index * 3 + 1] = obj_normals[normal_index * 3 + 1];
            normals[vertex_index * 3 + 2] = obj_normals[normal_index * 3 + 2];
        }

        Vulkan_renderer renderer(
            1280, 720, obj_vertices, vertex_indices, normals);
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
