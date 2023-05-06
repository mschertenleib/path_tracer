
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include "imgui_impl_vulkan.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <array>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{

struct
{
    GLFWwindow *window;

    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
    PFN_vkEnumerateInstanceExtensionProperties
        vkEnumerateInstanceExtensionProperties;
    PFN_vkCreateInstance vkCreateInstance;

    VkInstance instance;

#ifndef NDEBUG
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
#endif
    PFN_vkDestroyInstance vkDestroyInstance;
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkEnumerateDeviceExtensionProperties
        vkEnumerateDeviceExtensionProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties
        vkGetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
        vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkCreateDevice vkCreateDevice;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;

#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    std::uint32_t queue_family;
    VkDevice device;

    PFN_vkDestroyDevice vkDestroyDevice;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkDestroyImage vkDestroyImage;
    PFN_vkCreateCommandPool vkCreateCommandPool;
    PFN_vkDestroyCommandPool vkDestroyCommandPool;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    PFN_vkCmdBindPipeline vkCmdBindPipeline;
    PFN_vkCmdDispatch vkCmdDispatch;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkQueueWaitIdle vkQueueWaitIdle;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    PFN_vkCreateShaderModule vkCreateShaderModule;
    PFN_vkDestroyShaderModule vkDestroyShaderModule;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    PFN_vkCreateComputePipelines vkCreateComputePipelines;
    PFN_vkDestroyPipeline vkDestroyPipeline;
    PFN_vkCreateRenderPass vkCreateRenderPass;
    PFN_vkDestroyRenderPass vkDestroyRenderPass;

    VkQueue queue;
    VmaAllocator allocator;
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    std::uint32_t swapchain_image_count;
    std::uint32_t swapchain_min_image_count;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkImage render_image;
    VkImageView render_image_view;
    VmaAllocation render_image_allocation;
    std::uint32_t render_image_width;
    std::uint32_t render_image_height;
    VkCommandPool command_pool;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkPipelineLayout compute_pipeline_layout;
    VkPipeline compute_pipeline;
    VkRenderPass render_pass;
} g {};

[[noreturn]] void
fatal_error(std::string_view message,
            const std::source_location &loc = std::source_location::current())
{
    std::ostringstream oss;
    oss << loc.file_name() << ':' << loc.line() << ": " << message;
    throw std::runtime_error(oss.str());
}

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

[[nodiscard]] constexpr const char *
vk_result_to_string(VkResult result) noexcept
{
    switch (result)
    {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
#ifdef VK_ENABLE_BETA_EXTENSIONS
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
        return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
#endif
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
        return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
        return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
    default: return "unknown";
    }
}

void vk_check(VkResult result,
              const std::source_location &loc = std::source_location::current())
{
    if (result == VK_SUCCESS)
    {
        return;
    }

    std::ostringstream oss;
    oss << "VkResult is " << vk_result_to_string(result);
    fatal_error(oss.str(), loc);
}

void imgui_vk_check(VkResult result)
{
    if (result == VK_SUCCESS)
    {
        return;
    }

    std::ostringstream oss;
    oss << "VkResult is " << vk_result_to_string(result);
    fatal_error(oss.str());
}

[[nodiscard]] std::vector<std::uint32_t> read_binary_file(const char *file_name)
{
    const std::filesystem::path path(file_name);
    if (!std::filesystem::exists(path))
    {
        std::ostringstream oss;
        oss << "File \"" << file_name << "\" does not exist";
        fatal_error(oss.str());
    }

    const auto file_size = std::filesystem::file_size(path);
    const auto buffer_length =
        (file_size + sizeof(std::uint32_t) - 1u) / sizeof(std::uint32_t);

    std::vector<std::uint32_t> buffer(buffer_length);

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::ostringstream oss;
        oss << "Failed to open file \"" << path.string() << '\"';
        fatal_error(oss.str());
    }

    if (!file.read(reinterpret_cast<char *>(buffer.data()),
                   static_cast<std::streamsize>(buffer_length *
                                                sizeof(std::uint32_t))))
    {
        std::ostringstream oss;
        oss << "Reading file \"" << path.string() << "\" failed";
        fatal_error(oss.str());
    }

    return buffer;
}

