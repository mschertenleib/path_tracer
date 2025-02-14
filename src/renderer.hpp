#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <vk_mem_alloc.h>

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_NO_SPACESHIP_OPERATOR
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#ifndef NDEBUG
#define ENABLE_VALIDATION
#endif

class Unique_allocator
{
public:
    constexpr Unique_allocator() noexcept = default;

    constexpr explicit Unique_allocator(VmaAllocator allocator) noexcept
        : m_allocator {allocator}
    {
    }

    constexpr Unique_allocator(Unique_allocator &&rhs) noexcept
    {
        swap(rhs);
    }

    Unique_allocator &operator=(Unique_allocator &&rhs) noexcept
    {
        Unique_allocator temp(std::move(rhs));
        swap(temp);
        return *this;
    }

    Unique_allocator(const Unique_allocator &) = delete;
    Unique_allocator &operator=(const Unique_allocator &) = delete;

    ~Unique_allocator() noexcept
    {
        if (m_allocator)
        {
            vmaDestroyAllocator(m_allocator);
        }
    }

    [[nodiscard]] constexpr VmaAllocator get() const noexcept
    {
        return m_allocator;
    }

    constexpr void swap(Unique_allocator &rhs) noexcept
    {
        std::swap(m_allocator, rhs.m_allocator);
    }

private:
    VmaAllocator m_allocator {};
};

class Unique_allocation
{
public:
    constexpr Unique_allocation() noexcept = default;

    constexpr explicit Unique_allocation(VmaAllocation allocation,
                                         VmaAllocator allocator) noexcept
        : m_allocator {allocator}, m_allocation {allocation}
    {
    }

    constexpr Unique_allocation(Unique_allocation &&rhs) noexcept
    {
        swap(rhs);
    }

    Unique_allocation &operator=(Unique_allocation &&rhs) noexcept
    {
        Unique_allocation temp(std::move(rhs));
        swap(temp);
        return *this;
    }

    Unique_allocation(const Unique_allocation &) = delete;
    Unique_allocation &operator=(const Unique_allocation &) = delete;

    ~Unique_allocation() noexcept
    {
        if (m_allocator && m_allocation)
        {
            vmaFreeMemory(m_allocator, m_allocation);
        }
    }

    [[nodiscard]] constexpr VmaAllocation get() const noexcept
    {
        return m_allocation;
    }

    constexpr void swap(Unique_allocation &rhs) noexcept
    {
        std::swap(m_allocator, rhs.m_allocator);
        std::swap(m_allocation, rhs.m_allocation);
    }

private:
    VmaAllocator m_allocator {};
    VmaAllocation m_allocation {};
};

class ImGui_backend
{
public:
    constexpr ImGui_backend() noexcept = default;

    constexpr explicit ImGui_backend(bool initialized) noexcept
        : m_initialized {initialized}
    {
    }

    constexpr ImGui_backend(ImGui_backend &&rhs) noexcept
    {
        swap(rhs);
    }

    ImGui_backend &operator=(ImGui_backend &&rhs) noexcept
    {
        ImGui_backend temp(std::move(rhs));
        swap(temp);
        return *this;
    }

    ImGui_backend(const ImGui_backend &) = delete;
    ImGui_backend &operator=(const ImGui_backend &) = delete;

    ~ImGui_backend() noexcept;

    constexpr void swap(ImGui_backend &rhs) noexcept
    {
        std::swap(m_initialized, rhs.m_initialized);
    }

private:
    bool m_initialized {};
};

struct Vulkan_image
{
    std::uint32_t width;
    std::uint32_t height;
    vk::UniqueImage image;
    Unique_allocation allocation;
};

struct Vulkan_buffer
{
    vk::DeviceSize size;
    vk::UniqueBuffer buffer;
    Unique_allocation allocation;
};

struct Vulkan_context
{
    vk::DynamicLoader dl;

    vk::UniqueInstance instance;
#ifdef ENABLE_VALIDATION
    vk::UniqueDebugUtilsMessengerEXT debug_messenger;
#endif

