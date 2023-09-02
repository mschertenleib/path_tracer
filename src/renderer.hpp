#ifndef RENDERER_HPP
#define RENDERER_HPP

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

#include <cassert>
#include <cstdint>
#include <utility>

template <typename T>
class Handle
{
public:
    constexpr Handle() noexcept = default;

    using Pfn_destroy = void (*)(const struct Renderer_state *, T);

    Handle(const struct Renderer_state *renderer,
           T handle,
           Pfn_destroy pfn_destroy)
        : m_renderer {renderer}, m_handle {handle}, m_pfn_destroy {pfn_destroy}
    {
        assert(m_renderer);
        assert(m_handle);
        assert(m_pfn_destroy);
    }

    ~Handle() noexcept
    {
        if (m_handle)
        {
            m_pfn_destroy(m_renderer, m_handle);
        }
    }

    constexpr friend void swap(Handle &a, Handle &b) noexcept
    {
        std::swap(a.m_renderer, b.m_renderer);
        std::swap(a.m_handle, b.m_handle);
        std::swap(a.m_pfn_destroy, b.m_pfn_destroy);
    }

    Handle(Handle &&other) noexcept : Handle()
    {
        swap(*this, other);
    }

    Handle &operator=(Handle &&other) noexcept
    {
        auto tmp {other};
        swap(*this, tmp);
    }

    [[nodiscard]] constexpr T get() const noexcept
    {
        return m_handle;
    }

    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;

private:
    const struct Renderer_state *m_renderer {};
    T m_handle {};
    Pfn_destroy m_pfn_destroy {};
};

struct Renderer_state
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr {};
    PFN_vkEnumerateInstanceLayerProperties
        vkEnumerateInstanceLayerProperties {};
    PFN_vkEnumerateInstanceExtensionProperties
        vkEnumerateInstanceExtensionProperties {};
    PFN_vkCreateInstance vkCreateInstance {};
    PFN_vkDestroyInstance vkDestroyInstance {};
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT {};
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT {};

    Handle<VkInstance> instance {};
#ifndef NDEBUG
    Handle<VkDebugUtilsMessengerEXT> debug_messenger {};
#endif
    std::uint32_t queue_family_index {};
    VkPhysicalDevice physical_device {};
    VkPhysicalDeviceProperties physical_device_properties {};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR
        physical_device_ray_tracing_pipeline_properties {};
    Handle<VkDevice> device {};
    Handle<VmaAllocator> allocator {};
    VkQueue queue {};
    Handle<VkCommandPool> command_pool {};
};

void renderer_init(Renderer_state &vk);

#endif // RENDERER_HPP