#ifndef NDEBUG

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity [[maybe_unused]],
    VkDebugUtilsMessageTypeFlagsEXT message_type [[maybe_unused]],
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data [[maybe_unused]])
{
    std::cerr << callback_data->pMessage << std::endl;
    return VK_FALSE;
}

#endif

void load_global_commands() noexcept
{
    g.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr"));
    assert(g.vkGetInstanceProcAddr);

#define LOAD(func)                                                             \
    g.func =                                                                   \
        reinterpret_cast<PFN_##func>(g.vkGetInstanceProcAddr(nullptr, #func)); \
    assert(g.func);

    LOAD(vkEnumerateInstanceLayerProperties)
    LOAD(vkEnumerateInstanceExtensionProperties)
    LOAD(vkCreateInstance)

#undef LOAD
}

void load_instance_commands() noexcept
{
    assert(g.vkGetInstanceProcAddr);
    assert(g.instance);

#define LOAD(func)                                                             \
    g.func = reinterpret_cast<PFN_##func>(                                     \
        g.vkGetInstanceProcAddr(g.instance, #func));                           \
    assert(g.func);

#ifndef NDEBUG
    LOAD(vkCreateDebugUtilsMessengerEXT)
    LOAD(vkDestroyDebugUtilsMessengerEXT)
#endif
    LOAD(vkDestroyInstance)
    LOAD(vkDestroySurfaceKHR)
    LOAD(vkEnumeratePhysicalDevices)
    LOAD(vkEnumerateDeviceExtensionProperties)
    LOAD(vkGetPhysicalDeviceQueueFamilyProperties)
    LOAD(vkGetPhysicalDeviceFeatures2)
    LOAD(vkGetPhysicalDeviceSurfaceFormatsKHR)
    LOAD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
    LOAD(vkCreateDevice)
    LOAD(vkGetDeviceProcAddr)

#undef LOAD
}

void load_device_commands() noexcept
{
    assert(g.vkGetDeviceProcAddr);
    assert(g.device);

#define LOAD(func)                                                             \
    g.func =                                                                   \
        reinterpret_cast<PFN_##func>(g.vkGetDeviceProcAddr(g.device, #func));  \
    assert(g.func);

    LOAD(vkDestroyDevice)
    LOAD(vkGetDeviceQueue)
    LOAD(vkCreateSwapchainKHR)
    LOAD(vkDestroySwapchainKHR)
    LOAD(vkGetSwapchainImagesKHR)
    LOAD(vkCreateImageView)
    LOAD(vkDestroyImageView)
    LOAD(vkDestroyImage)
    LOAD(vkCreateCommandPool)
    LOAD(vkDestroyCommandPool)
    LOAD(vkAllocateCommandBuffers)
    LOAD(vkFreeCommandBuffers)
    LOAD(vkBeginCommandBuffer)
    LOAD(vkEndCommandBuffer)
    LOAD(vkCmdPipelineBarrier)
    LOAD(vkCmdCopyImageToBuffer)
    LOAD(vkCmdBindDescriptorSets)
    LOAD(vkCmdBindPipeline)
    LOAD(vkCmdDispatch)
    LOAD(vkQueueSubmit)
    LOAD(vkQueueWaitIdle)
    LOAD(vkDeviceWaitIdle)
    LOAD(vkCreateDescriptorSetLayout)
    LOAD(vkDestroyDescriptorSetLayout)
    LOAD(vkCreateDescriptorPool)
    LOAD(vkDestroyDescriptorPool)
    LOAD(vkAllocateDescriptorSets)
    LOAD(vkUpdateDescriptorSets)
    LOAD(vkCreateShaderModule)
    LOAD(vkDestroyShaderModule)
    LOAD(vkCreatePipelineLayout)
    LOAD(vkDestroyPipelineLayout)
    LOAD(vkCreateComputePipelines)
    LOAD(vkDestroyPipeline)
    LOAD(vkCreateRenderPass)
    LOAD(vkDestroyRenderPass)

#undef LOAD
}

void get_queue_families(VkPhysicalDevice physical_device,
                        std::uint32_t &queue_family)
{
    queue_family = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t property_count {};
    g.vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &property_count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(property_count);
    g.vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &property_count, properties.data());

    for (std::uint32_t i {}; i < property_count; ++i)
    {
        if ((properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            glfwGetPhysicalDevicePresentationSupport(
                g.instance, physical_device, i))
        {
            queue_family = i;
            return;
        }
    }
}

[[nodiscard]] bool
is_physical_device_suitable(VkPhysicalDevice physical_device,
                            const char *const *device_extensions,
                            std::uint32_t device_extension_count)
{
    VkResult result;

    std::uint32_t queue_family;
    get_queue_families(physical_device, queue_family);
    if (queue_family == std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    std::uint32_t extension_property_count {};
    result = g.vkEnumerateDeviceExtensionProperties(
        physical_device, nullptr, &extension_property_count, nullptr);
    vk_check(result);
    std::vector<VkExtensionProperties> extension_properties(
        extension_property_count);
    result =
        g.vkEnumerateDeviceExtensionProperties(physical_device,
                                               nullptr,
                                               &extension_property_count,
                                               extension_properties.data());
    vk_check(result);

    const auto is_extension_supported =
        [&extension_properties](const char *extension)
    {
        return std::any_of(
            extension_properties.begin(),
            extension_properties.end(),
            [extension](const VkExtensionProperties &properties)
            { return std::strcmp(properties.extensionName, extension) == 0; });
    };

    if (!std::all_of(device_extensions,
                     device_extensions + device_extension_count,
                     is_extension_supported))
    {
        return false;
    }

    VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features {};
    ray_query_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR
        acceleration_structure_features {};
    acceleration_structure_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    acceleration_structure_features.pNext = &ray_query_features;

    VkPhysicalDeviceFeatures2 features {};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &acceleration_structure_features;

    g.vkGetPhysicalDeviceFeatures2(physical_device, &features);

    if (!acceleration_structure_features.accelerationStructure ||
        !ray_query_features.rayQuery)
    {
        return false;
    }

    return true;
}

void create_swapchain()
{
    VkResult result;

    std::uint32_t surface_format_count {};
    result = g.vkGetPhysicalDeviceSurfaceFormatsKHR(
        g.physical_device, g.surface, &surface_format_count, nullptr);
    vk_check(result);
    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    result = g.vkGetPhysicalDeviceSurfaceFormatsKHR(g.physical_device,
                                                    g.surface,
                                                    &surface_format_count,
                                                    surface_formats.data());
    vk_check(result);

    auto surface_format = surface_formats.front();
    for (const auto &format : surface_formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surface_format = format;
            break;
        }
    }

    VkSurfaceCapabilitiesKHR surface_capabilities {};
    result = g.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        g.physical_device, g.surface, &surface_capabilities);
    vk_check(result);

    VkExtent2D swapchain_extent;
    if (surface_capabilities.currentExtent.width !=
        std::numeric_limits<std::uint32_t>::max())
    {
        swapchain_extent = surface_capabilities.currentExtent;
    }
    else
    {
        int framebuffer_width;
        int framebuffer_height;
        glfwGetFramebufferSize(
            g.window, &framebuffer_width, &framebuffer_height);
        swapchain_extent = {static_cast<std::uint32_t>(framebuffer_width),
                            static_cast<std::uint32_t>(framebuffer_height)};
    }
    swapchain_extent.width =
        std::clamp(swapchain_extent.width,
                   surface_capabilities.minImageExtent.width,
                   surface_capabilities.maxImageExtent.width);
    swapchain_extent.height =
        std::clamp(swapchain_extent.height,
                   surface_capabilities.minImageExtent.height,
                   surface_capabilities.maxImageExtent.height);

    g.swapchain_min_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 &&
        g.swapchain_min_image_count > surface_capabilities.maxImageCount)
    {
        g.swapchain_min_image_count = surface_capabilities.maxImageCount;
    }

    g.swapchain_format = surface_format.format;

    VkSwapchainCreateInfoKHR swapchain_create_info {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = g.surface;
    swapchain_create_info.minImageCount = g.swapchain_min_image_count;
    swapchain_create_info.imageFormat = surface_format.format;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = swapchain_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.preTransform = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 1;
    swapchain_create_info.pQueueFamilyIndices = &g.queue_family;

    result = g.vkCreateSwapchainKHR(
        g.device, &swapchain_create_info, nullptr, &g.swapchain);
    vk_check(result);

    result = g.vkGetSwapchainImagesKHR(
        g.device, g.swapchain, &g.swapchain_image_count, nullptr);
    vk_check(result);
    g.swapchain_images.resize(g.swapchain_image_count);
    result = g.vkGetSwapchainImagesKHR(g.device,
                                       g.swapchain,
                                       &g.swapchain_image_count,
                                       g.swapchain_images.data());
    vk_check(result);

    g.swapchain_image_views.resize(g.swapchain_image_count);

    VkImageViewCreateInfo image_view_create_info {};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = g.swapchain_format;
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;
    image_view_create_info.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    for (std::uint32_t i {}; i < g.swapchain_image_count; ++i)
    {
        image_view_create_info.image = g.swapchain_images[i];
        result = g.vkCreateImageView(g.device,
                                     &image_view_create_info,
                                     nullptr,
                                     &g.swapchain_image_views[i]);
        vk_check(result);
    }
}

