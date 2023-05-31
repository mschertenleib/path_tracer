#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

void init();
void shutdown();
void run();

template <typename Handle, typename Destroy, typename Owner = void>
class Vulkan_handle
{
public:
    constexpr Vulkan_handle() noexcept = default;

    Vulkan_handle(Handle handle, Destroy destroy, Owner owner) noexcept
        : m_handle {handle}, m_destroy {destroy}, m_owner {owner}
    {
        assert(m_handle);
        assert(m_destroy);
        assert(m_owner);
    }

    ~Vulkan_handle() noexcept
    {
        if (m_handle)
        {
            m_destroy(m_owner, m_handle, nullptr);
        }
    }

    Vulkan_handle(const Vulkan_handle &) = delete;
    Vulkan_handle(Vulkan_handle &&) noexcept = default;

    Vulkan_handle &operator=(const Vulkan_handle &) = delete;
    Vulkan_handle &operator=(Vulkan_handle &&) noexcept = default;

    [[nodiscard]] constexpr const Handle &get() const noexcept
    {
        return m_handle;
    }

private:
    Handle m_handle {VK_NULL_HANDLE};
    Destroy m_destroy {nullptr};
    Owner m_owner {VK_NULL_HANDLE};
};

template <typename Handle, typename Destroy>
class Vulkan_handle<Handle, Destroy, void>
{
public:
    constexpr Vulkan_handle() noexcept = default;

    Vulkan_handle(Handle handle, Destroy destroy) noexcept
        : m_handle {handle}, m_destroy {destroy}
    {
        assert(m_handle);
        assert(m_destroy);
    }

    ~Vulkan_handle() noexcept
    {
        if (m_handle)
        {
            m_destroy(m_handle, nullptr);
        }
    }

    Vulkan_handle(const Vulkan_handle &) = delete;
    Vulkan_handle(Vulkan_handle &&) noexcept = default;

    Vulkan_handle &operator=(const Vulkan_handle &) = delete;
    Vulkan_handle &operator=(Vulkan_handle &&) noexcept = default;

    [[nodiscard]] constexpr const Handle &get() const noexcept
    {
        return m_handle;
    }

private:
    Handle m_handle {VK_NULL_HANDLE};
    Destroy m_destroy {nullptr};
};

using Vulkan_instance = Vulkan_handle<VkInstance, PFN_vkDestroyInstance>;

#ifndef NDEBUG
using Vulkan_debug_messenger =
    Vulkan_handle<VkDebugUtilsMessengerEXT,
                  PFN_vkDestroyDebugUtilsMessengerEXT,
                  VkInstance>;
#endif

using Vulkan_surface =
    Vulkan_handle<VkSurfaceKHR, PFN_vkDestroySurfaceKHR, VkInstance>;

using Vulkan_device = Vulkan_handle<VkDevice, PFN_vkDestroyDevice>;

using Vulkan_swapchain =
    Vulkan_handle<VkSwapchainKHR, PFN_vkDestroySwapchainKHR, VkDevice>;

using Vulkan_image_view =
    Vulkan_handle<VkImageView, PFN_vkDestroyImageView, VkDevice>;

using Vulkan_sampler = Vulkan_handle<VkSampler, PFN_vkDestroySampler, VkDevice>;

using Vulkan_command_pool =
    Vulkan_handle<VkCommandPool, PFN_vkDestroyCommandPool, VkDevice>;

using Vulkan_descriptor_set_layout =
    Vulkan_handle<VkDescriptorSetLayout,
                  PFN_vkDestroyDescriptorSetLayout,
                  VkDevice>;

using Vulkan_descriptor_pool =
    Vulkan_handle<VkDescriptorPool, PFN_vkDestroyDescriptorPool, VkDevice>;

using Vulkan_pipeline_layout =
    Vulkan_handle<VkPipelineLayout, PFN_vkDestroyPipelineLayout, VkDevice>;

using Vulkan_shader_module =
    Vulkan_handle<VkShaderModule, PFN_vkDestroyShaderModule, VkDevice>;

using Vulkan_pipeline =
    Vulkan_handle<VkPipeline, PFN_vkDestroyPipeline, VkDevice>;

using Vulkan_render_pass =
    Vulkan_handle<VkRenderPass, PFN_vkDestroyRenderPass, VkDevice>;

using Vulkan_framebuffer =
    Vulkan_handle<VkFramebuffer, PFN_vkDestroyFramebuffer, VkDevice>;

using Vulkan_semaphore =
    Vulkan_handle<VkSemaphore, PFN_vkDestroySemaphore, VkDevice>;

using Vulkan_fence = Vulkan_handle<VkFence, PFN_vkDestroyFence, VkDevice>;

class Vulkan_allocator
{
public:
    constexpr Vulkan_allocator() noexcept = default;

    explicit Vulkan_allocator(VmaAllocator allocator) noexcept
        : m_allocator {allocator}
    {
        assert(m_allocator);
    }

    ~Vulkan_allocator() noexcept
    {
        if (m_allocator)
        {
            vmaDestroyAllocator(m_allocator);
        }
    }

    Vulkan_allocator(const Vulkan_allocator &) = delete;
    Vulkan_allocator(Vulkan_allocator &&) noexcept = default;

