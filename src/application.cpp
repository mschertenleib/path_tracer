#include "application.hpp"
#include "camera.hpp"
#include "renderer.hpp"
#include "vec3.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <ImGuizmo.h>

#include <assimp/config.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/metadata.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>

#include <tinyfiledialogs.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numbers>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{

struct Deleter
{
    void operator()(GLFWwindow *window)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    void operator()(ImGuiContext *context)
    {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(context);
    }
};

struct Application_state
{
    std::unique_ptr<GLFWwindow, Deleter> window;
    std::unique_ptr<ImGuiContext, Deleter> imgui_context;
    Vulkan_context context;
    Vulkan_render_resources render_resources;
    Assimp::Importer importer; // FIXME: this is just to keep the imported scene
                               // alive for debugging. We will probably end up
                               // having our own scene representation anyways.
    const aiScene *scene;
    bool scene_loaded;
    Camera camera;
    std::uint32_t render_width;
    std::uint32_t render_height;
};

void remove_quotes(std::string &str)
{
    auto filtered_str =
        std::views::filter(str, [](auto c) { return c != '\'' && c != '\"'; });
    str.assign(filtered_str.begin(), filtered_str.end());
}

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

void glfw_framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    const auto state =
        static_cast<Application_state *>(glfwGetWindowUserPointer(window));
    resize_framebuffer(state->context,
                       static_cast<std::uint32_t>(width),
                       static_cast<std::uint32_t>(height));
}

