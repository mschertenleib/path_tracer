#include "geometry.hpp"
#include "renderer.hpp"
#include "utility.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <chrono>
#include <iostream>
#include <numbers>
#include <stdexcept>

#include <cmath>
#include <cstdlib>

namespace
{

[[nodiscard]] bool is_fullscreen(GLFWwindow *window)
{
    return glfwGetWindowMonitor(window) != nullptr;
}

void set_fullscreen(GLFWwindow *window)
{
    // TODO: ideally use the current monitor, not the primary one
    const auto monitor = glfwGetPrimaryMonitor();
    const auto video_mode = glfwGetVideoMode(monitor);
    glfwSetWindowMonitor(window,
                         monitor,
                         0,
                         0,
                         video_mode->width,
                         video_mode->height,
                         video_mode->refreshRate);
}

void set_windowed(GLFWwindow *window)
{
    // TODO: use previous windowed size, scale must be taken into account
    const auto monitor = glfwGetPrimaryMonitor();
    const auto video_mode = glfwGetVideoMode(monitor);
    constexpr int width {1280};
    constexpr int height {720};
    glfwSetWindowMonitor(window,
                         nullptr,
                         (video_mode->width - width) / 2,
                         (video_mode->height - height) / 2,
                         width,
                         height,
                         GLFW_DONT_CARE);
}

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

void glfw_framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    const auto context =
        static_cast<Vulkan_context *>(glfwGetWindowUserPointer(window));
    resize_framebuffer(*context,
                       static_cast<std::uint32_t>(width),
                       static_cast<std::uint32_t>(height));
}

void glfw_key_callback(GLFWwindow *window,
                       int key,
                       [[maybe_unused]] int scancode,
                       int action,
                       [[maybe_unused]] int mods)

{
    if (action == GLFW_PRESS && key == GLFW_KEY_F)
    {
        if (is_fullscreen(window))
        {
            set_windowed(window);
        }
        else
        {
            set_fullscreen(window);
        }
    }
}

