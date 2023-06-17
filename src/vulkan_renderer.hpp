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

    explicit Unique_allocator(const VmaAllocatorCreateInfo &create_info);

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
    VmaAllocator m_allocator {};
};

class Unique_buffer
{
public:
    constexpr Unique_buffer() noexcept = default;

    Unique_buffer(VmaAllocator allocator,
                  const vk::BufferCreateInfo &buffer_create_info,
                  const VmaAllocationCreateInfo &allocation_create_info,
                  VmaAllocationInfo *allocation_info);

    ~Unique_buffer() noexcept;

    Unique_buffer(Unique_buffer &&rhs) noexcept;
    Unique_buffer &operator=(Unique_buffer &&rhs) noexcept;

    Unique_buffer(const Unique_buffer &) = delete;
    Unique_buffer &operator=(const Unique_buffer &) = delete;

    [[nodiscard]] constexpr const vk::Buffer &get() const noexcept
    {
        return m_buffer;
    }

private:
    vk::Buffer m_buffer {};
    VmaAllocation m_allocation {};
    VmaAllocator m_allocator {};
};

class Unique_image
{
public:
    constexpr Unique_image() noexcept = default;

    Unique_image(VmaAllocator allocator,
                 const vk::ImageCreateInfo &image_create_info,
                 const VmaAllocationCreateInfo &allocation_create_info);

    ~Unique_image() noexcept;

    Unique_image(Unique_image &&rhs) noexcept;
    Unique_image &operator=(Unique_image &&rhs) noexcept;

    Unique_image(const Unique_image &) = delete;
    Unique_image &operator=(const Unique_image &) = delete;

    [[nodiscard]] constexpr const vk::Image &get() const noexcept
    {
        return m_image;
    }

private:
    vk::Image m_image {};
    VmaAllocation m_allocation {};
    VmaAllocator m_allocator {};
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
    VmaAllocation m_allocation {};
    VmaAllocator m_allocator {};
};

struct Queue_family_indices
{
    std::uint32_t graphics;
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

    [[nodiscard]] vk::DescriptorSet get_final_render_descriptor_set();
    [[nodiscard]] std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> get_heap_budgets();
    void draw_frame(std::uint32_t rng_seed);
    void resize_framebuffer();
    void resize_render_target(std::uint32_t render_width,
                              std::uint32_t render_height);
    void store_to_png(const char *file_name);

private:
    void create_instance(std::uint32_t api_version);
    void create_surface();
    void select_physical_device(std::uint32_t device_extension_count,
                                const char *const *device_extension_names);
    [[nodiscard]] Queue_family_indices
    get_queue_family_indices(vk::PhysicalDevice physical_device);
    void create_device(std::uint32_t device_extension_count,
                       const char *const *device_extension_names);
    void create_allocator(std::uint32_t api_version);
    void create_swapchain(std::uint32_t framebuffer_width,
                          std::uint32_t framebuffer_height);
    void create_command_pool();
    [[nodiscard]] vk::CommandBuffer begin_one_time_submit_command_buffer();
    void end_one_time_submit_command_buffer(vk::CommandBuffer command_buffer);
    void create_storage_image(std::uint32_t width, std::uint32_t height);
    void create_render_target(std::uint32_t width, std::uint32_t height);
    void create_vertex_buffer(const std::vector<float> &vertices);
    void create_index_buffer(const std::vector<std::uint32_t> &indices);
    void create_geometry_buffers();
    [[nodiscard]] vk::DeviceAddress get_device_address(vk::Buffer buffer);
    [[nodiscard]] vk::DeviceAddress
    get_device_address(vk::AccelerationStructureKHR acceleration_structure);
    void create_blas();
    void create_tlas();
    void create_descriptor_set_layout();
    void create_final_render_descriptor_set_layout();
    void create_descriptor_pool();
    void create_descriptor_set();
    void create_final_render_descriptor_set();
    [[nodiscard]] vk::UniqueShaderModule
    create_shader_module(const char *file_name);
    void create_ray_tracing_pipeline();
    void create_shader_binding_table();
    void create_render_pass();
    void create_framebuffers();
    void create_command_buffers();
    void create_synchronization_objects();
    void init_imgui();
    void recreate_swapchain();

    struct GLFWwindow *m_window {};

    vk::UniqueInstance m_instance {};
#ifndef NDEBUG
    vk::UniqueDebugUtilsMessengerEXT m_debug_messenger {};
#endif

    vk::UniqueSurfaceKHR m_surface {};

    Queue_family_indices m_queue_family_indices {
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint32_t>::max()};
    vk::PhysicalDevice m_physical_device {};
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
        m_ray_tracing_properties {};

    vk::UniqueDevice m_device {};

    Unique_allocator m_allocator {};

    vk::Queue m_graphics_queue {};
    vk::Queue m_present_queue {};

    vk::Format m_swapchain_format {vk::Format::eUndefined};
    vk::Extent2D m_swapchain_extent {};
    std::uint32_t m_swapchain_min_image_count {};
    vk::UniqueSwapchainKHR m_swapchain {};
    std::vector<vk::Image> m_swapchain_images {};
    std::vector<vk::UniqueImageView> m_swapchain_image_views {};

    vk::UniqueCommandPool m_command_pool {};

    std::uint32_t m_render_width {};
    std::uint32_t m_render_height {};

    Unique_image m_storage_image {};
    vk::UniqueImageView m_storage_image_view {};

    Unique_image m_render_target_image {};
    vk::UniqueImageView m_render_target_image_view {};
    vk::UniqueSampler m_render_target_sampler {};

    std::uint32_t m_num_vertices {};
    vk::DeviceSize m_vertex_buffer_size {};
    Unique_buffer m_vertex_buffer {};

    std::uint32_t m_num_indices {};
    vk::DeviceSize m_index_buffer_size {};
    Unique_buffer m_index_buffer {};

    vk::UniqueAccelerationStructureKHR m_blas {};
    vk::UniqueAccelerationStructureKHR m_tlas {};

    vk::UniqueDescriptorSetLayout m_descriptor_set_layout {};

    vk::UniqueDescriptorSetLayout m_final_render_descriptor_set_layout {};

    vk::UniqueDescriptorPool m_descriptor_pool {};

    vk::DescriptorSet m_descriptor_set {};

    vk::DescriptorSet m_final_render_descriptor_set {};

    std::vector<vk::RayTracingShaderGroupCreateInfoKHR>
        m_ray_tracing_shader_groups {};
    vk::UniquePipelineLayout m_ray_tracing_pipeline_layout {};
    vk::UniquePipeline m_ray_tracing_pipeline {};
    Unique_buffer m_sbt_buffer;
    vk::StridedDeviceAddressRegionKHR m_rgen_region {};
    vk::StridedDeviceAddressRegionKHR m_miss_region {};
    vk::StridedDeviceAddressRegionKHR m_hit_region {};
    vk::StridedDeviceAddressRegionKHR m_call_region {};

    vk::UniqueRenderPass m_render_pass {};

    std::vector<vk::UniqueFramebuffer> m_framebuffers {};

    std::vector<vk::CommandBuffer> m_command_buffers {};

    static constexpr std::uint32_t s_frames_in_flight {2};
    std::array<vk::UniqueSemaphore, s_frames_in_flight>
        m_image_available_semaphores {};
    std::array<vk::UniqueSemaphore, s_frames_in_flight>
        m_render_finished_semaphores {};
    std::array<vk::UniqueFence, s_frames_in_flight> m_in_flight_fences {};

    std::uint32_t m_current_frame {};

    bool m_framebuffer_resized {};
};

#endif // VULKAN_RENDERER_HPP
