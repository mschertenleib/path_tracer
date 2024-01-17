#include "scene.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <stdexcept>

Geometry load_obj(const char *file_name, bool normalize)
{
    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_PP_PTV_NORMALIZE, true);

    const auto *scene = importer.ReadFile(
        file_name,
        aiProcess_Triangulate |
            (normalize ? aiProcess_PreTransformVertices : 0) |
            aiProcess_GenBoundingBoxes | aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType);
    if (scene == nullptr)
    {
        throw std::runtime_error(importer.GetErrorString());
    }

    if (!scene->HasMeshes())
    {
        throw std::runtime_error("Scene has no meshes");
    }

    Geometry geometry {};

    const auto *const mesh = scene->mMeshes[0];
    geometry.vertices.resize(mesh->mNumVertices * 3);
    for (unsigned int i {0}; i < mesh->mNumVertices; ++i)
    {
        geometry.vertices[i * 3 + 0] = mesh->mVertices[i].x;
        geometry.vertices[i * 3 + 1] = mesh->mVertices[i].y;
        geometry.vertices[i * 3 + 2] = mesh->mVertices[i].z;
    }

    geometry.indices.resize(mesh->mNumFaces * 3);
    for (unsigned int i {0}; i < mesh->mNumFaces; ++i)
    {
        geometry.indices[i * 3 + 0] = mesh->mFaces[i].mIndices[0];
        geometry.indices[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
        geometry.indices[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
    }

    return geometry;
}

Camera create_camera(const vec3 &position,
                     const vec3 &look_at,
                     const vec3 &world_up,
                     float focal_length,
                     float sensor_width,
                     float sensor_height)
{
    Camera camera {};
    camera.position = position;
    camera.look_at = look_at;
    // z points into the scene
    camera.dir_z = normalize(look_at - position);
    // x points to the right
    camera.dir_x = normalize(cross(camera.dir_z, world_up));
    // y points down
    camera.dir_y = cross(camera.dir_z, camera.dir_x);
    camera.dir_z *= focal_length;
    camera.dir_x *= sensor_width * 0.5f;
    camera.dir_y *= sensor_height * 0.5f;

    return camera;
}
