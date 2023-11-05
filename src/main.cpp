#include "geometry.hpp"
#include "renderer.hpp"
#include "utility.hpp"

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
