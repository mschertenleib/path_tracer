#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

void init();
void shutdown();
void run();

class Vulkan_instance
{
public:
    constexpr Vulkan_instance() noexcept = default;

    Vulkan_instance(PFN_vkGetInstanceProcAddr loader_fn,
                    std::uint32_t api_version,
                    const void *next,
                    std::uint32_t enabled_layer_count,
                    const char *const *enabled_layer_names,
                    std::uint32_t enabled_extension_count,
                    const char *const *enabled_extension_names);

    ~Vulkan_instance() noexcept;

    Vulkan_instance(const Vulkan_instance &) = delete;
    Vulkan_instance(Vulkan_instance &&) noexcept = default;

    Vulkan_instance &operator=(const Vulkan_instance &) = delete;
    Vulkan_instance &operator=(Vulkan_instance &&) noexcept = default;

    [[nodiscard]] inline constexpr const VkInstance &get() const noexcept
    {
        return m_instance;
    }

private:
    VkInstance m_instance {VK_NULL_HANDLE};
};

#ifndef NDEBUG

class Vulkan_debug_messenger
{
public:
    constexpr Vulkan_debug_messenger() noexcept = default;

    Vulkan_debug_messenger(
        PFN_vkGetInstanceProcAddr loader_fn,
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT &create_info);

    ~Vulkan_debug_messenger() noexcept;

    Vulkan_debug_messenger(const Vulkan_debug_messenger &) = delete;
    Vulkan_debug_messenger(Vulkan_debug_messenger &&) noexcept = default;

    Vulkan_debug_messenger &operator=(const Vulkan_debug_messenger &) = delete;
    Vulkan_debug_messenger &
    operator=(Vulkan_debug_messenger &&) noexcept = default;

    [[nodiscard]] inline constexpr const VkDebugUtilsMessengerEXT &
    get() const noexcept
    {
        return m_debug_messenger;
    }

private:
    VkDebugUtilsMessengerEXT m_debug_messenger {VK_NULL_HANDLE};
    VkInstance m_instance {VK_NULL_HANDLE};
};

#endif

namespace temporary
{

class Vulkan_handle
{
public:
    constexpr Vulkan_handle() noexcept = default;

    Vulkan_handle(VkInstance instance);

    ~Vulkan_handle() noexcept;

    Vulkan_handle(const Vulkan_handle &) = delete;
    Vulkan_handle(Vulkan_handle &&) noexcept = default;

    Vulkan_handle &operator=(const Vulkan_handle &) = delete;
    Vulkan_handle &operator=(Vulkan_handle &&) noexcept = default;

    [[nodiscard]] inline constexpr const VkDebugUtilsMessengerEXT &
    get() const noexcept
    {
        return m_handle;
    }

private:
    VkDebugUtilsMessengerEXT m_handle {VK_NULL_HANDLE};
    VkInstance m_instance {VK_NULL_HANDLE};
};

} // namespace temporary

struct Vulkan_renderer
{
    Vulkan_instance instance;
#ifndef NDEBUG
    Vulkan_debug_messenger debug_messenger;
#endif
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    std::uint32_t queue_family {std::numeric_limits<std::uint32_t>::max()};
    VkDevice device;

    VkQueue queue;
    VmaAllocator allocator;

    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    std::uint32_t swapchain_image_count;
    std::uint32_t swapchain_min_image_count;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;

    VkImage storage_image_image;
    VmaAllocation storage_image_allocation;
    VkImageView storage_image_image_view;
    std::uint32_t storage_image_width;
    std::uint32_t storage_image_height;

    VkImage render_target_image;
    VmaAllocation render_target_allocation;
    VkImageView render_target_image_view;
    std::uint32_t render_target_width;
    std::uint32_t render_target_height;

    VkSampler render_target_sampler;
    VkCommandPool command_pool;
    VkDescriptorSetLayout storage_image_descriptor_set_layout;
    VkDescriptorSetLayout render_target_descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet storage_image_descriptor_set;
    VkDescriptorSet render_target_descriptor_set;
    VkPipelineLayout compute_pipeline_layout;
    VkPipeline compute_pipeline;
    VkRenderPass render_pass;
    std::vector<VkFramebuffer> framebuffers;

    static constexpr std::uint32_t frames_in_flight {2};
    std::array<VkCommandBuffer, frames_in_flight> frame_command_buffers;
    std::array<VkSemaphore, frames_in_flight> image_available_semaphores;
    std::array<VkSemaphore, frames_in_flight> render_finished_semaphores;
    std::array<VkFence, frames_in_flight> in_flight_fences;

    std::uint32_t current_frame;
    bool framebuffer_resized;

    VkBuffer vertex_buffer;
    VmaAllocation vertex_buffer_allocation;
    VkDeviceSize vertex_buffer_size;

    VkBuffer index_buffer;
    VmaAllocation index_buffer_allocation;
    VkDeviceSize index_buffer_size;
};

#endif // VULKAN_RENDERER_HPP