    vk::PhysicalDevice physical_device;
    std::uint32_t graphics_compute_queue_family_index;
    std::uint32_t present_queue_family_index;
    vk::PhysicalDeviceProperties physical_device_properties;
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
        physical_device_ray_tracing_pipeline_properties;

    vk::UniqueDevice device;
    vk::Queue graphics_compute_queue;
    vk::Queue present_queue;

    vk::UniqueSurfaceKHR surface;

    Unique_allocator allocator;

    vk::UniqueCommandPool command_pool;

    bool framebuffer_resized;
    std::uint32_t framebuffer_width;
    std::uint32_t framebuffer_height;
    vk::Format swapchain_format;
    vk::Extent2D swapchain_extent;
    std::uint32_t swapchain_min_image_count;
    vk::UniqueSwapchainKHR swapchain;
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::UniqueImageView> swapchain_image_views;

    vk::UniqueDescriptorPool descriptor_pool;
    vk::UniqueRenderPass render_pass;
    std::vector<vk::UniqueFramebuffer> framebuffers;
    static constexpr std::uint32_t frames_in_flight {2};
    std::vector<vk::UniqueCommandBuffer> command_buffers;
    std::array<vk::UniqueSemaphore, frames_in_flight>
        image_available_semaphores;
    std::array<vk::UniqueSemaphore, frames_in_flight>
        render_finished_semaphores;
    std::array<vk::UniqueFence, frames_in_flight> in_flight_fences;
    std::uint32_t current_frame_in_flight;
    std::uint32_t global_frame_count;

    vk::UniqueQueryPool query_pool;

    ImGui_backend imgui_backend;
};

struct Vulkan_render_resources
{
    Vulkan_image storage_image;
    vk::UniqueImageView storage_image_view;
    Vulkan_image render_target;
    vk::UniqueImageView render_target_view;
    vk::UniqueSampler render_target_sampler;
    Vulkan_buffer vertex_buffer;
    Vulkan_buffer index_buffer;
    Vulkan_buffer blas_buffer;
    vk::UniqueAccelerationStructureKHR blas;
    Vulkan_buffer tlas_buffer;
    vk::UniqueAccelerationStructureKHR tlas;
    vk::UniqueDescriptorSetLayout descriptor_set_layout;
    vk::UniqueDescriptorSetLayout final_render_descriptor_set_layout;
    vk::UniqueDescriptorSet descriptor_set;
    vk::UniqueDescriptorSet final_render_descriptor_set;
    vk::UniquePipelineLayout ray_tracing_pipeline_layout;
    vk::UniquePipeline ray_tracing_pipeline;
    Vulkan_buffer sbt_buffer;
    vk::StridedDeviceAddressRegionKHR sbt_raygen_region;
    vk::StridedDeviceAddressRegionKHR sbt_miss_region;
    vk::StridedDeviceAddressRegionKHR sbt_hit_region;
    vk::StridedDeviceAddressRegionKHR sbt_callable_region;
    std::uint32_t samples_to_render;
    std::uint32_t sample_count;
    std::uint32_t samples_per_frame;
};

[[nodiscard]] Vulkan_context create_context(struct GLFWwindow *window);

[[nodiscard]] Vulkan_render_resources
create_render_resources(const Vulkan_context &context,
                        std::uint32_t render_width,
                        std::uint32_t render_height,
                        const struct aiScene *scene);

void draw_frame(Vulkan_context &context,
                Vulkan_render_resources &render_resources,
                const struct Camera &camera);

void resize_framebuffer(Vulkan_context &context,
                        std::uint32_t width,
                        std::uint32_t height);

void reset_render(Vulkan_render_resources &render_resources);

// On failure, returns an error message. On success, returns an empty string.
[[nodiscard]] std::string
write_to_png(const Vulkan_context &context,
             const Vulkan_render_resources &render_resources,
             const char *file_name);

#endif // RENDERER_HPP