    Vulkan_allocator &operator=(const Vulkan_allocator &) = delete;
    Vulkan_allocator &operator=(Vulkan_allocator &&) noexcept = default;

    [[nodiscard]] constexpr const VmaAllocator &get() const noexcept
    {
        return m_allocator;
    }

private:
    VmaAllocator m_allocator {VK_NULL_HANDLE};
};

class Vulkan_image
{
public:
    constexpr Vulkan_image() noexcept = default;

    Vulkan_image(VkImage image,
                 VmaAllocation allocation,
                 VmaAllocator allocator) noexcept
        : m_image {image}, m_allocation {allocation}, m_allocator {allocator}
    {
        assert(m_image);
        assert(m_allocation);
        assert(m_allocator);
    }

    ~Vulkan_image() noexcept
    {
        if (m_image)
        {
            vmaDestroyImage(m_allocator, m_image, m_allocation);
        }
    }

    Vulkan_image(const Vulkan_image &) = delete;
    Vulkan_image(Vulkan_image &&) noexcept = default;

    Vulkan_image &operator=(const Vulkan_image &) = delete;
    Vulkan_image &operator=(Vulkan_image &&) noexcept = default;

    [[nodiscard]] constexpr const VkImage &get() const noexcept
    {
        return m_image;
    }

    [[nodiscard]] constexpr const VmaAllocation &get_allocation() const noexcept
    {
        return m_allocation;
    }

private:
    VkImage m_image {VK_NULL_HANDLE};
    VmaAllocation m_allocation {VK_NULL_HANDLE};
    VmaAllocator m_allocator {VK_NULL_HANDLE};
};

class Vulkan_buffer
{
public:
    constexpr Vulkan_buffer() noexcept = default;

    Vulkan_buffer(VkBuffer buffer,
                  VmaAllocation allocation,
                  VmaAllocator allocator) noexcept
        : m_buffer {buffer}, m_allocation {allocation}, m_allocator {allocator}
    {
        assert(m_buffer);
        assert(m_allocation);
        assert(m_allocator);
    }

    ~Vulkan_buffer() noexcept
    {
        if (m_buffer)
        {
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        }
    }

    Vulkan_buffer(const Vulkan_buffer &) = delete;
    Vulkan_buffer(Vulkan_buffer &&) noexcept = default;

    Vulkan_buffer &operator=(const Vulkan_buffer &) = delete;
    Vulkan_buffer &operator=(Vulkan_buffer &&) noexcept = default;

    [[nodiscard]] constexpr const VkBuffer &get() const noexcept
    {
        return m_buffer;
    }

    [[nodiscard]] constexpr const VmaAllocation &get_allocation() const noexcept
    {
        return m_allocation;
    }

private:
    VkBuffer m_buffer {VK_NULL_HANDLE};
    VmaAllocation m_allocation {VK_NULL_HANDLE};
    VmaAllocator m_allocator {VK_NULL_HANDLE};
};

struct Vulkan_renderer
{
    Vulkan_instance instance;
#ifndef NDEBUG
    Vulkan_debug_messenger debug_messenger;
#endif
    Vulkan_surface surface;
    VkPhysicalDevice physical_device;
    std::uint32_t queue_family {std::numeric_limits<std::uint32_t>::max()};
    Vulkan_device device;
    VkQueue queue;
    Vulkan_allocator allocator;
    Vulkan_swapchain swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    std::uint32_t swapchain_image_count;
    std::uint32_t swapchain_min_image_count;
    std::vector<VkImage> swapchain_images;
    std::vector<Vulkan_image_view> swapchain_image_views;
    Vulkan_image storage_image;
    Vulkan_image_view storage_image_view;
    std::uint32_t storage_image_width;
    std::uint32_t storage_image_height;
    Vulkan_image render_target_image;
    Vulkan_image_view render_target_image_view;
    std::uint32_t render_target_width;
    std::uint32_t render_target_height;
    Vulkan_sampler render_target_sampler;
    Vulkan_command_pool command_pool;
    Vulkan_descriptor_set_layout storage_image_descriptor_set_layout;
    Vulkan_descriptor_set_layout render_target_descriptor_set_layout;
    Vulkan_descriptor_pool descriptor_pool;
    VkDescriptorSet storage_image_descriptor_set;
    VkDescriptorSet render_target_descriptor_set;
    Vulkan_pipeline_layout compute_pipeline_layout;
    Vulkan_pipeline compute_pipeline;
    Vulkan_render_pass render_pass;
    std::vector<Vulkan_framebuffer> framebuffers;
    static constexpr std::uint32_t frames_in_flight {2};
    std::array<VkCommandBuffer, frames_in_flight> frame_command_buffers;
    std::array<Vulkan_semaphore, frames_in_flight> image_available_semaphores;
    std::array<Vulkan_semaphore, frames_in_flight> render_finished_semaphores;
    std::array<Vulkan_fence, frames_in_flight> in_flight_fences;
    std::uint32_t current_frame;
    bool framebuffer_resized;
    Vulkan_buffer vertex_buffer;
    VkDeviceSize vertex_buffer_size;
    Vulkan_buffer index_buffer;
    VkDeviceSize index_buffer_size;
};

#endif // VULKAN_RENDERER_HPP
