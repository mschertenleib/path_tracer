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
#include <stdexcept>

#include <cstdlib>

namespace
{

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
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
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " <model.obj> <output.png>\n";
            return EXIT_FAILURE;
        }

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

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        SCOPE_EXIT([] { ImGui::DestroyContext(); });

        auto &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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
        const auto geometry = load_obj(argv[1]);
        t.stop();

        t.start("load_scene");
        load_scene(context, 1280, 720, geometry);
        SCOPE_EXIT([&] { destroy_scene_resources(context); });
        t.stop();

        t.start("render");
        render(context);
        t.stop();

        t.start("write_to_png");
        write_to_png(context, argv[2]);
        t.stop();

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