void destroy_swapchain()
{
    if (!g.device)
    {
        return;
    }

    for (const auto image_view : g.swapchain_image_views)
    {
        g.vkDestroyImageView(g.device, image_view, nullptr);
    }

    if (g.swapchain)
    {
        g.vkDestroySwapchainKHR(g.device, g.swapchain, nullptr);
    }
}

[[nodiscard]] VkCommandBuffer begin_one_time_submit_command_buffer()
{
    VkResult result;

    VkCommandBufferAllocateInfo command_buffer_allocate_info {};
    command_buffer_allocate_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandPool = g.command_pool;
    command_buffer_allocate_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    result = g.vkAllocateCommandBuffers(
        g.device, &command_buffer_allocate_info, &command_buffer);
    vk_check(result);

    VkCommandBufferBeginInfo command_buffer_begin_info {};
    command_buffer_begin_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = g.vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
    vk_check(result);

    return command_buffer;
}

void end_one_time_submit_command_buffer(VkCommandBuffer command_buffer)
{
    VkResult result;

    result = g.vkEndCommandBuffer(command_buffer);
    vk_check(result);

    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    result = g.vkQueueSubmit(g.queue, 1, &submit_info, VK_NULL_HANDLE);
    vk_check(result);

    result = g.vkQueueWaitIdle(g.queue);
    vk_check(result);
}