[[nodiscard]] GLFWwindow *glfw_init()
{
    glfwSetErrorCallback(&glfw_error_callback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    if (!glfwVulkanSupported())
    {
        glfwTerminate();
        throw std::runtime_error("Vulkan loader or ICD have not been found");
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const auto window =
        glfwCreateWindow(1280, 720, "Path Tracer", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetFramebufferSizeCallback(window, &glfw_framebuffer_size_callback);

    return window;
}

[[nodiscard]] ImGuiContext *imgui_init(GLFWwindow *window)
{
    IMGUI_CHECKVERSION();
    auto *const ctx = ImGui::CreateContext();

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();

    float y_scale {1.0f};
    glfwGetWindowContentScale(window, nullptr, &y_scale);
    ImFontConfig font_config {};
    font_config.SizePixels = 13.0f * y_scale;
    io.Fonts->AddFontDefault(&font_config);

    if (!ImGui_ImplGlfw_InitForVulkan(window, true))
    {
        ImGui::DestroyContext(ctx);
        throw std::runtime_error("Failed to initialize ImGui GLFW backend");
    }

    return ctx;
}

void open_scene(Application_state &state, const char *file_name)
{
    state.importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
                                      aiComponent_ANIMATIONS |
                                          aiComponent_BONEWEIGHTS);
    state.importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80);
    state.importer.SetPropertyInteger(
        AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
    state.importer.SetPropertyBool(AI_CONFIG_PP_FD_CHECKAREA, false);

    // FIXME: remove
    state.importer.SetPropertyBool(AI_CONFIG_PP_PTV_NORMALIZE, true);

    const auto *const scene = state.importer.ReadFile(
        file_name,
        // aiProcess_CalcTangentSpace| // TODO: do we want these to be
        // pre-computed, or can we compute them on the fly in the closest hit
        // shader? Also, what if a mesh has normals but no normal map (which is
        // common)? In that case pre-computing tangents/bitangents is just a
        // waste of memory.
        aiProcess_PreTransformVertices // FIXME: remove
            | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate |
            aiProcess_RemoveComponent |
            aiProcess_GenSmoothNormals | // TODO: if the file does not contain
                                         // normals, do we want them to be
                                         // smooth, or do we just move on with
                                         // per-triangle normals computed in the
                                         // closest hit shader?
            aiProcess_ValidateDataStructure |
            aiProcess_RemoveRedundantMaterials | aiProcess_FixInfacingNormals |
            aiProcess_SortByPType | aiProcess_FindDegenerates |
            aiProcess_FindInvalidData | aiProcess_GenUVCoords |
            aiProcess_TransformUVCoords |
            aiProcess_FindInstances | // TODO: remove if the import becomes too
                                      // slow
            aiProcess_EmbedTextures // TODO: do we really want this? If it does
                                    // not consistently embed ALL texture
                                    // files, we will have to load some of
                                    // them manually anyways.
            | aiProcess_GenBoundingBoxes);
    if (scene == nullptr)
    {
        std::string importer_error(state.importer.GetErrorString());
        remove_quotes(importer_error);
        tinyfd_messageBox("Error", importer_error.c_str(), "ok", "error", 1);
        return;
    }

    if (!scene->HasMeshes())
    {
        tinyfd_messageBox("Error", "Scene has no meshes", "ok", "error", 1);
        return;
    }

    state.scene = scene;

    state.render_width = 640;
    state.render_height = 480;

    const vec3 position {0.0f, 0.0f, 3.5f};
    const vec3 target {0.0f, 0.0f, 0.0f};
    const auto vertical_fov = 45.0f / 180.0f * std::numbers::pi_v<float>;
    const auto aspect_ratio = static_cast<float>(state.render_width) /
                              static_cast<float>(state.render_height);
    const float sensor_distance {1.0f};
    const auto sensor_half_height =
        std::tan(vertical_fov * 0.5f) * sensor_distance;
    const auto sensor_half_width = aspect_ratio * sensor_half_height;
    state.camera = create_camera(position,
                                 target,
                                 sensor_distance,
                                 sensor_half_width,
                                 sensor_half_height);

    state.context.device->waitIdle();

    state.render_resources = {};
    state.render_resources = create_render_resources(
        state.context, state.render_width, state.render_height, scene);

    state.scene_loaded = true;
}

void open_scene_with_dialog(Application_state &state)
{
    const auto file_name =
        tinyfd_openFileDialog("Open scene", nullptr, 0, nullptr, nullptr, 0);
    if (file_name != nullptr)
    {
        open_scene(state, file_name);
    }
}

void close_scene(Application_state &state)
{
    if (state.scene_loaded)
    {
        state.context.device->waitIdle();
        state.render_resources = {};
        state.scene_loaded = false;
    }
}

void save_as_png_with_dialog(Application_state &state)
{
    constexpr const char *filter_patterns[] {"*.png"};
    const auto file_name =
        tinyfd_saveFileDialog("Save As",
                              nullptr,
                              static_cast<int>(std::size(filter_patterns)),
                              filter_patterns,
                              nullptr);
    if (file_name != nullptr)
    {
        auto error_message =
            write_to_png(state.context, state.render_resources, file_name);
        if (!error_message.empty())
        {
            remove_quotes(error_message);
            tinyfd_messageBox("Error", error_message.c_str(), "ok", "error", 1);
        }
    }
}

void centered_image(ImTextureID texture_id, float aspect_ratio)
{
    const auto region_size = ImGui::GetContentRegionAvail();
    if (region_size.x > 0.0f && region_size.y > 0.0f)
    {
        const auto region_aspect_ratio = region_size.x / region_size.y;
        const auto cursor_pos = ImGui::GetCursorPos();
        auto width = region_size.x;
        auto height = region_size.y;
        auto x = cursor_pos.x;
        auto y = cursor_pos.y;
        if (aspect_ratio >= region_aspect_ratio)
        {
            height = width / aspect_ratio;
            y += (region_size.y - height) * 0.5f;
        }
        else
        {
            width = height * aspect_ratio;
            x += (region_size.x - width) * 0.5f;
        }
        ImGui::SetCursorPos({x, y});
        ImGui::Image(texture_id, {width, height});
    }
}

void display_node(const aiNode *node)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    if (ImGui::TreeNode(node->mName.C_Str()))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Metadata");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(node->mMetaData != nullptr ? "[metadata]" : "");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Transform");
        ImGui::TableNextColumn();
        const auto tf = node->mTransformation;
        ImGui::Text("%8.3f %8.3f %8.3f %8.3f",
                    static_cast<double>(tf.a1),
                    static_cast<double>(tf.a2),
                    static_cast<double>(tf.a3),
                    static_cast<double>(tf.a4));
        ImGui::Text("%8.3f %8.3f %8.3f %8.3f",
                    static_cast<double>(tf.b1),
                    static_cast<double>(tf.b2),
                    static_cast<double>(tf.b3),
                    static_cast<double>(tf.b4));
        ImGui::Text("%8.3f %8.3f %8.3f %8.3f",
                    static_cast<double>(tf.c1),
                    static_cast<double>(tf.c2),
                    static_cast<double>(tf.c3),
                    static_cast<double>(tf.c4));
        ImGui::Text("%8.3f %8.3f %8.3f %8.3f",
                    static_cast<double>(tf.d1),
                    static_cast<double>(tf.d2),
                    static_cast<double>(tf.d3),
                    static_cast<double>(tf.d4));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Meshes");
        ImGui::TableNextColumn();
        std::ostringstream oss;
        for (unsigned int i {0}; i < node->mNumMeshes; ++i)
        {
            oss << node->mMeshes[i] << ' ';
        }
        const auto str = oss.str();
        ImGui::TextUnformatted(str.c_str());

        for (unsigned int i {0}; i < node->mNumChildren; ++i)
        {
            display_node(node->mChildren[i]);
        }

        ImGui::TreePop();
    }
}

