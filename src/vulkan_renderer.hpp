#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#include "vk_mem_alloc.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

class Unique_allocator
{
public:
    constexpr Unique_allocator() noexcept = default;

    explicit Unique_allocator(VmaAllocator allocator) noexcept;

    ~Unique_allocator() noexcept;

    Unique_allocator(Unique_allocator &&rhs) noexcept;
    Unique_allocator &operator=(Unique_allocator &&rhs) noexcept;

    Unique_allocator(const Unique_allocator &) = delete;
    Unique_allocator &operator=(const Unique_allocator &) = delete;

    [[nodiscard]] constexpr const VmaAllocator &get() const noexcept
    {
        return m_allocator;
    }

private:
    VmaAllocator m_allocator {VK_NULL_HANDLE};
};

class Unique_allocation
{
public:
    constexpr Unique_allocation() noexcept = default;

    Unique_allocation(VmaAllocation allocation,
                      VmaAllocator allocator) noexcept;

    ~Unique_allocation() noexcept;

    Unique_allocation(Unique_allocation &&rhs) noexcept;
    Unique_allocation &operator=(Unique_allocation &&rhs) noexcept;

    Unique_allocation(const Unique_allocation &) = delete;
    Unique_allocation &operator=(const Unique_allocation &) = delete;

    [[nodiscard]] constexpr const VmaAllocation &get() const noexcept
    {
        return m_allocation;
    }

private:
    VmaAllocation m_allocation {VK_NULL_HANDLE};
    VmaAllocator m_allocator {VK_NULL_HANDLE};
};

struct Queue_family_indices
{
    std::uint32_t graphics_compute;
    std::uint32_t present;
};

class Vulkan_renderer
{
public:
    constexpr Vulkan_renderer() noexcept = default;

    Vulkan_renderer(struct GLFWwindow *window,
                    std::uint32_t framebuffer_width,
                    std::uint32_t framebuffer_height,
                    std::uint32_t render_width,
                    std::uint32_t render_height);

    ~Vulkan_renderer();

    Vulkan_renderer(const Vulkan_renderer &) = delete;
    Vulkan_renderer(Vulkan_renderer &&) noexcept = default;

    Vulkan_renderer &operator=(const Vulkan_renderer &) = delete;
    Vulkan_renderer &operator=(Vulkan_renderer &&) noexcept = default;

    void draw_frame(std::uint32_t rng_seed);
    void store_to_png(const char *file_name);

private:
    void create_instance();
    void create_surface(struct GLFWwindow *window);
    void select_physical_device(std::uint32_t device_extension_count,
                                const char *const *device_extension_names);
    void create_device(std::uint32_t device_extension_count,
                       const char *const *device_extension_names);
    void create_allocator();
    void create_swapchain();

    vk::UniqueInstance m_instance {};
#ifndef NDEBUG
    vk::UniqueDebugUtilsMessengerEXT m_debug_messenger {};
#endif
    vk::UniqueSurfaceKHR m_surface {};
    Queue_family_indices m_queue_family_indices {
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint32_t>::max()};
    vk::PhysicalDevice m_physical_device {};
    vk::UniqueDevice m_device {};
    Unique_allocator m_allocator {};
    vk::Queue m_graphics_compute_queue {};
    vk::Queue m_present_queue {};
    vk::UniqueSwapchainKHR m_swapchain {};
    vk::Format m_swapchain_format {vk::Format::eUndefined};
    vk::Extent2D m_swapchain_extent {};
    std::uint32_t m_swapchain_image_count {};
    std::uint32_t m_swapchain_min_image_count {};
    std::vector<vk::Image> m_swapchain_images {};
    std::vector<vk::UniqueImageView> m_swapchain_image_views {};
    vk::UniqueImage m_storage_image {};
    vk::UniqueImageView m_storage_image_view {};
    std::uint32_t m_storage_image_width {};
    std::uint32_t m_storage_image_height {};
    vk::UniqueImage m_render_target_image {};
    vk::UniqueImageView m_render_target_image_view {};
    std::uint32_t m_render_target_width {};
    std::uint32_t m_render_target_height {};
    vk::UniqueSampler m_render_target_sampler {};
    vk::UniqueDescriptorPool m_command_pool {};
    vk::UniqueDescriptorSetLayout m_storage_image_descriptor_set_layout {};
    vk::UniqueDescriptorSetLayout m_render_target_descriptor_set_layout {};
    vk::UniqueDescriptorPool m_descriptor_pool {};
    vk::DescriptorSet m_storage_image_descriptor_set {};
    vk::DescriptorSet m_render_target_descriptor_set {};
    vk::UniquePipelineLayout m_compute_pipeline_layout {};
    vk::UniquePipeline m_compute_pipeline {};
    vk::UniqueRenderPass m_render_pass {};
    std::vector<vk::UniqueFramebuffer> m_framebuffers {};
    static constexpr std::uint32_t s_frames_in_flight {2};
    std::array<vk::CommandBuffer, s_frames_in_flight>
        m_frame_command_buffers {};
    std::array<vk::UniqueSemaphore, s_frames_in_flight>
        m_image_available_semaphores {};
    std::array<vk::UniqueSemaphore, s_frames_in_flight>
        m_render_finished_semaphores {};
    std::array<vk::UniqueFence, s_frames_in_flight> m_in_flight_fences {};
    std::uint32_t m_current_frame {};
    bool m_framebuffer_resized {};
    vk::UniqueBuffer m_vertex_buffer {};
    Unique_allocation m_vertex_buffer_allocation {};
    vk::DeviceSize m_vertex_buffer_size {};
    vk::UniqueBuffer m_index_buffer {};
    Unique_allocation m_index_buffer_allocation {};
    vk::DeviceSize m_index_buffer_size {};
};

#endif // VULKAN_RENDERER_HPP
