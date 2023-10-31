#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

#include <cstdint>

struct Vulkan_global_functions
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
    PFN_vkEnumerateInstanceExtensionProperties
        vkEnumerateInstanceExtensionProperties;
    PFN_vkCreateInstance vkCreateInstance;
};

struct Vulkan_instance_functions
{
    PFN_vkDestroyInstance vkDestroyInstance;
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
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

struct Vulkan_device_functions
{
    PFN_vkDestroyDevice vkDestroyDevice;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkCreateCommandPool  vkCreateCommandPool;
    PFN_vkDestroyCommandPool  vkDestroyCommandPool;
};

struct Vulkan_instance
{
    VkInstance instance;
    Vulkan_instance_functions functions;
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
};

struct Vulkan_physical_device
{
    VkPhysicalDevice physical_device;
    std::uint32_t queue_family_index;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR
        ray_tracing_pipeline_properties;
};

// FIXME: merge Vulkan_physical_device and Vulkan_device
struct Vulkan_device
{
    VkDevice device;
    Vulkan_device_functions functions;
};

struct Vulkan_context
{
    Vulkan_global_functions global_functions;
    Vulkan_instance instance;
    Vulkan_physical_device physical_device;
    Vulkan_device device;
    VmaAllocator allocator;
    VkQueue queue;
    VkCommandPool command_pool;
};

[[nodiscard]] Vulkan_context
create_vulkan_context(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr);

void destroy_vulkan_context(Vulkan_context &context);

#endif // RENDERER_HPP
