#ifndef PATH_TRACER_HPP
#define PATH_TRACER_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

struct Queue_family_indices
{
    std::uint32_t graphics;
    std::uint32_t present;
};

struct Swapchain
{
    vk::UniqueSwapchainKHR swapchain;
    vk::Format format;
    vk::Extent2D extent;
    std::uint32_t min_image_count;
};

struct Image
{
    vk::UniqueImage image;
    vk::UniqueImageView view;
    vk::UniqueDeviceMemory memory;
};

struct Buffer
{
    vk::UniqueBuffer buffer;
    vk::UniqueDeviceMemory memory;
};

struct Push_constants
{
    std::uint32_t resolution_width;
    std::uint32_t resolution_height;
    std::uint32_t frame;
};

constexpr std::uint32_t frames_in_flight {3};

class Renderer
{
public:
    [[nodiscard]] Renderer();
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    Renderer(Renderer &&) = default;
    Renderer &operator=(Renderer &&) = default;

    void run();

private:
    static void key_callback(
        GLFWwindow *window, int key, int scancode, int action, int mods);
    static void
    framebuffer_size_callback(GLFWwindow *window, int width, int height);

    [[nodiscard]] bool is_fullscreen();
    void set_fullscreen();
    void set_windowed();

    void create_renderer(GLFWwindow *window,
                         std::uint32_t render_width,
                         std::uint32_t render_height);

    void init_imgui();
    static void shutdown_imgui();

    void wait();

    void resize_framebuffer(std::uint32_t framebuffer_width,
                            std::uint32_t framebuffer_height);

    void begin_frame();
    void draw_frame();

    void recreate_swapchain();

    GLFWwindow *m_window {};
    vk::UniqueInstance m_instance {};
#ifdef ENABLE_VALIDATION_LAYERS
    vk::UniqueDebugUtilsMessengerEXT m_debug_messenger {};
#endif
    vk::UniqueSurfaceKHR m_surface {};
    vk::PhysicalDevice m_physical_device {};
    Queue_family_indices m_queue_family_indices {};
    vk::UniqueDevice m_device {};
    vk::Queue m_graphics_queue {};
    vk::Queue m_present_queue {};
    std::uint32_t m_framebuffer_width {};
    std::uint32_t m_framebuffer_height {};
    Swapchain m_swapchain {};
    std::vector<vk::Image> m_swapchain_images {};
    std::vector<vk::UniqueImageView> m_swapchain_image_views {};
    vk::UniqueSampler m_sampler {};
    vk::UniqueDescriptorPool m_descriptor_pool {};
    vk::UniqueDescriptorPool m_imgui_descriptor_pool {};
    vk::UniqueCommandPool m_command_pool {};
    std::uint32_t m_offscreen_width {};
    std::uint32_t m_offscreen_height {};
    Image m_storage_image {};
    vk::DescriptorSet m_offscreen_descriptor_set {};
    vk::UniqueRenderPass m_render_pass {};
    vk::UniqueDescriptorSetLayout m_descriptor_set_layout {};
    vk::UniquePipelineLayout m_pipeline_layout {};
    vk::UniquePipeline m_pipeline {};
    std::vector<vk::UniqueFramebuffer> m_framebuffers {};
    std::array<vk::DescriptorSet, frames_in_flight> m_descriptor_sets {};
    std::array<vk::UniqueCommandBuffer, frames_in_flight>
        m_draw_command_buffers {};
    std::array<vk::UniqueSemaphore, frames_in_flight>
        m_image_available_semaphores {};
    std::array<vk::UniqueSemaphore, frames_in_flight>
        m_render_finished_semaphores {};
    std::array<vk::UniqueFence, frames_in_flight> m_in_flight_fences {};
    std::uint32_t m_current_frame {};
    bool m_framebuffer_resized {};
};

#endif // PATH_TRACER_HPP
