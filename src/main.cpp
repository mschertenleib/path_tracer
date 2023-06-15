#include "vulkan_renderer.hpp"

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

#include <cstdlib>
#include <iostream>

namespace
{

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

void glfw_framebuffer_size_callback([[maybe_unused]] GLFWwindow *window,
                                    [[maybe_unused]] int width,
                                    [[maybe_unused]] int height)
{
    const auto renderer =
        static_cast<Vulkan_renderer *>(glfwGetWindowUserPointer(window));
    renderer->resize_framebuffer();
}

} // namespace

int main()
{
    try
    {
        glfwSetErrorCallback(glfw_error_callback);

        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        if (!glfwVulkanSupported())
        {
            throw std::runtime_error(
                "Vulkan loader or ICD have not been found");
        }

        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        const auto window =
            glfwCreateWindow(1280, 720, "Path Tracer", nullptr, nullptr);
        if (!window)
        {
            throw std::runtime_error("Failed to create GLFW window");
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(window, true);

        {
            int framebuffer_width {};
            int framebuffer_height {};
            glfwGetFramebufferSize(
                window, &framebuffer_width, &framebuffer_height);

            constexpr std::uint32_t render_width {160};
            constexpr std::uint32_t render_height {90};

            Vulkan_renderer renderer(
                window,
                static_cast<std::uint32_t>(framebuffer_width),
                static_cast<std::uint32_t>(framebuffer_height),
                render_width,
                render_height);

            glfwSetWindowUserPointer(window, &renderer);
            glfwSetFramebufferSizeCallback(window,
                                           glfw_framebuffer_size_callback);

            char input_text_buffer[256] {"image.png"};

            std::uint32_t rng_seed {1};

            while (!glfwWindowShouldClose(window))
            {
                glfwPollEvents();

                ImGui_ImplGlfw_NewFrame();
                ImGui_ImplVulkan_NewFrame();
                ImGui::NewFrame();

                ImGui::SetNextWindowPos({0.0f, 0.0f});
                ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_WindowBg,
                                      {0.0f, 0.0f, 0.0f, 0.0f});
                if (ImGui::Begin("Viewport",
                                 nullptr,
                                 ImGuiWindowFlags_NoDecoration |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus))
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
                        ImGui::Image(
                            static_cast<ImTextureID>(
                                renderer.get_final_render_descriptor_set()),
                            {width, height});
                    }
                }
                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();

                if (ImGui::Begin("Settings"))
                {
                    ImGui::Text(
                        "%.2f ms/frame, %.1f fps",
                        1000.0 / static_cast<double>(ImGui::GetIO().Framerate),
                        static_cast<double>(ImGui::GetIO().Framerate));

                    const auto budgets = renderer.get_heap_budgets();
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

                    ImGui::InputText("PNG file name",
                                     input_text_buffer,
                                     sizeof(input_text_buffer));

                    if (ImGui::Button("Store to PNG"))
                    {
                        renderer.store_to_png(input_text_buffer);
                    }

                    if (ImGui::Button("Change RNG seed"))
                    {
                        rng_seed += render_width * render_height;
                    }
                }
                ImGui::End();

                ImGui::Render();

                renderer.draw_frame(rng_seed);
            }
        }

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
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
