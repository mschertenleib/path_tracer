#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

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
};

struct Vulkan_instance
{
    VkInstance instance;
    Vulkan_instance_functions functions;
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
};

struct Vulkan_context
{
    Vulkan_global_functions global_functions;
    Vulkan_instance instance;
};

[[nodiscard]] Vulkan_context
create_vulkan_context(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr);

void destroy_vulkan_context(Vulkan_context &context);

#endif // RENDERER_HPP