void scene_graph_table(const aiScene *scene)
{
    if (ImGui::BeginTable(
            "scene_graph",
            2,
            ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_NoBordersInBody))
    {
        ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_NoHide);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        // TODO: the nodes store a pointer to their parent, so we could actually
        // display them without recursion nor a stack
        display_node(scene->mRootNode);

        ImGui::EndTable();
    }
}

void material_properties_table(const aiScene *scene)
{
    if (ImGui::BeginTable(
            "properties",
            3,
            ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_NoBordersInBody))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
        ImGui::TableSetupColumn("Texture type/index");
        ImGui::TableSetupColumn("Data");
        ImGui::TableHeadersRow();

        for (unsigned int mat_i {0}; mat_i < scene->mNumMaterials; ++mat_i)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            const auto *const material = scene->mMaterials[mat_i];
            const auto name = material->GetName();
            const auto open = ImGui::TreeNode(name.C_Str());
            if (open)
            {
                for (unsigned int prop_i {0}; prop_i < material->mNumProperties;
                     ++prop_i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    const auto *const property = material->mProperties[prop_i];
                    ImGui::TextUnformatted(property->mKey.C_Str());
                    ImGui::TableNextColumn();
                    if (property->mSemantic != 0)
                    {
                        const auto type = aiTextureTypeToString(
                            static_cast<aiTextureType>(property->mSemantic));
                        ImGui::Text("%s, %u", type, property->mIndex);
                    }
                    ImGui::TableNextColumn();

                    switch (property->mType)
                    {
                    case aiPTI_Float: [[fallthrough]];
                    case aiPTI_Double:
                    {
                        // NOTE: this assumes there are no float/double
                        // properties with more than 4 components
                        float values[4] {};
                        unsigned int size {4};
                        const auto result =
                            material->Get(property->mKey.C_Str(),
                                          property->mSemantic,
                                          property->mIndex,
                                          values,
                                          &size);
                        if (result != aiReturn_SUCCESS)
                        {
                            throw std::runtime_error(
                                "Assimp material float property read failed");
                        }
                        switch (size)
                        {
                        case 1:
                            ImGui::Text("%f", static_cast<double>(values[0]));
                            break;
                        case 2:
                            ImGui::Text("%f %f",
                                        static_cast<double>(values[0]),
                                        static_cast<double>(values[1]));
                            break;
                        case 3:
                            ImGui::Text("%f %f %f",
                                        static_cast<double>(values[0]),
                                        static_cast<double>(values[1]),
                                        static_cast<double>(values[2]));
                            break;
                        case 4:
                            ImGui::Text("%f %f %f %f",
                                        static_cast<double>(values[0]),
                                        static_cast<double>(values[1]),
                                        static_cast<double>(values[2]),
                                        static_cast<double>(values[3]));
                            break;
                        default: break;
                        }
                        break;
                    }
                    case aiPTI_Integer:
                    {
                        // NOTE: this assumes there are no integer
                        // properties with more than 4 components
                        int values[4];
                        unsigned int size {4};
                        const auto result =
                            material->Get(property->mKey.C_Str(),
                                          property->mSemantic,
                                          property->mIndex,
                                          values,
                                          &size);
                        if (result != aiReturn_SUCCESS)
                        {
                            throw std::runtime_error(
                                "Assimp material integer property read failed");
                        }
                        switch (size)
                        {
                        case 1: ImGui::Text("%d", values[0]); break;
                        case 2:
                            ImGui::Text("%d %d", values[0], values[1]);
                            break;
                        case 3:
                            ImGui::Text(
                                "%d %d %d", values[0], values[1], values[2]);
                            break;
                        case 4:
                            ImGui::Text("%d %d %d %d",
                                        values[0],
                                        values[1],
                                        values[2],
                                        values[3]);
                            break;
                        default: break;
                        }
                        break;
                    }
                    case aiPTI_String:
                    {
                        aiString str;
                        const auto result =
                            material->Get(property->mKey.C_Str(),
                                          property->mSemantic,
                                          property->mIndex,
                                          str);
                        if (result != aiReturn_SUCCESS)
                        {
                            throw std::runtime_error(
                                "Assimp material string property read failed");
                        }
                        ImGui::TextUnformatted(str.C_Str());
                        break;
                    }
                    case aiPTI_Buffer:
                    {
                        if (property->mDataLength <= 4)
                        {
                            std::ostringstream oss;
                            oss << "0x";
                            for (unsigned int i {0}; i < property->mDataLength;
                                 ++i)
                            {
                                oss << std::hex << std::setw(2)
                                    << std::setfill('0')
                                    << static_cast<int>(property->mData[i]);
                            }
                            const auto str = oss.str();
                            ImGui::TextUnformatted(str.c_str());
                        }
                        else
                        {
                            ImGui::Text("%u byte buffer",
                                        property->mDataLength);
                        }
                        break;
                    }
                    default: break;
                    }
                }

                ImGui::TreePop();
            }
        }

        ImGui::EndTable();
    }
}

