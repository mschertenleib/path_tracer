#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "vulkan_renderer.hpp"

class Unique_window
{
public:
    constexpr Unique_window() noexcept = default;

    Unique_window(int width, int height, const char *title);

    ~Unique_window() noexcept;

    Unique_window(Unique_window &&rhs) noexcept;
    Unique_window &operator=(Unique_window &&rhs) noexcept;

    Unique_window(const Unique_window &) = delete;
    Unique_window &operator=(const Unique_window &) = delete;

    [[nodiscard]] constexpr struct GLFWwindow *get() const noexcept
    {
        return m_window;
    }

    [[nodiscard]] bool is_fullscreen();
    void set_fullscreen();
    void set_windowed();

private:
    struct GLFWwindow *m_window {};
};

class Application
{
public:
    Application();

    Application(Application &&) noexcept = default;
    Application &operator=(Application &&) noexcept = default;

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    void run();

private:
    static void glfw_key_callback(
        struct GLFWwindow *window, int key, int scancode, int action, int mods);
    static void glfw_framebuffer_size_callback(struct GLFWwindow *window,
                                               int width,
                                               int height);

    Unique_window m_window {};
    std::uint32_t m_render_width {};
    std::uint32_t m_render_height {};
    Vulkan_renderer m_renderer {};
};

#endif // APPLICATION_HPP