void init()
{
    {
        glfwSetErrorCallback(glfw_error_callback);

        if (!glfwInit())
        {
            fatal_error("Failed to initialize GLFW");
        }

        if (!glfwVulkanSupported())
        {
            fatal_error("Vulkan loader or ICD have not been found");
        }

        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        g.window = glfwCreateWindow(1280, 720, "Path Tracer", nullptr, nullptr);
        if (!g.window)
        {
            fatal_error("Failed to create GLFW window");
        }
    }

    load_global_commands();

    VkResult result;

    {
        VkApplicationInfo application_info {};
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.apiVersion = VK_API_VERSION_1_2;

        std::uint32_t glfw_required_extension_count {};
        const auto glfw_required_extensions =
            glfwGetRequiredInstanceExtensions(&glfw_required_extension_count);

#ifndef NDEBUG

        std::uint32_t layer_property_count {};
        result = g.vkEnumerateInstanceLayerProperties(&layer_property_count,
                                                      nullptr);
        vk_check(result);
        std::vector<VkLayerProperties> layer_properties(layer_property_count);
        result = g.vkEnumerateInstanceLayerProperties(&layer_property_count,
                                                      layer_properties.data());
        vk_check(result);

        constexpr auto khronos_validation_layer = "VK_LAYER_KHRONOS_validation";
        if (std::none_of(layer_properties.begin(),
                         layer_properties.end(),
                         [](const VkLayerProperties &properties) {
                             return std::strcmp(properties.layerName,
                                                khronos_validation_layer) == 0;
                         }))
        {
            fatal_error("Validation layers are not supported");
        }

        std::vector<const char *> required_extensions(
            glfw_required_extensions,
            glfw_required_extensions + glfw_required_extension_count);
        required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        std::uint32_t extension_property_count {};
        result = g.vkEnumerateInstanceExtensionProperties(
            nullptr, &extension_property_count, nullptr);
        vk_check(result);
        std::vector<VkExtensionProperties> extension_properties(
            extension_property_count);
        result = g.vkEnumerateInstanceExtensionProperties(
            nullptr, &extension_property_count, extension_properties.data());
        vk_check(result);

        const auto is_extension_supported =
            [&extension_properties](const char *extension)
        {
            return std::any_of(
                extension_properties.begin(),
                extension_properties.end(),
                [extension](const VkExtensionProperties &properties) {
                    return std::strcmp(properties.extensionName, extension) ==
                           0;
                });
        };

        if (!std::all_of(required_extensions.begin(),
                         required_extensions.end(),
                         is_extension_supported))
        {
            fatal_error("Unsupported instance extension");
        }

        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info {};
        debug_messenger_create_info.sType =
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_messenger_create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_messenger_create_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_messenger_create_info.pfnUserCallback = &debug_callback;

        VkInstanceCreateInfo instance_create_info {};
        instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.pNext = &debug_messenger_create_info;
        instance_create_info.pApplicationInfo = &application_info;
        instance_create_info.enabledLayerCount = 1;
        instance_create_info.ppEnabledLayerNames = &khronos_validation_layer;
        instance_create_info.enabledExtensionCount =
            static_cast<std::uint32_t>(required_extensions.size());
        instance_create_info.ppEnabledExtensionNames =
            required_extensions.data();

        result =
            g.vkCreateInstance(&instance_create_info, nullptr, &g.instance);
        vk_check(result);

        load_instance_commands();

        result = g.vkCreateDebugUtilsMessengerEXT(g.instance,
                                                  &debug_messenger_create_info,
                                                  nullptr,
                                                  &g.debug_messenger);
        vk_check(result);

#else

        VkInstanceCreateInfo instance_create_info {};
        instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.pApplicationInfo = &application_info;
        instance_create_info.enabledExtensionCount =
            glfw_required_extension_count;
        instance_create_info.ppEnabledExtensionNames = glfw_required_extensions;

        result =
            g.vkCreateInstance(&instance_create_info, nullptr, &g.instance);
        vk_check(result);

        load_instance_commands();
#endif
    }

    {
        result =
            glfwCreateWindowSurface(g.instance, g.window, nullptr, &g.surface);
        vk_check(result);
    }

    constexpr const char *device_extensions[] {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME};
    constexpr auto device_extension_count =
        static_cast<std::uint32_t>(std::size(device_extensions));

    {
        std::uint32_t physical_device_count {};
        result = g.vkEnumeratePhysicalDevices(
            g.instance, &physical_device_count, nullptr);
        vk_check(result);
        if (physical_device_count == 0)
        {
            fatal_error("Failed to find a physical device with Vulkan support");
        }

        std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
        result = g.vkEnumeratePhysicalDevices(
            g.instance, &physical_device_count, physical_devices.data());
        vk_check(result);

        g.physical_device = {};
        for (const auto physical_device : physical_devices)
        {
            if (is_physical_device_suitable(
                    physical_device, device_extensions, device_extension_count))
            {
                g.physical_device = physical_device;
                break;
            }
        }
        if (!g.physical_device)
        {
            fatal_error("Failed to find a suitable physical device");
        }
    }

    {
        const float queue_priority {1.0f};
        VkDeviceQueueCreateInfo queue_create_info {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = g.queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;

        VkDeviceCreateInfo device_create_info {};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.queueCreateInfoCount = 1;
        device_create_info.pQueueCreateInfos = &queue_create_info;
        device_create_info.enabledExtensionCount = device_extension_count;
        device_create_info.ppEnabledExtensionNames = device_extensions;

        result = g.vkCreateDevice(
            g.physical_device, &device_create_info, nullptr, &g.device);
        vk_check(result);
    }

    load_device_commands();

    {
        g.vkGetDeviceQueue(g.device, g.queue_family, 0, &g.queue);
    }

    {
        VmaVulkanFunctions vulkan_functions {};
        vulkan_functions.vkGetInstanceProcAddr = g.vkGetInstanceProcAddr;
        vulkan_functions.vkGetDeviceProcAddr = g.vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorCreateInfo {};
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        allocatorCreateInfo.physicalDevice = g.physical_device;
        allocatorCreateInfo.device = g.device;
        allocatorCreateInfo.instance = g.instance;
        allocatorCreateInfo.pVulkanFunctions = &vulkan_functions;

        result = vmaCreateAllocator(&allocatorCreateInfo, &g.allocator);
        vk_check(result);
    }

    create_swapchain();

    {
        g.render_image_width = 160;
        g.render_image_height = 90;

        constexpr auto format = VK_FORMAT_R32G32B32A32_SFLOAT;

        VkImageCreateInfo image_create_info {};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.format = format;
        image_create_info.extent.width = g.render_image_width;
        image_create_info.extent.height = g.render_image_height;
        image_create_info.extent.depth = 1;
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = VK_IMAGE_USAGE_STORAGE_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.queueFamilyIndexCount = 1;
        image_create_info.pQueueFamilyIndices = &g.queue_family;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocation_create_info {};
        allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        result = vmaCreateImage(g.allocator,
                                &image_create_info,
                                &allocation_create_info,
                                &g.render_image,
                                &g.render_image_allocation,
                                nullptr);
        vk_check(result);

        VkImageViewCreateInfo image_view_create_info {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = g.render_image;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;
        image_view_create_info.subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        result = g.vkCreateImageView(
            g.device, &image_view_create_info, nullptr, &g.render_image_view);
        vk_check(result);
    }

    {
        VkCommandPoolCreateInfo command_pool_create_info {};
        command_pool_create_info.sType =
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.queueFamilyIndex = g.queue_family;
        result = g.vkCreateCommandPool(
            g.device, &command_pool_create_info, nullptr, &g.command_pool);
        vk_check(result);
    }

    {
        VkDescriptorSetLayoutBinding descriptor_set_layout_binding {};
        descriptor_set_layout_binding.binding = 0;
        descriptor_set_layout_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptor_set_layout_binding.descriptorCount = 1;
        descriptor_set_layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info {};
        descriptor_set_layout_create_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_create_info.bindingCount = 1;
        descriptor_set_layout_create_info.pBindings =
            &descriptor_set_layout_binding;

        result =
            g.vkCreateDescriptorSetLayout(g.device,
                                          &descriptor_set_layout_create_info,
                                          nullptr,
                                          &g.descriptor_set_layout);
        vk_check(result);
    }

    {
        const VkDescriptorPoolSize pool_sizes[] {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

        VkDescriptorPoolCreateInfo descriptor_pool_create_info {};
        descriptor_pool_create_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptor_pool_create_info.flags =
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descriptor_pool_create_info.maxSets = 1000 * std::size(pool_sizes);
        descriptor_pool_create_info.poolSizeCount =
            static_cast<std::uint32_t>(std::size(pool_sizes));
        descriptor_pool_create_info.pPoolSizes = pool_sizes;

        result = g.vkCreateDescriptorPool(g.device,
                                          &descriptor_pool_create_info,
                                          nullptr,
                                          &g.descriptor_pool);
        vk_check(result);
    }

    {
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info {};
        descriptor_set_allocate_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.descriptorPool = g.descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &g.descriptor_set_layout;

        result = g.vkAllocateDescriptorSets(
            g.device, &descriptor_set_allocate_info, &g.descriptor_set);
        vk_check(result);

        VkDescriptorImageInfo descriptor_image_info {};
        descriptor_image_info.sampler = VK_NULL_HANDLE;
        descriptor_image_info.imageView = g.render_image_view;
        descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet descriptor_write {};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = g.descriptor_set;
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorCount = 1;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptor_write.pImageInfo = &descriptor_image_info;

        g.vkUpdateDescriptorSets(g.device, 1, &descriptor_write, 0, nullptr);
    }

    {
        const auto shader_code = read_binary_file("render.comp.spv");

        VkShaderModuleCreateInfo shader_module_create_info {};
        shader_module_create_info.sType =
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_module_create_info.codeSize =
            shader_code.size() * sizeof(std::uint32_t);
        shader_module_create_info.pCode = shader_code.data();

        VkShaderModule shader_module {};
        result = g.vkCreateShaderModule(
            g.device, &shader_module_create_info, nullptr, &shader_module);
        vk_check(result);

        VkPipelineShaderStageCreateInfo shader_stage_create_info {};
        shader_stage_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shader_stage_create_info.module = shader_module;
        shader_stage_create_info.pName = "main";

        VkPipelineLayoutCreateInfo pipeline_layout_create_info {};
        pipeline_layout_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = &g.descriptor_set_layout;

        result = g.vkCreatePipelineLayout(g.device,
                                          &pipeline_layout_create_info,
                                          nullptr,
                                          &g.compute_pipeline_layout);
        vk_check(result);

        VkComputePipelineCreateInfo compute_pipeline_create_info {};
        compute_pipeline_create_info.sType =
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compute_pipeline_create_info.stage = shader_stage_create_info;
        compute_pipeline_create_info.layout = g.compute_pipeline_layout;

        result = g.vkCreateComputePipelines(g.device,
                                            VK_NULL_HANDLE,
                                            1,
                                            &compute_pipeline_create_info,
                                            nullptr,
                                            &g.compute_pipeline);
        vk_check(result);

        g.vkDestroyShaderModule(g.device, shader_module, nullptr);
    }

    {
        VkAttachmentDescription color_attachment_description {};
        color_attachment_description.format = g.swapchain_format;
        color_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_description.stencilLoadOp =
            VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_description.stencilStoreOp =
            VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment_description.finalLayout =
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_reference {};
        color_attachment_reference.attachment = 0;
        color_attachment_reference.layout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass_description {};
        subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_description.colorAttachmentCount = 1;
        subpass_description.pColorAttachments = &color_attachment_reference;

        VkSubpassDependency subpass_dependency {};
        subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpass_dependency.dstSubpass = 0;
        subpass_dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.srcAccessMask = VK_ACCESS_NONE;
        subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_create_info {};
        render_pass_create_info.sType =
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = 1;
        render_pass_create_info.pAttachments = &color_attachment_description;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass_description;
        render_pass_create_info.dependencyCount = 1;
        render_pass_create_info.pDependencies = &subpass_dependency;

        result = g.vkCreateRenderPass(
            g.device, &render_pass_create_info, nullptr, &g.render_pass);
        vk_check(result);
    }

    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(g.window, true);

        const auto loader_func = [](const char *name, void *)
        { return g.vkGetInstanceProcAddr(g.instance, name); };
        ImGui_ImplVulkan_LoadFunctions(loader_func);

        ImGui_ImplVulkan_InitInfo init_info {};
        init_info.Instance = g.instance;
        init_info.PhysicalDevice = g.physical_device;
        init_info.Device = g.device;
        init_info.QueueFamily = g.queue_family;
        init_info.Queue = g.queue;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = g.descriptor_pool;
        init_info.Subpass = 0;
        init_info.MinImageCount = g.swapchain_min_image_count;
        init_info.ImageCount = g.swapchain_image_count;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = &imgui_vk_check;
        ImGui_ImplVulkan_Init(&init_info, g.render_pass);

        const auto command_buffer = begin_one_time_submit_command_buffer();
        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
        end_one_time_submit_command_buffer(command_buffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }
}

void shutdown()
{
    if (g.device)
    {
        const auto result = g.vkDeviceWaitIdle(g.device);
        vk_check(result);
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (g.device)
    {
        if (g.render_pass)
        {
            g.vkDestroyRenderPass(g.device, g.render_pass, nullptr);
        }

        if (g.compute_pipeline)
        {
            g.vkDestroyPipeline(g.device, g.compute_pipeline, nullptr);
        }

        if (g.compute_pipeline_layout)
        {
            g.vkDestroyPipelineLayout(
                g.device, g.compute_pipeline_layout, nullptr);
        }

        if (g.descriptor_pool)
        {
            g.vkDestroyDescriptorPool(g.device, g.descriptor_pool, nullptr);
        }

        if (g.descriptor_set_layout)
        {
            g.vkDestroyDescriptorSetLayout(
                g.device, g.descriptor_set_layout, nullptr);
        }

        if (g.command_pool)
        {
            g.vkDestroyCommandPool(g.device, g.command_pool, nullptr);
        }

        if (g.render_image_allocation)
        {
            vmaFreeMemory(g.allocator, g.render_image_allocation);
        }

        if (g.render_image_view)
        {
            g.vkDestroyImageView(g.device, g.render_image_view, nullptr);
        }

        if (g.render_image)
        {
            g.vkDestroyImage(g.device, g.render_image, nullptr);
        }
    }

    destroy_swapchain();

    if (g.allocator)
    {
        vmaDestroyAllocator(g.allocator);
    }

    if (g.device)
    {
        g.vkDestroyDevice(g.device, nullptr);
    }

    if (g.instance)
    {
        if (g.surface)
        {
            g.vkDestroySurfaceKHR(g.instance, g.surface, nullptr);
        }

#ifndef NDEBUG
        if (g.debug_messenger)
        {
            g.vkDestroyDebugUtilsMessengerEXT(
                g.instance, g.debug_messenger, nullptr);
        }
#endif

        g.vkDestroyInstance(g.instance, nullptr);
    }

    glfwDestroyWindow(g.window);
    glfwTerminate();

    g = {};
}

void run()
{
    {
        const auto command_buffer = begin_one_time_submit_command_buffer();

        VkImageSubresourceRange subresource_range {};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = 1;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        VkImageMemoryBarrier image_memory_barrier {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.srcAccessMask = VK_ACCESS_NONE;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.image = g.render_image;
        image_memory_barrier.subresourceRange = subresource_range;

        g.vkCmdPipelineBarrier(command_buffer,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               {},
                               0,
                               nullptr,
                               0,
                               nullptr,
                               1,
                               &image_memory_barrier);

        g.vkCmdBindDescriptorSets(command_buffer,
                                  VK_PIPELINE_BIND_POINT_COMPUTE,
                                  g.compute_pipeline_layout,
                                  0,
                                  1,
                                  &g.descriptor_set,
                                  0,
                                  nullptr);

        g.vkCmdBindPipeline(
            command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, g.compute_pipeline);

        constexpr std::uint32_t group_size_x {32};
        constexpr std::uint32_t group_size_y {32};
        constexpr std::uint32_t group_size_z {1};
        g.vkCmdDispatch(
            command_buffer, group_size_x, group_size_y, group_size_z);

        end_one_time_submit_command_buffer(command_buffer);
    }

    {
        VkBufferCreateInfo buffer_create_info {};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size =
            g.render_image_width * g.render_image_height * 4 * sizeof(float);
        buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        buffer_create_info.queueFamilyIndexCount = 1;
        buffer_create_info.pQueueFamilyIndices = &g.queue_family;

        VmaAllocationCreateInfo allocation_create_info {};
        allocation_create_info.flags =
            VMA_ALLOCATION_CREATE_MAPPED_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

        VkBuffer staging_buffer {};
        VmaAllocation staging_buffer_allocation {};
        VmaAllocationInfo staging_buffer_allocation_info {};
        vk_check(vmaCreateBuffer(g.allocator,
                                 &buffer_create_info,
                                 &allocation_create_info,
                                 &staging_buffer,
                                 &staging_buffer_allocation,
                                 &staging_buffer_allocation_info));

        const auto command_buffer = begin_one_time_submit_command_buffer();

        VkImageSubresourceRange subresource_range {};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = 1;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        VkImageMemoryBarrier image_memory_barrier {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.image = g.render_image;
        image_memory_barrier.subresourceRange = subresource_range;

        g.vkCmdPipelineBarrier(command_buffer,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               {},
                               0,
                               nullptr,
                               0,
                               nullptr,
                               1,
                               &image_memory_barrier);

        VkBufferImageCopy region {};
        region.bufferOffset = 0;
        region.bufferImageHeight = g.render_image_height;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {g.render_image_width, g.render_image_height, 1};

        g.vkCmdCopyImageToBuffer(command_buffer,
                                 g.render_image,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 staging_buffer,
                                 1,
                                 &region);

        end_one_time_submit_command_buffer(command_buffer);

        const auto *const hdr_image_data = reinterpret_cast<float *>(
            staging_buffer_allocation_info.pMappedData);
        const auto image_size =
            g.render_image_width * g.render_image_height * 4;
        std::vector<std::uint8_t> rgba8_image_data(image_size);
        for (std::uint32_t i {}; i < image_size; ++i)
        {
            rgba8_image_data[i] =
                static_cast<std::uint8_t>(hdr_image_data[i] * 255.0f);
        }

        if (!stbi_write_png("image.png",
                            static_cast<int>(g.render_image_width),
                            static_cast<int>(g.render_image_height),
                            4,
                            rgba8_image_data.data(),
                            static_cast<int>(g.render_image_width * 4)))
        {
            fatal_error("Failed to write PNG image");
        }

        vmaDestroyBuffer(
            g.allocator, staging_buffer, staging_buffer_allocation);
    }

    while (!glfwWindowShouldClose(g.window))
    {
        glfwPollEvents();
    }
}

} // namespace

int main()
{
    try
    {
        init();
        run();
        shutdown();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception thrown\n";
        shutdown();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
