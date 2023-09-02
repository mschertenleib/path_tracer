#include "renderer.hpp"

#include <iostream>
#include <limits>

namespace
{

#ifndef NDEBUG

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity [[maybe_unused]],
    VkDebugUtilsMessageTypeFlagsEXT messageTypes [[maybe_unused]],
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData [[maybe_unused]])
{
    std::cerr << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void create_debug_messenger(Renderer_state &vk)
{
}

#endif

void load_global_functions(Renderer_state &vk)
{
    vk.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;

#define LOAD_FUNCTION(f)                                                       \
    do                                                                         \
    {                                                                          \
        vk.f = reinterpret_cast<decltype(vk.f)>(                               \
            vk.vkGetInstanceProcAddr(VK_NULL_HANDLE, #f));                     \
        assert(vk.f);                                                          \
    } while (false)

    LOAD_FUNCTION(vkEnumerateInstanceLayerProperties);
    LOAD_FUNCTION(vkEnumerateInstanceExtensionProperties);
    LOAD_FUNCTION(vkCreateInstance);

#undef LOAD_FUNCTION
}

void create_instance(Renderer_state &vk, const void *p_next)
{
}

void load_instance_functions(Renderer_state &vk)
{
    assert(vk.vkGetInstanceProcAddr);
    assert(vk.instance.get());

#define LOAD_FUNCTION(f)                                                       \
    do                                                                         \
    {                                                                          \
        vk.f = reinterpret_cast<decltype(vk.f)>(                               \
            vk.vkGetInstanceProcAddr(vk.instance.get(), #f));                  \
        assert(vk.f);                                                          \
    } while (false)

    LOAD_FUNCTION(vkDestroyInstance);
    LOAD_FUNCTION(vkCreateDebugUtilsMessengerEXT);
    LOAD_FUNCTION(vkDestroyDebugUtilsMessengerEXT);

#undef LOAD_FUNCTION
}

void load_device_functions(Renderer_state &vk)
{
}

} // namespace

void renderer_init(Renderer_state &vk)
{
    load_global_functions(vk);
}