void scene_metadata_table(const aiScene *scene)
{
    if (ImGui::BeginTable(
            "scene_metadata",
            2,
            ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_NoBordersInBody))
    {
        ImGui::TableSetupColumn("Key");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        for (unsigned int i {0}; i < scene->mMetaData->mNumProperties; ++i)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            const auto key = scene->mMetaData->mKeys[i];
            ImGui::TextUnformatted(key.C_Str());
            ImGui::TableNextColumn();

            const auto value = scene->mMetaData->mValues[i];
            switch (value.mType)
            {
            case AI_BOOL:
                ImGui::TextUnformatted(
                    *static_cast<bool *>(value.mData) ? "true" : "false");
                break;
            case AI_INT32:
                ImGui::Text("%d", *static_cast<std::int32_t *>(value.mData));
                break;
            case AI_UINT64:
                ImGui::Text("%lu", *static_cast<std::uint64_t *>(value.mData));
                break;
            case AI_FLOAT:
                ImGui::Text(
                    "%f",
                    static_cast<double>(*static_cast<float *>(value.mData)));
                break;
            case AI_DOUBLE:
                ImGui::Text("%f", *static_cast<double *>(value.mData));
                break;
            case AI_AISTRING:
                ImGui::TextUnformatted(
                    static_cast<aiString *>(value.mData)->C_Str());
                break;
            case AI_AIVECTOR3D:
            {
                const auto *const v = static_cast<float *>(value.mData);
                ImGui::Text("%f %f %f",
                            static_cast<double>(v[0]),
                            static_cast<double>(v[1]),
                            static_cast<double>(v[2]));
            }
            break;
            case AI_AIMETADATA: ImGui::TextUnformatted("[metadata]"); break;
            case AI_INT64:
                ImGui::Text("%ld", *static_cast<std::int64_t *>(value.mData));
                break;
            case AI_UINT32:
                ImGui::Text("%u", *static_cast<std::uint32_t *>(value.mData));
                break;
            default: break;
            }
        }

        ImGui::EndTable();
    }
}