class Timer
{
public:
    inline void start(const char *name) noexcept
    {
        m_name = name;
        m_start = std::chrono::steady_clock::now();
    }
    inline void stop()
    {
        const auto stop = std::chrono::steady_clock::now();
        std::cout
            << m_name << ": "
            << std::chrono::duration<double, std::milli>(stop - m_start).count()
            << " ms\n";
    }

private:
    const char *m_name {""};
    std::chrono::steady_clock::time_point m_start {};
};

} // namespace

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

        glfwSetErrorCallback(&glfw_error_callback);

        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        SCOPE_EXIT([] { glfwTerminate(); });

        if (!glfwVulkanSupported())
        {
            throw std::runtime_error(
                "Vulkan loader or ICD have not been found");
        }

        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        const auto window =
            glfwCreateWindow(1280, 720, "Path tracer", nullptr, nullptr);
        if (!window)
        {
            throw std::runtime_error("Failed to create GLFW window");
        }
        SCOPE_EXIT([window] { glfwDestroyWindow(window); });

        glfwSetKeyCallback(window, &glfw_key_callback);
        glfwSetFramebufferSizeCallback(window, &glfw_framebuffer_size_callback);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        SCOPE_EXIT([] { ImGui::DestroyContext(); });

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
            return EXIT_FAILURE;
        }
        SCOPE_EXIT([] { ImGui_ImplGlfw_Shutdown(); });

        Timer t;

        t.start("create_vulkan_context");
        auto context = create_vulkan_context(window);
        SCOPE_EXIT([&] { destroy_vulkan_context(context); });
        t.stop();

        t.start("load_obj");
        const auto geometry = load_obj(obj_file_name);
        t.stop();

        Camera camera {};
        if (std::string(obj_file_name).ends_with("cornell_box.obj"))
        {
            const vec3 position {278.0f, 273.0f, -800.0f};
            const vec3 look_at {278.0f, 273.0f, 0.0f};
            const vec3 world_up {0.0f, 1.0f, 0.0f};
            const auto aspect_ratio = static_cast<float>(render_width) /
                                      static_cast<float>(render_height);
            const float focal_length {0.035f};
            const float sensor_height {0.025f};
            const auto sensor_width = aspect_ratio * sensor_height;
            camera = create_camera(position,
                                   look_at,
                                   world_up,
                                   focal_length,
                                   sensor_width,
                                   sensor_height);
        }
        else
        {
            const vec3 position {-0.07f, 0.15f, 0.2f};
            const vec3 look_at {-0.02f, 0.12f, 0.0f};
            const vec3 world_up {0.0f, 1.0f, 0.0f};
            const auto vertical_fov =
                45.0f / 180.0f * std::numbers::pi_v<float>;
            const auto aspect_ratio = static_cast<float>(render_width) /
                                      static_cast<float>(render_height);
            const float focal_length {1.0f};
            const auto sensor_height = 2.0f * std::tan(vertical_fov * 0.5f);
            const auto sensor_width = aspect_ratio * sensor_height;
            camera = create_camera(position,
                                   look_at,
                                   world_up,
                                   focal_length,
                                   sensor_width,
                                   sensor_height);
        }

        t.start("load_scene");
        load_scene(context, render_width, render_height, geometry);
        SCOPE_EXIT([&] { destroy_scene_resources(context); });
        t.stop();

        glfwSetWindowUserPointer(window, &context);

        char filename_buffer[1024] {"image.png"};

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            ImGui_ImplGlfw_NewFrame();
            ImGui_ImplVulkan_NewFrame();
            ImGui::NewFrame();

            ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

            ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.0f, 0.0f, 0.0f, 1.0f});
            if (ImGui::Begin("Viewport"))
            {
                const auto region_size = ImGui::GetContentRegionAvail();
                if (region_size.x > 0.0f && region_size.y > 0.0f)
                {
                    const auto image_aspect_ratio =
                        static_cast<float>(render_width) /
                        static_cast<float>(render_height);
                    const auto region_aspect_ratio =
                        region_size.x / region_size.y;
                    const auto cursor_pos = ImGui::GetCursorPos();
                    auto width = region_size.x;
                    auto height = region_size.y;
                    auto x = cursor_pos.x;
                    auto y = cursor_pos.y;
                    if (image_aspect_ratio >= region_aspect_ratio)
                    {
                        height = width / image_aspect_ratio;
                        y += (region_size.y - height) * 0.5f;
                    }
                    else
                    {
                        width = height * image_aspect_ratio;
                        x += (region_size.x - width) * 0.5f;
                    }
                    ImGui::SetCursorPos({x, y});
                    ImGui::Image(static_cast<ImTextureID>(
                                     context.final_render_descriptor_set),
                                 {width, height});
                }
            }
            ImGui::End();
            ImGui::PopStyleColor();

            if (ImGui::Begin("Parameters"))
            {
                ImGui::Text("%.2f ms/frame, %.1f fps",
                            1000.0 /
                                static_cast<double>(ImGui::GetIO().Framerate),
                            static_cast<double>(ImGui::GetIO().Framerate));

                ImGui::Text("Press [F] to toggle fullscreen");

                ImGui::Text("Resolution: %u x %u", render_width, render_height);

                ImGui::Text("Samples: %u", context.sample_count);

                auto samples_to_render =
                    static_cast<int>(context.samples_to_render);
                ImGui::InputInt("Total samples", &samples_to_render);
                context.samples_to_render =
                    static_cast<std::uint32_t>(std::max(samples_to_render, 1));

                auto samples_per_frame =
                    static_cast<int>(context.samples_per_frame);
                ImGui::InputInt("Samples per frame", &samples_per_frame, 1, 10);
                context.samples_per_frame =
                    static_cast<std::uint32_t>(std::max(samples_per_frame, 1));

                if (ImGui::Button("Reset render") ||
                    context.samples_to_render < context.sample_count)
                {
                    reset_render(context);
                }

                ImGui::InputText(
                    "File name", filename_buffer, sizeof(filename_buffer) - 1);
                if (ImGui::Button("Write to PNG"))
                {
                    t.start("write_to_png");
                    write_to_png(context, filename_buffer);
                    t.stop();
                }
            }
            ImGui::End();

            ImGui::Render();

            draw_frame(context, camera);
        }

        wait_idle(context);

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
