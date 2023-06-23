#include "application.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include "imgui_impl_vulkan.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

namespace
{

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

void glfw_framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    const auto renderer =
        static_cast<Vulkan_renderer *>(glfwGetWindowUserPointer(window));
    renderer->resize_framebuffer(static_cast<std::uint32_t>(width),
                                 static_cast<std::uint32_t>(height));
}

} // namespace

Unique_window::Unique_window(int width, int height, const char *title)
{
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    if (!glfwVulkanSupported())
    {
        throw std::runtime_error("Vulkan loader or ICD have not been found");
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForVulkan(m_window, true))
    {
        ImGui::DestroyContext();
        glfwDestroyWindow(m_window);
        glfwTerminate();
        throw std::runtime_error(
            "Failed to initialize ImGui for GLFW and Vulkan");
    }
}

Unique_window::~Unique_window() noexcept
{
    if (m_window)
    {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

Unique_window::Unique_window(Unique_window &&rhs) noexcept
    : m_window {rhs.m_window}
{
    rhs.m_window = nullptr;
}

Unique_window &Unique_window::operator=(Unique_window &&rhs) noexcept
{
    const auto old_window = m_window;
    m_window = rhs.m_window;
    rhs.m_window = nullptr;
    if (old_window)
    {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(old_window);
        glfwTerminate();
    }
    return *this;
}

Application::Application()
{
    m_window = Unique_window(1280, 720, "Path Tracer");

    int framebuffer_width {};
    int framebuffer_height {};
    glfwGetFramebufferSize(
        m_window.get(), &framebuffer_width, &framebuffer_height);

    m_render_width = 1600;
    m_render_height = 900;

    m_renderer = Vulkan_renderer(m_window.get(),
                                 static_cast<std::uint32_t>(framebuffer_width),
                                 static_cast<std::uint32_t>(framebuffer_height),
                                 m_render_width,
                                 m_render_height);

    glfwSetWindowUserPointer(m_window.get(), &m_renderer);
    glfwSetFramebufferSizeCallback(m_window.get(),
                                   glfw_framebuffer_size_callback);
}

void Application::run()
{
    char input_text_buffer[256] {"image.png"};

    std::uint32_t rng_seed {1};

    while (!glfwWindowShouldClose(m_window.get()))
    {
        glfwPollEvents();

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0.0f, 0.0f});
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.0f, 0.0f, 0.0f, 0.0f});
        if (ImGui::Begin("Viewport",
                         nullptr,
                         ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoBringToFrontOnFocus))
        {
            const auto region_size = ImGui::GetContentRegionAvail();
            if (region_size.x > 0.0f && region_size.y > 0.0f)
            {
                const auto image_aspect_ratio =
                    static_cast<float>(m_render_width) /
                    static_cast<float>(m_render_height);
                const auto region_aspect_ratio = region_size.x / region_size.y;
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
                                 m_renderer.get_final_render_descriptor_set()),
                             {width, height});
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (ImGui::Begin("Settings"))
        {
            ImGui::Text("%.2f ms/frame, %.1f fps",
                        1000.0 / static_cast<double>(ImGui::GetIO().Framerate),
                        static_cast<double>(ImGui::GetIO().Framerate));

            const auto budgets = m_renderer.get_heap_budgets();
            ImGui::Text("Memory heaps:");
            for (std::size_t i {}; i < budgets.size(); ++i)
            {
                if (budgets[i].budget > 0)
                {
                    ImGui::Text("  [%llu]: %llu MB / %llu MB",
                                i,
                                budgets[i].usage / 1'000'000u,
                                budgets[i].budget / 1'000'000u);
                }
            }

            ImGui::InputText("##", input_text_buffer, sizeof(input_text_buffer));
            ImGui::SameLine();
            if (ImGui::Button("Store to PNG"))
            {
                m_renderer.store_to_png(input_text_buffer);
            }

            if (ImGui::Button("Change RNG seed"))
            {
                rng_seed += m_render_width * m_render_height;
            }
        }
        ImGui::End();

        ImGui::Render();

        m_renderer.draw_frame(rng_seed);
    }
}
