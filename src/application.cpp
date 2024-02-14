#include "application.hpp"
#include "camera.hpp"
#include "renderer.hpp"
#include "utility.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <tinyfiledialogs.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <ranges>
#include <stdexcept>

namespace
{

struct Application_state
{
    Vulkan_context context;
    Vulkan_render_resources render_resources;
    bool scene_loaded;
    Camera camera;
    std::uint32_t render_width;
    std::uint32_t render_height;
};

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

void glfw_scroll_callback(GLFWwindow *window,
                          [[maybe_unused]] double xoffset,
                          double yoffset)
{
}

[[nodiscard]] GLFWwindow *init_glfw()
{
    glfwSetErrorCallback(&glfw_error_callback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    SCOPE_FAIL([] { glfwTerminate(); });

    if (!glfwVulkanSupported())
    {
        throw std::runtime_error("Vulkan loader or ICD have not been found");
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const auto window =
        glfwCreateWindow(1280, 720, "Path Tracer", nullptr, nullptr);
    if (!window)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetFramebufferSizeCallback(window, &glfw_framebuffer_size_callback);
    glfwSetScrollCallback(window, &glfw_scroll_callback);

    return window;
}

void shutdown_glfw(GLFWwindow *window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

void init_imgui(GLFWwindow *window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    SCOPE_FAIL([] { ImGui::DestroyContext(); });

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    float y_scale {1.0f};
    glfwGetWindowContentScale(window, nullptr, &y_scale);
    ImFontConfig font_config {};
    font_config.SizePixels = 13.0f * y_scale;
    io.Fonts->AddFontDefault(&font_config);

    if (!ImGui_ImplGlfw_InitForVulkan(window, true))
    {
        throw std::runtime_error("Failed to initialize ImGui GLFW backend");
    }
}

void shutdown_imgui()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void open_scene(Application_state &state, const char *file_name)
{
    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_PP_PTV_NORMALIZE, true);

    const auto *const scene = importer.ReadFile(
        file_name,
        static_cast<unsigned int>(
            aiProcess_Triangulate | aiProcess_PreTransformVertices |
            aiProcess_GenBoundingBoxes | aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType));
    if (scene == nullptr)
    {
        // Messages can not contain quotes
        std::string importer_error {importer.GetErrorString()};
        auto filtered_message = std::views::filter(
            importer_error, [](auto c) { return c != '\'' && c != '\"'; });
        importer_error.assign(filtered_message.begin(), filtered_message.end());
        tinyfd_messageBox("Error", importer_error.c_str(), "ok", "error", 1);
        return;
    }

    if (!scene->HasMeshes())
    {
        tinyfd_messageBox("Error", "Scene has no meshes", "ok", "error", 1);
        return;
    }

    state.render_width = 640;
    state.render_height = 480;

    const vec3 position {0.0f, 0.0f, 3.5f};
    const vec3 target {0.0f, 0.0f, 0.0f};
    const auto vertical_fov = 45.0f / 180.0f * std::numbers::pi_v<float>;
    const auto aspect_ratio = static_cast<float>(state.render_width) /
                              static_cast<float>(state.render_height);
    const float focal_length {1.0f};
    const auto sensor_height =
        2.0f * std::tan(vertical_fov * 0.5f) * focal_length;
    const auto sensor_width = aspect_ratio * sensor_height;
    state.camera = create_camera(
        position, target, focal_length, sensor_width, sensor_height);

    wait_idle(state.context);
    if (state.scene_loaded)
    {
        destroy_render_resources(state.context, state.render_resources);
    }
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
        wait_idle(state.context);
        destroy_render_resources(state.context, state.render_resources);
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
            // Messages can not contain quotes
            auto filtered_message = std::views::filter(
                error_message, [](auto c) { return c != '\'' && c != '\"'; });
            error_message.assign(filtered_message.begin(),
                                 filtered_message.end());
            tinyfd_messageBox("Error", error_message.c_str(), "ok", "error", 1);
        }
    }
}

void make_centered_image(ImTextureID texture_id, float aspect_ratio)
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

void make_ui(Application_state &state)
{
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", nullptr))
            {
                open_scene_with_dialog(state);
            }
            if (ImGui::MenuItem("Close", nullptr, false, state.scene_loaded))
            {
                close_scene(state);
            }
            if (ImGui::MenuItem(
                    "Save as PNG", nullptr, false, state.scene_loaded))
            {
                save_as_png_with_dialog(state);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (state.scene_loaded)
    {
        ImGui::SetNextWindowSize({640, 480}, ImGuiCond_FirstUseEver);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.0f, 0.0f, 0.0f, 1.0f});
        if (ImGui::Begin("Viewport"))
        {
            make_centered_image(
                static_cast<ImTextureID>(
                    state.render_resources.final_render_descriptor_set),
                static_cast<float>(state.render_width) /
                    static_cast<float>(state.render_height));
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    if (ImGui::Begin("Parameters"))
    {
        ImGui::Text("%.2f ms/frame, %.1f fps",
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
                reset_render(state.render_resources);
            }

            ImGui::SeparatorText("Orbital Camera");
            static const float initial_camera_distance {
                norm(state.camera.target - state.camera.position)};
            static float camera_distance {initial_camera_distance};
            if (ImGui::SliderFloat("Distance",
                                   &camera_distance,
                                   0.0f,
                                   10.0f * initial_camera_distance))
            {
                orbital_camera_set_distance(state.camera, camera_distance);
                reset_render(state.render_resources);
            }
            static float camera_yaw {state.camera.yaw};
            if (ImGui::SliderAngle("Yaw", &camera_yaw))
            {
                orbital_camera_set_yaw(state.camera, camera_yaw);
                reset_render(state.render_resources);
            }
            static float camera_pitch {state.camera.pitch};
            if (ImGui::SliderAngle("Pitch", &camera_pitch, -90.0f, 90.0f))
            {
                orbital_camera_set_pitch(state.camera, camera_pitch);
                reset_render(state.render_resources);
            }
        }
    }
    ImGui::End();
}

} // namespace

void run(const char *file_name)
{
    const auto window = init_glfw();
    SCOPE_EXIT([window] { shutdown_glfw(window); });

    init_imgui(window);
    SCOPE_EXIT([] { shutdown_imgui(); });

    Application_state state {};

    state.context = create_context(window);
    SCOPE_EXIT([&] { destroy_context(state.context); });

    if (file_name != nullptr)
    {
        open_scene(state, file_name);
    }
    SCOPE_EXIT(
        [&]
        {
            if (state.scene_loaded)
            {
                destroy_render_resources(state.context, state.render_resources);
            }
        });

    glfwSetWindowUserPointer(window, &state);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();

        make_ui(state);

        ImGui::Render();

        draw_frame(state.context, state.render_resources, state.camera);
    }

    wait_idle(state.context);
}
