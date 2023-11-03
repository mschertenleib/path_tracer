#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

#include <cstdint>

struct Vulkan_instance
{
    VkInstance instance;
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
    PFN_vkEnumerateInstanceExtensionProperties
        vkEnumerateInstanceExtensionProperties;
    PFN_vkCreateInstance vkCreateInstance;
    PFN_vkDestroyInstance vkDestroyInstance;
#ifndef NDEBUG
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
#endif
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties
        vkGetPhysicalDeviceQueueFamilyProperties;
    PFN_vkEnumerateDeviceExtensionProperties
        vkEnumerateDeviceExtensionProperties;
    PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
    PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
    PFN_vkCreateDevice vkCreateDevice;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
};

struct Vulkan_device
{
    VkPhysicalDevice physical_device;
    std::uint32_t queue_family_index;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR
        ray_tracing_pipeline_properties;
    VkDevice device;
    PFN_vkDestroyDevice vkDestroyDevice;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkCreateCommandPool vkCreateCommandPool;
    PFN_vkDestroyCommandPool vkDestroyCommandPool;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkQueueWaitIdle vkQueueWaitIdle;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
};

struct Vulkan_image
{
    std::uint32_t width;
    std::uint32_t height;
    VkImage image;
    VmaAllocation allocation;
};

struct Vulkan_buffer
{
    VkDeviceSize size;
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct Vulkan_context
{
    Vulkan_instance instance;
    Vulkan_device device;
    VmaAllocator allocator;
    VkQueue queue;
    VkCommandPool command_pool;
    Vulkan_image storage_image;
    VkImageView storage_image_view;
    Vulkan_buffer vertex_buffer;
    Vulkan_buffer index_buffer;
};

[[nodiscard]] Vulkan_context
create_vulkan_context(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr);
void destroy_vulkan_context(Vulkan_context &context);

void load_scene(Vulkan_context &context,
                std::uint32_t render_width,
                std::uint32_t render_height,
                const struct Geometry &geometry);
void destroy_scene_resources(const Vulkan_context &context);

void render(const Vulkan_context &context);

void write_to_png(const Vulkan_context &context, const char *file_name);

#endif // RENDERER_HPP