void make_ui(Application_state &state)
{
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

    if (ImGui::IsKeyDown(ImGuiMod_Shortcut))
    {
        if (ImGui::IsKeyDown(ImGuiKey_O))
        {
            open_scene_with_dialog(state);
        }
        else if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            close_scene(state);
        }
        else if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            save_as_png_with_dialog(state);
        }
    }

    auto popup_bg = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
    popup_bg.w = 1.0f;
    ImGui::PushStyleColor(ImGuiCol_PopupBg, popup_bg);
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "Ctrl+O"))
            {
                open_scene_with_dialog(state);
            }
            if (ImGui::MenuItem("Close", "Ctrl+W", false, state.scene_loaded))
            {
                close_scene(state);
            }
            if (ImGui::MenuItem(
                    "Save as PNG", "Ctrl+S", false, state.scene_loaded))
            {
                save_as_png_with_dialog(state);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleColor();

    mat4x4 view {};
    mat4x4 inverse_view {};
    bool need_to_reset {false};

    if (state.scene_loaded)
    {
        ImGui::SetNextWindowSize({640, 480}, ImGuiCond_FirstUseEver);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.0f, 0.0f, 0.0f, 1.0f});
        if (ImGui::Begin("Viewport"))
        {
            // Get position and size from the origin of the window content
            // region
            const auto cursor_pos = ImGui::GetCursorScreenPos();
            const auto region_size = ImGui::GetContentRegionAvail();

            centered_image(
                static_cast<ImTextureID>(
                    state.render_resources.final_render_descriptor_set.get()),
                static_cast<float>(state.render_width) /
                    static_cast<float>(state.render_height));

            ImGuizmo::SetDrawlist();
            static const auto view_manipulate_size = [&]
            {
                float y_scale {1.0f};
                glfwGetWindowContentScale(
                    state.window.get(), nullptr, &y_scale);
                return ImVec2 {128 * y_scale, 128 * y_scale};
            }();

            // FIXME: this back-and-forth between view and inverse view can
            // cause accumulating errors
            mat4x4 old_inverse_view {};
            const auto vx = state.camera.direction_x;
            const auto vy = -state.camera.direction_y;
            const auto vz = -state.camera.direction_z;
            old_inverse_view.m[0][0] = vx.x;
            old_inverse_view.m[0][1] = vx.y;
            old_inverse_view.m[0][2] = vx.z;
            old_inverse_view.m[0][3] = 0.0f;
            old_inverse_view.m[1][0] = vy.x;
            old_inverse_view.m[1][1] = vy.y;
            old_inverse_view.m[1][2] = vy.z;
            old_inverse_view.m[1][3] = 0.0f;
            old_inverse_view.m[2][0] = vz.x;
            old_inverse_view.m[2][1] = vz.y;
            old_inverse_view.m[2][2] = vz.z;
            old_inverse_view.m[2][3] = 0.0f;
            old_inverse_view.m[3][0] = state.camera.position.x;
            old_inverse_view.m[3][1] = state.camera.position.y;
            old_inverse_view.m[3][2] = state.camera.position.z;
            old_inverse_view.m[3][3] = 1.0f;

            const auto old_view = invert_rigid_transform(old_inverse_view);
            view = old_view;

            // FIXME: shouldn't we call the other version of ViewManipulate?
            ImGuizmo::ViewManipulate(
                &view.m[0][0],
                state.camera.distance,
                {cursor_pos.x + region_size.x - view_manipulate_size.x,
                 cursor_pos.y},
                view_manipulate_size,
                0x00000000);

            if (view != old_view)
            {
                need_to_reset = true;
            }

            inverse_view = invert_rigid_transform(view);
            const auto unit_direction_x =
                normalize(vec3 {inverse_view.m[0][0],
                                inverse_view.m[0][1],
                                inverse_view.m[0][2]});
            const auto unit_direction_y =
                normalize(vec3 {inverse_view.m[1][0],
                                inverse_view.m[1][1],
                                inverse_view.m[1][2]});
            const auto unit_direction_z =
                normalize(vec3 {inverse_view.m[2][0],
                                inverse_view.m[2][1],
                                inverse_view.m[2][2]});
            state.camera.direction_x =
                unit_direction_x * norm(state.camera.direction_x);
            state.camera.direction_y =
                -unit_direction_y * norm(state.camera.direction_y);
            state.camera.direction_z =
                -unit_direction_z * norm(state.camera.direction_z);
            state.camera.position = vec3 {inverse_view.m[3][0],
                                          inverse_view.m[3][1],
                                          inverse_view.m[3][2]};
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    if (ImGui::Begin("Parameters"))
    {
        ImGui::Text("%.3f ms/frame, %.2f fps",
                    1000.0 / static_cast<double>(ImGui::GetIO().Framerate),
                    static_cast<double>(ImGui::GetIO().Framerate));

        if (state.scene_loaded)
        {
            ImGui::Text(
                "Resolution: %u x %u", state.render_width, state.render_height);

            ImGui::Text("Samples: %u", state.render_resources.sample_count);

            auto samples_to_render =
                static_cast<int>(state.render_resources.samples_to_render);
            ImGui::InputInt("Total samples", &samples_to_render);
            state.render_resources.samples_to_render =
                static_cast<std::uint32_t>(std::max(samples_to_render, 1));

            auto samples_per_frame =
                static_cast<int>(state.render_resources.samples_per_frame);
            ImGui::InputInt("Samples per frame", &samples_per_frame, 1, 10);
            state.render_resources.samples_per_frame =
                static_cast<std::uint32_t>(std::max(samples_per_frame, 1));

            if (ImGui::Button("Reset render") ||
                state.render_resources.samples_to_render <
                    state.render_resources.sample_count)
            {
                need_to_reset = true;
            }

            ImGui::SeparatorText("Camera");
            static const float initial_camera_distance {state.camera.distance};
            static float camera_distance {initial_camera_distance};
            if (ImGui::SliderFloat("Distance",
                                   &camera_distance,
                                   0.0f,
                                   10.0f * initial_camera_distance))
            {
                camera_set_distance(state.camera, camera_distance);
                need_to_reset = true;
            }
            if (ImGui::SliderFloat("Focus distance",
                                   &state.camera.focus_distance,
                                   0.001f,
                                   10.0f * initial_camera_distance))
            {
                need_to_reset = true;
            }
            if (ImGui::SliderFloat("Aperture radius",
                                   &state.camera.aperture_radius,
                                   0.0f,
                                   0.3f * initial_camera_distance))
            {
                need_to_reset = true;
            }

            ImGui::SeparatorText("Camera");
            const auto dx = normalize(state.camera.direction_x);
            const auto dy = normalize(state.camera.direction_y);
            const auto dz = normalize(state.camera.direction_z);
            ImGui::Text("X: %6.2f %6.2f %6.2f   norm: %6.2f",
                        static_cast<double>(dx.x),
                        static_cast<double>(dx.y),
                        static_cast<double>(dx.z),
                        static_cast<double>(norm(state.camera.direction_x)));
            ImGui::Text("Y: %6.2f %6.2f %6.2f   norm: %6.2f",
                        static_cast<double>(dy.x),
                        static_cast<double>(dy.y),
                        static_cast<double>(dy.z),
                        static_cast<double>(norm(state.camera.direction_y)));
            ImGui::Text("Z: %6.2f %6.2f %6.2f   norm: %6.2f",
                        static_cast<double>(dz.x),
                        static_cast<double>(dz.y),
                        static_cast<double>(dz.z),
                        static_cast<double>(norm(state.camera.direction_z)));
            ImGui::Text("P: %6.2f %6.2f %6.2f",
                        static_cast<double>(state.camera.position.x),
                        static_cast<double>(state.camera.position.y),
                        static_cast<double>(state.camera.position.z));

            ImGui::SeparatorText("View");
            ImGui::Text("%6.2f %6.2f %6.2f %6.2f",
                        static_cast<double>(view.m[0][0]),
                        static_cast<double>(view.m[1][0]),
                        static_cast<double>(view.m[2][0]),
                        static_cast<double>(view.m[3][0]));
            ImGui::Text("%6.2f %6.2f %6.2f %6.2f",
                        static_cast<double>(view.m[0][1]),
                        static_cast<double>(view.m[1][1]),
                        static_cast<double>(view.m[2][1]),
                        static_cast<double>(view.m[3][1]));
            ImGui::Text("%6.2f %6.2f %6.2f %6.2f",
                        static_cast<double>(view.m[0][2]),
                        static_cast<double>(view.m[1][2]),
                        static_cast<double>(view.m[2][2]),
                        static_cast<double>(view.m[3][2]));
            ImGui::Text("%6.2f %6.2f %6.2f %6.2f",
                        static_cast<double>(view.m[0][3]),
                        static_cast<double>(view.m[1][3]),
                        static_cast<double>(view.m[2][3]),
                        static_cast<double>(view.m[3][3]));

            ImGui::SeparatorText("Inverse view");
            ImGui::Text("%6.2f %6.2f %6.2f %6.2f",
                        static_cast<double>(inverse_view.m[0][0]),
                        static_cast<double>(inverse_view.m[1][0]),
                        static_cast<double>(inverse_view.m[2][0]),
                        static_cast<double>(inverse_view.m[3][0]));
            ImGui::Text("%6.2f %6.2f %6.2f %6.2f",
                        static_cast<double>(inverse_view.m[0][1]),
                        static_cast<double>(inverse_view.m[1][1]),
                        static_cast<double>(inverse_view.m[2][1]),
                        static_cast<double>(inverse_view.m[3][1]));
            ImGui::Text("%6.2f %6.2f %6.2f %6.2f",
                        static_cast<double>(inverse_view.m[0][2]),
                        static_cast<double>(inverse_view.m[1][2]),
                        static_cast<double>(inverse_view.m[2][2]),
                        static_cast<double>(inverse_view.m[3][2]));
            ImGui::Text("%6.2f %6.2f %6.2f %6.2f",
                        static_cast<double>(inverse_view.m[0][3]),
                        static_cast<double>(inverse_view.m[1][3]),
                        static_cast<double>(inverse_view.m[2][3]),
                        static_cast<double>(inverse_view.m[3][3]));

            if (need_to_reset)
            {
                reset_render(state.render_resources);
            }
        }
    }
    ImGui::End();

    if (state.scene_loaded)
    {
        if (ImGui::Begin("Scene"))
        {
            if (ImGui::TreeNode("Graph"))
            {
                scene_graph_table(state.scene);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Materials"))
            {
                material_properties_table(state.scene);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Metadata"))
            {
                scene_metadata_table(state.scene);
                ImGui::TreePop();
            }
        }
        ImGui::End();
    }
}

} // namespace

void run(const char *file_name)
{
    Application_state state {};

    state.window.reset(glfw_init());

    state.imgui_context.reset(imgui_init(state.window.get()));

    // TODO: right now, we first create the window then try creating the Vulkan
    // context, but if we fail to create the context, the window will just flash
    // and close immediately. It would be best to first create the Vulkan
    // instance and device, and only then create the window (which we will need
    // for creating the surface).
    state.context = create_context(state.window.get());

    Assimp::DefaultLogger::create();
    Assimp::DefaultLogger::get()->setLogSeverity(Assimp::Logger::VERBOSE);
    const auto stream =
        Assimp::LogStream::createDefaultStream(aiDefaultLogStream_STDOUT);
    constexpr auto severity = Assimp::Logger::Debugging | Assimp::Logger::Info |
                              Assimp::Logger::Err | Assimp::Logger::Warn;
    Assimp::DefaultLogger::get()->attachStream(stream, severity);

    try
    {
        if (file_name != nullptr)
        {
            open_scene(state, file_name);
        }

        glfwSetWindowUserPointer(state.window.get(), &state);

        while (!glfwWindowShouldClose(state.window.get()))
        {
            glfwPollEvents();

            ImGui_ImplGlfw_NewFrame();
            ImGui_ImplVulkan_NewFrame();
            ImGui::NewFrame();
            ImGuizmo::BeginFrame();

            make_ui(state);

            ImGui::Render();

            draw_frame(state.context, state.render_resources, state.camera);
        }

        state.context.device->waitIdle();

        // FIXME: this won't be executed when an exception is thrown
        Assimp::DefaultLogger::kill();
    }
    catch (...)
    {
        // FIXME: find the best way to handle this to be as exception safe as
        // possible, though that might not be actually possible...
        // What if we get a device lost error? In that case we can not
        // wait
        state.context.device->waitIdle();

        throw; // FIXME: it makes no sense to throw again just to catch the
               // exception in main immediately
    }
}
