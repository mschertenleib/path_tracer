#include "vulkan_renderer.hpp"

#include "tiny_obj_loader.h"

#include "stb_image_write.h"

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

#include <array>
#include <cassert>
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

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace
{

#define GLOBAL_FUNCTIONS_MAP(MACRO)                                            \
    MACRO(vkGetInstanceProcAddr)                                               \
    MACRO(vkEnumerateInstanceLayerProperties)                                  \
    MACRO(vkEnumerateInstanceExtensionProperties)                              \
    MACRO(vkCreateInstance)

#ifndef NDEBUG
#define INSTANCE_FUNCTIONS_DEBUG_MAP(MACRO)                                    \
    MACRO(vkCreateDebugUtilsMessengerEXT)                                      \
    MACRO(vkDestroyDebugUtilsMessengerEXT)
#else
#define INSTANCE_FUNCTIONS_DEBUG_MAP(MACRO)
#endif

#define INSTANCE_FUNCTIONS_MAP(MACRO)                                          \
    INSTANCE_FUNCTIONS_DEBUG_MAP(MACRO)                                        \
    MACRO(vkDestroyInstance)                                                   \
    MACRO(vkDestroySurfaceKHR)                                                 \
    MACRO(vkEnumeratePhysicalDevices)                                          \
    MACRO(vkEnumerateDeviceExtensionProperties)                                \
    MACRO(vkGetPhysicalDeviceQueueFamilyProperties)                            \
    MACRO(vkGetPhysicalDeviceFeatures2)                                        \
    MACRO(vkGetPhysicalDeviceSurfaceFormatsKHR)                                \
    MACRO(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)                           \
    MACRO(vkCreateDevice)                                                      \
    MACRO(vkGetDeviceProcAddr)

#define DEVICE_FUNCTIONS_MAP(MACRO)                                            \
    MACRO(vkDestroyDevice)                                                     \
    MACRO(vkGetDeviceQueue)                                                    \
    MACRO(vkCreateSwapchainKHR)                                                \
    MACRO(vkDestroySwapchainKHR)                                               \
    MACRO(vkGetSwapchainImagesKHR)                                             \
    MACRO(vkCreateImageView)                                                   \
    MACRO(vkDestroyImageView)                                                  \
    MACRO(vkCreateSampler)                                                     \
    MACRO(vkDestroySampler)                                                    \
    MACRO(vkDestroyImage)                                                      \
    MACRO(vkCreateCommandPool)                                                 \
    MACRO(vkDestroyCommandPool)                                                \
    MACRO(vkAllocateCommandBuffers)                                            \
    MACRO(vkFreeCommandBuffers)                                                \
    MACRO(vkBeginCommandBuffer)                                                \
    MACRO(vkEndCommandBuffer)                                                  \
    MACRO(vkCmdPipelineBarrier)                                                \
    MACRO(vkCmdCopyImageToBuffer)                                              \
    MACRO(vkCmdBindDescriptorSets)                                             \
    MACRO(vkCmdBindPipeline)                                                   \
    MACRO(vkCmdPushConstants)                                                  \
    MACRO(vkCmdDispatch)                                                       \
    MACRO(vkCmdBeginRenderPass)                                                \
    MACRO(vkCmdEndRenderPass)                                                  \
    MACRO(vkCmdBlitImage)                                                      \
    MACRO(vkCmdCopyBuffer)                                                     \
    MACRO(vkQueueSubmit)                                                       \
    MACRO(vkQueueWaitIdle)                                                     \
    MACRO(vkDeviceWaitIdle)                                                    \
    MACRO(vkCreateDescriptorSetLayout)                                         \
    MACRO(vkDestroyDescriptorSetLayout)                                        \
    MACRO(vkCreateDescriptorPool)                                              \
    MACRO(vkDestroyDescriptorPool)                                             \
    MACRO(vkAllocateDescriptorSets)                                            \
    MACRO(vkUpdateDescriptorSets)                                              \
    MACRO(vkCreateShaderModule)                                                \
    MACRO(vkDestroyShaderModule)                                               \
    MACRO(vkCreatePipelineLayout)                                              \
    MACRO(vkDestroyPipelineLayout)                                             \
    MACRO(vkCreateComputePipelines)                                            \
    MACRO(vkDestroyPipeline)                                                   \
    MACRO(vkCreateRenderPass)                                                  \
    MACRO(vkDestroyRenderPass)                                                 \
    MACRO(vkCreateFramebuffer)                                                 \
    MACRO(vkDestroyFramebuffer)                                                \
    MACRO(vkCreateSemaphore)                                                   \
    MACRO(vkDestroySemaphore)                                                  \
    MACRO(vkCreateFence)                                                       \
    MACRO(vkDestroyFence)                                                      \
    MACRO(vkWaitForFences)                                                     \
    MACRO(vkAcquireNextImageKHR)                                               \
    MACRO(vkResetFences)                                                       \
    MACRO(vkResetCommandBuffer)                                                \
    MACRO(vkQueuePresentKHR)

#define DECLARE_FUNCTION(name) PFN_##name name {};
GLOBAL_FUNCTIONS_MAP(DECLARE_FUNCTION)
INSTANCE_FUNCTIONS_MAP(DECLARE_FUNCTION)
DEVICE_FUNCTIONS_MAP(DECLARE_FUNCTION)
#undef DECLARE_FUNCTION

struct Vulkan_buffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VkDeviceSize size;
};

struct Vulkan_image
{
    VkImage image;
    VmaAllocation allocation;
    VkImageView image_view;
    std::uint32_t width;
    std::uint32_t height;
};

struct Vulkan_swapchain
{
    VkSwapchainKHR swapchain;
    VkFormat format;
    VkExtent2D extent;
    std::uint32_t image_count;
    std::uint32_t min_image_count;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
};

inline constexpr std::uint32_t frames_in_flight {2};

struct
{
    GLFWwindow *window;

    VkInstance instance;

#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    std::uint32_t queue_family {std::numeric_limits<std::uint32_t>::max()};
    VkDevice device;

    VkQueue queue;
    VmaAllocator allocator;
    Vulkan_swapchain swapchain;
    Vulkan_image storage_image;
    Vulkan_image render_target;
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
    std::array<VkCommandBuffer, frames_in_flight> frame_command_buffers;
    std::array<VkSemaphore, frames_in_flight> image_available_semaphores;
    std::array<VkSemaphore, frames_in_flight> render_finished_semaphores;
    std::array<VkFence, frames_in_flight> in_flight_fences;
    std::uint32_t current_frame;
    bool framebuffer_resized;

    Vulkan_buffer vertex_buffer;
    Vulkan_buffer index_buffer;
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

void glfw_framebuffer_size_callback([[maybe_unused]] GLFWwindow *window,
                                    [[maybe_unused]] int width,
                                    [[maybe_unused]] int height)
{
    g.framebuffer_resized = true;
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

void load_global_commands()
{
    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr"));
    assert(vkGetInstanceProcAddr);

#define LOAD_FUNCTION(func)                                                    \
    func =                                                                     \
        reinterpret_cast<PFN_##func>(vkGetInstanceProcAddr(nullptr, #func));   \
    assert(func);

    GLOBAL_FUNCTIONS_MAP(LOAD_FUNCTION)

#undef LOAD_FUNCTION
}

void load_instance_commands()
{
    assert(vkGetInstanceProcAddr);
    assert(g.instance);

#define LOAD_FUNCTION(func)                                                    \
    func = reinterpret_cast<PFN_##func>(                                       \
        vkGetInstanceProcAddr(g.instance, #func));                             \
    assert(func);

    INSTANCE_FUNCTIONS_MAP(LOAD_FUNCTION)

#undef LOAD_FUNCTION
}

void load_device_commands()
{
    assert(vkGetDeviceProcAddr);
    assert(g.device);

#define LOAD_FUNCTION(func)                                                    \
    func = reinterpret_cast<PFN_##func>(vkGetDeviceProcAddr(g.device, #func)); \
    assert(func);

    DEVICE_FUNCTIONS_MAP(LOAD_FUNCTION)

#undef LOAD_FUNCTION
}

[[nodiscard]] Vulkan_buffer create_buffer(VmaAllocator allocator,
                                          VkDeviceSize size,
                                          std::uint32_t queue_family_index,
                                          VkBufferUsageFlags buffer_usage,
                                          VmaAllocationCreateFlags flags,
                                          VmaMemoryUsage memory_usage,
                                          VmaAllocationInfo *allocation_info)
{
    Vulkan_buffer buffer {};
    buffer.size = size;

    VkBufferCreateInfo buffer_create_info {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 1;
    buffer_create_info.pQueueFamilyIndices = &queue_family_index;
    buffer_create_info.size = size;
    buffer_create_info.usage = buffer_usage;

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.flags = flags;
    allocation_create_info.usage = memory_usage;

    const auto result = vmaCreateBuffer(allocator,
                                        &buffer_create_info,
                                        &allocation_create_info,
                                        &buffer.buffer,
                                        &buffer.allocation,
                                        allocation_info);
    vk_check(result);

    return buffer;
}

void destroy_buffer(VmaAllocator allocator, Vulkan_buffer &buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    buffer = {};
}

[[nodiscard]] Vulkan_buffer
create_staging_buffer(VmaAllocator allocator,
                      VkDeviceSize size,
                      std::uint32_t queue_family_index,
                      VmaAllocationInfo *allocation_info)
{
    return create_buffer(
        allocator,
        size,
        queue_family_index,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_ALLOCATION_CREATE_MAPPED_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        VMA_MEMORY_USAGE_AUTO,
        allocation_info);
}

void get_queue_families(VkPhysicalDevice physical_device,
                        std::uint32_t &queue_family)
{
    queue_family = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t property_count {};
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &property_count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
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
    result = vkEnumerateDeviceExtensionProperties(
        physical_device, nullptr, &extension_property_count, nullptr);
    vk_check(result);
    std::vector<VkExtensionProperties> extension_properties(
        extension_property_count);
    result = vkEnumerateDeviceExtensionProperties(physical_device,
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

    vkGetPhysicalDeviceFeatures2(physical_device, &features);

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
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        g.physical_device, g.surface, &surface_format_count, nullptr);
    vk_check(result);
    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(g.physical_device,
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
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        g.physical_device, g.surface, &surface_capabilities);
    vk_check(result);

    if (surface_capabilities.currentExtent.width !=
        std::numeric_limits<std::uint32_t>::max())
    {
        g.swapchain.extent = surface_capabilities.currentExtent;
    }
    else
    {
        int framebuffer_width;
        int framebuffer_height;
        glfwGetFramebufferSize(
            g.window, &framebuffer_width, &framebuffer_height);
        g.swapchain.extent = {static_cast<std::uint32_t>(framebuffer_width),
                              static_cast<std::uint32_t>(framebuffer_height)};
    }
    g.swapchain.extent.width =
        std::clamp(g.swapchain.extent.width,
                   surface_capabilities.minImageExtent.width,
                   surface_capabilities.maxImageExtent.width);
    g.swapchain.extent.height =
        std::clamp(g.swapchain.extent.height,
                   surface_capabilities.minImageExtent.height,
                   surface_capabilities.maxImageExtent.height);

    g.swapchain.min_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 &&
        g.swapchain.min_image_count > surface_capabilities.maxImageCount)
    {
        g.swapchain.min_image_count = surface_capabilities.maxImageCount;
    }

    g.swapchain.format = surface_format.format;

    VkSwapchainCreateInfoKHR swapchain_create_info {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = g.surface;
    swapchain_create_info.minImageCount = g.swapchain.min_image_count;
    swapchain_create_info.imageFormat = surface_format.format;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = g.swapchain.extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.preTransform = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 1;
    swapchain_create_info.pQueueFamilyIndices = &g.queue_family;

    result = vkCreateSwapchainKHR(
        g.device, &swapchain_create_info, nullptr, &g.swapchain.swapchain);
    vk_check(result);

    result = vkGetSwapchainImagesKHR(
        g.device, g.swapchain.swapchain, &g.swapchain.image_count, nullptr);
    vk_check(result);
    g.swapchain.images.resize(g.swapchain.image_count);
    result = vkGetSwapchainImagesKHR(g.device,
                                     g.swapchain.swapchain,
                                     &g.swapchain.image_count,
                                     g.swapchain.images.data());
    vk_check(result);

    g.swapchain.image_views.resize(g.swapchain.image_count);

    VkImageViewCreateInfo image_view_create_info {};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = g.swapchain.format;
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    for (std::uint32_t i {}; i < g.swapchain.image_count; ++i)
    {
        image_view_create_info.image = g.swapchain.images[i];
        result = vkCreateImageView(g.device,
                                   &image_view_create_info,
                                   nullptr,
                                   &g.swapchain.image_views[i]);
        vk_check(result);
    }
}

void destroy_swapchain()
{
    if (!g.device)
    {
        return;
    }

    for (const auto image_view : g.swapchain.image_views)
    {
        vkDestroyImageView(g.device, image_view, nullptr);
    }

    if (g.swapchain.swapchain)
    {
        vkDestroySwapchainKHR(g.device, g.swapchain.swapchain, nullptr);
    }
}

void create_framebuffers()
{
    g.framebuffers.resize(g.swapchain.image_count);

    VkFramebufferCreateInfo framebuffer_create_info {};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = g.render_pass;
    framebuffer_create_info.attachmentCount = 1;
    framebuffer_create_info.width = g.swapchain.extent.width;
    framebuffer_create_info.height = g.swapchain.extent.height;
    framebuffer_create_info.layers = 1;

    for (std::uint32_t i {}; i < g.swapchain.image_count; ++i)
    {
        framebuffer_create_info.pAttachments = &g.swapchain.image_views[i];

        const auto result = vkCreateFramebuffer(
            g.device, &framebuffer_create_info, nullptr, &g.framebuffers[i]);
        vk_check(result);
    }
}

void destroy_framebuffers()
{
    for (std::uint32_t i {}; i < g.swapchain.image_count; ++i)
    {
        if (g.framebuffers[i])
        {
            vkDestroyFramebuffer(g.device, g.framebuffers[i], nullptr);
        }
    }
}

void recreate_swapchain()
{
    int width {};
    int height {};
    glfwGetFramebufferSize(g.window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(g.window, &width, &height);
        glfwWaitEvents();
    }

    const auto result = vkDeviceWaitIdle(g.device);
    vk_check(result);

    destroy_framebuffers();
    destroy_swapchain();

    create_swapchain();
    create_framebuffers();
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
    result = vkAllocateCommandBuffers(
        g.device, &command_buffer_allocate_info, &command_buffer);
    vk_check(result);

    VkCommandBufferBeginInfo command_buffer_begin_info {};
    command_buffer_begin_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
    vk_check(result);

    return command_buffer;
}

void end_one_time_submit_command_buffer(VkCommandBuffer command_buffer)
{
    VkResult result;

    result = vkEndCommandBuffer(command_buffer);
    vk_check(result);

    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    result = vkQueueSubmit(g.queue, 1, &submit_info, VK_NULL_HANDLE);
    vk_check(result);

    result = vkQueueWaitIdle(g.queue);
    vk_check(result);
}

void draw_frame(std::uint32_t rng_seed)
{
    VkResult result;

    result = vkWaitForFences(g.device,
                             1,
                             &g.in_flight_fences[g.current_frame],
                             VK_TRUE,
                             std::numeric_limits<std::uint64_t>::max());
    vk_check(result);

    std::uint32_t image_index;
    result =
        vkAcquireNextImageKHR(g.device,
                              g.swapchain.swapchain,
                              std::numeric_limits<std::uint64_t>::max(),
                              g.image_available_semaphores[g.current_frame],
                              VK_NULL_HANDLE,
                              &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate_swapchain();
        return;
    }
    else if (result != VK_SUBOPTIMAL_KHR)
    {
        vk_check(result);
    }

    result = vkResetFences(g.device, 1, &g.in_flight_fences[g.current_frame]);
    vk_check(result);

    const auto command_buffer = g.frame_command_buffers[g.current_frame];

    result = vkResetCommandBuffer(command_buffer, {});
    vk_check(result);

    VkCommandBufferBeginInfo command_buffer_begin_info {};
    command_buffer_begin_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    result = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
    vk_check(result);

    vkCmdBindDescriptorSets(command_buffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            g.compute_pipeline_layout,
                            0,
                            1,
                            &g.storage_image_descriptor_set,
                            0,
                            nullptr);

    vkCmdBindPipeline(
        command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, g.compute_pipeline);

    vkCmdPushConstants(command_buffer,
                       g.compute_pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(std::uint32_t),
                       &rng_seed);

    constexpr std::uint32_t group_size_x {32};
    constexpr std::uint32_t group_size_y {32};
    constexpr std::uint32_t group_size_z {1};
    vkCmdDispatch(command_buffer, group_size_x, group_size_y, group_size_z);

    VkImageMemoryBarrier image_memory_barriers[2] {};

    image_memory_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barriers[0].srcAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    image_memory_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_memory_barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_memory_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_memory_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barriers[0].image = g.storage_image.image;
    image_memory_barriers[0].subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barriers[0].subresourceRange.baseMipLevel = 0;
    image_memory_barriers[0].subresourceRange.levelCount = 1;
    image_memory_barriers[0].subresourceRange.baseArrayLayer = 0;
    image_memory_barriers[0].subresourceRange.layerCount = 1;

    image_memory_barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    image_memory_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_memory_barriers[1].oldLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_memory_barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_memory_barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barriers[1].image = g.render_target.image;
    image_memory_barriers[1].subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barriers[1].subresourceRange.baseMipLevel = 0;
    image_memory_barriers[1].subresourceRange.levelCount = 1;
    image_memory_barriers[1].subresourceRange.baseArrayLayer = 0;
    image_memory_barriers[1].subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         {},
                         0,
                         nullptr,
                         0,
                         nullptr,
                         2,
                         image_memory_barriers);

    VkImageBlit image_blit {};
    image_blit.srcOffsets[0] = {0, 0, 0};
    image_blit.srcOffsets[1] = {
        static_cast<std::int32_t>(g.storage_image.width),
        static_cast<std::int32_t>(g.storage_image.height),
        1};
    image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit.srcSubresource.mipLevel = 0;
    image_blit.srcSubresource.baseArrayLayer = 0;
    image_blit.srcSubresource.layerCount = 1;
    image_blit.dstOffsets[0] = {0, 0, 0};
    image_blit.dstOffsets[1] = {
        static_cast<std::int32_t>(g.storage_image.width),
        static_cast<std::int32_t>(g.storage_image.height),
        1};
    image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit.dstSubresource.mipLevel = 0;
    image_blit.dstSubresource.baseArrayLayer = 0;
    image_blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(command_buffer,
                   g.storage_image.image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   g.render_target.image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &image_blit,
                   VK_FILTER_NEAREST);

    image_memory_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_memory_barriers[0].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    image_memory_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_memory_barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_memory_barriers[0].image = g.storage_image.image;

    image_memory_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_memory_barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    image_memory_barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_memory_barriers[1].newLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_memory_barriers[1].image = g.render_target.image;

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         {},
                         0,
                         nullptr,
                         0,
                         nullptr,
                         2,
                         image_memory_barriers);

    const VkClearValue clear_value {
        .color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderPassBeginInfo render_pass_begin_info {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = g.render_pass;
    render_pass_begin_info.framebuffer = g.framebuffers[image_index];
    render_pass_begin_info.renderArea.offset = {0, 0};
    render_pass_begin_info.renderArea.extent = g.swapchain.extent;
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(
        command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);

    vkCmdEndRenderPass(command_buffer);

    result = vkEndCommandBuffer(command_buffer);
    vk_check(result);

    const VkPipelineStageFlags wait_stage {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores =
        &g.image_available_semaphores[g.current_frame];
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores =
        &g.render_finished_semaphores[g.current_frame];

    result = vkQueueSubmit(
        g.queue, 1, &submit_info, g.in_flight_fences[g.current_frame]);
    vk_check(result);

    VkPresentInfoKHR present_info {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores =
        &g.render_finished_semaphores[g.current_frame];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g.swapchain.swapchain;
    present_info.pImageIndices = &image_index;

    result = vkQueuePresentKHR(g.queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        g.framebuffer_resized)
    {
        g.framebuffer_resized = false;
        recreate_swapchain();
    }
    else
    {
        vk_check(result);
    }

    g.current_frame = (g.current_frame + 1) % frames_in_flight;
}

void store_to_png(const char *filename)
{
    VkBufferCreateInfo buffer_create_info {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size =
        g.storage_image.width * g.storage_image.height * 4;
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

    VkImageMemoryBarrier image_memory_barrier {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = g.render_target.image;
    image_memory_barrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barrier.subresourceRange.baseMipLevel = 0;
    image_memory_barrier.subresourceRange.levelCount = 1;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    image_memory_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
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
    region.bufferImageHeight = g.storage_image.height;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {g.storage_image.width, g.storage_image.height, 1};

    vkCmdCopyImageToBuffer(command_buffer,
                           g.render_target.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           staging_buffer,
                           1,
                           &region);

    image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         {},
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &image_memory_barrier);

    end_one_time_submit_command_buffer(command_buffer);

    if (!stbi_write_png(filename,
                        static_cast<int>(g.storage_image.width),
                        static_cast<int>(g.storage_image.height),
                        4,
                        staging_buffer_allocation_info.pMappedData,
                        static_cast<int>(g.storage_image.width * 4)))
    {
        fatal_error("Failed to write PNG image");
    }

    vmaDestroyBuffer(g.allocator, staging_buffer, staging_buffer_allocation);
}

} // namespace

Unique_allocator::Unique_allocator(VmaAllocator allocator) noexcept
    : m_allocator {allocator}
{
    assert(m_allocator);
}

Unique_allocator::~Unique_allocator() noexcept
{
    if (m_allocator)
    {
        vmaDestroyAllocator(m_allocator);
    }
}

Unique_allocation::Unique_allocation(VmaAllocation allocation,
                                     VmaAllocator allocator) noexcept
    : m_allocation {allocation}, m_allocator {allocator}
{
    assert(m_allocation);
    assert(m_allocator);
}

Unique_allocation::~Unique_allocation() noexcept
{
    if (m_allocation)
    {
        vmaFreeMemory(m_allocator, m_allocation);
    }
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

        glfwSetFramebufferSizeCallback(g.window,
                                       glfw_framebuffer_size_callback);
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
        result =
            vkEnumerateInstanceLayerProperties(&layer_property_count, nullptr);
        vk_check(result);
        std::vector<VkLayerProperties> layer_properties(layer_property_count);
        result = vkEnumerateInstanceLayerProperties(&layer_property_count,
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
        result = vkEnumerateInstanceExtensionProperties(
            nullptr, &extension_property_count, nullptr);
        vk_check(result);
        std::vector<VkExtensionProperties> extension_properties(
            extension_property_count);
        result = vkEnumerateInstanceExtensionProperties(
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

        result = vkCreateInstance(&instance_create_info, nullptr, &g.instance);
        vk_check(result);

        load_instance_commands();

        result = vkCreateDebugUtilsMessengerEXT(g.instance,
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

        result = vkCreateInstance(&instance_create_info, nullptr, &g.instance);
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
        result = vkEnumeratePhysicalDevices(
            g.instance, &physical_device_count, nullptr);
        vk_check(result);
        if (physical_device_count == 0)
        {
            fatal_error("Failed to find a physical device with Vulkan support");
        }

        std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
        result = vkEnumeratePhysicalDevices(
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

    get_queue_families(g.physical_device, g.queue_family);

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

        result = vkCreateDevice(
            g.physical_device, &device_create_info, nullptr, &g.device);
        vk_check(result);
    }

    load_device_commands();

    {
        vkGetDeviceQueue(g.device, g.queue_family, 0, &g.queue);
    }

    {
        VmaVulkanFunctions vulkan_functions {};
        vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

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
        VkCommandPoolCreateInfo command_pool_create_info {};
        command_pool_create_info.sType =
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.flags =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_create_info.queueFamilyIndex = g.queue_family;
        result = vkCreateCommandPool(
            g.device, &command_pool_create_info, nullptr, &g.command_pool);
        vk_check(result);
    }

    {
        g.storage_image.width = 160;
        g.storage_image.height = 90;

        VkImageCreateInfo image_create_info {};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        image_create_info.extent.width = g.storage_image.width;
        image_create_info.extent.height = g.storage_image.height;
        image_create_info.extent.depth = 1;
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage =
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.queueFamilyIndexCount = 1;
        image_create_info.pQueueFamilyIndices = &g.queue_family;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocation_create_info {};
        allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        result = vmaCreateImage(g.allocator,
                                &image_create_info,
                                &allocation_create_info,
                                &g.storage_image.image,
                                &g.storage_image.allocation,
                                nullptr);
        vk_check(result);

        VkImageViewCreateInfo image_view_create_info {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = g.storage_image.image;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(g.device,
                                   &image_view_create_info,
                                   nullptr,
                                   &g.storage_image.image_view);
        vk_check(result);

        const auto command_buffer = begin_one_time_submit_command_buffer();

        VkImageMemoryBarrier image_memory_barrier {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.srcAccessMask = VK_ACCESS_NONE;
        image_memory_barrier.dstAccessMask =
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.image = g.storage_image.image;
        image_memory_barrier.subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_COLOR_BIT;
        image_memory_barrier.subresourceRange.baseMipLevel = 0;
        image_memory_barrier.subresourceRange.levelCount = 1;
        image_memory_barrier.subresourceRange.baseArrayLayer = 0;
        image_memory_barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(command_buffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             {},
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &image_memory_barrier);

        end_one_time_submit_command_buffer(command_buffer);
    }

    {
        VkImageCreateInfo image_create_info {};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
        image_create_info.extent.width = g.storage_image.width;
        image_create_info.extent.height = g.storage_image.height;
        image_create_info.extent.depth = 1;
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.queueFamilyIndexCount = 1;
        image_create_info.pQueueFamilyIndices = &g.queue_family;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocation_create_info {};
        allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        result = vmaCreateImage(g.allocator,
                                &image_create_info,
                                &allocation_create_info,
                                &g.render_target.image,
                                &g.render_target.allocation,
                                nullptr);
        vk_check(result);

        VkImageViewCreateInfo image_view_create_info {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = g.render_target.image;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(g.device,
                                   &image_view_create_info,
                                   nullptr,
                                   &g.render_target.image_view);
        vk_check(result);

        VkSamplerCreateInfo sampler_create_info {};
        sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_create_info.magFilter = VK_FILTER_NEAREST;
        sampler_create_info.minFilter = VK_FILTER_NEAREST;
        sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_create_info.anisotropyEnable = VK_FALSE;
        sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_create_info.unnormalizedCoordinates = VK_FALSE;
        sampler_create_info.compareEnable = VK_FALSE;
        sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_create_info.minLod = 0.0f;
        sampler_create_info.maxLod = 0.0f;
        sampler_create_info.mipLodBias = 0.0f;

        result = vkCreateSampler(
            g.device, &sampler_create_info, nullptr, &g.render_target_sampler);
        vk_check(result);

        const auto command_buffer = begin_one_time_submit_command_buffer();

        VkImageMemoryBarrier image_memory_barrier {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.srcAccessMask = VK_ACCESS_NONE;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_memory_barrier.newLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.image = g.render_target.image;
        image_memory_barrier.subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_COLOR_BIT;
        image_memory_barrier.subresourceRange.baseMipLevel = 0;
        image_memory_barrier.subresourceRange.levelCount = 1;
        image_memory_barrier.subresourceRange.baseArrayLayer = 0;
        image_memory_barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(command_buffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             {},
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &image_memory_barrier);

        end_one_time_submit_command_buffer(command_buffer);
    }

    {
        tinyobj::ObjReader reader;
        reader.ParseFromFile("../resources/bunny.obj");
        if (!reader.Valid())
        {
            fatal_error("Failed to load OBJ file");
        }

        const auto &vertices = reader.GetAttrib().GetVertices();
        const auto &shapes = reader.GetShapes();
        if (shapes.size() != 1)
        {
            fatal_error("OBJ file contains more than one shape");
        }
        const auto &shape = shapes.front();
        std::vector<std::uint32_t> indices;
        indices.reserve(shape.mesh.indices.size());
        for (const auto &index : shape.mesh.indices)
        {
            indices.push_back(static_cast<std::uint32_t>(index.vertex_index));
        }

        {
            g.vertex_buffer.size = vertices.size() * sizeof(tinyobj::real_t);

            VkBufferCreateInfo buffer_create_info {};
            buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            buffer_create_info.queueFamilyIndexCount = 1;
            buffer_create_info.pQueueFamilyIndices = &g.queue_family;
            buffer_create_info.size = g.vertex_buffer.size;
            buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo allocation_create_info {};
            allocation_create_info.flags =
                VMA_ALLOCATION_CREATE_MAPPED_BIT |
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

            VkBuffer staging_buffer {};
            VmaAllocation staging_buffer_allocation {};
            VmaAllocationInfo staging_buffer_allocation_info {};
            result = vmaCreateBuffer(g.allocator,
                                     &buffer_create_info,
                                     &allocation_create_info,
                                     &staging_buffer,
                                     &staging_buffer_allocation,
                                     &staging_buffer_allocation_info);
            vk_check(result);

            buffer_create_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            allocation_create_info.flags = {};
            allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            result = vmaCreateBuffer(g.allocator,
                                     &buffer_create_info,
                                     &allocation_create_info,
                                     &g.vertex_buffer.buffer,
                                     &g.vertex_buffer.allocation,
                                     nullptr);
            vk_check(result);

            std::memcpy(staging_buffer_allocation_info.pMappedData,
                        vertices.data(),
                        g.vertex_buffer.size);

            const auto command_buffer = begin_one_time_submit_command_buffer();

            const VkBufferCopy region {
                .srcOffset = 0, .dstOffset = 0, .size = g.vertex_buffer.size};

            vkCmdCopyBuffer(command_buffer,
                            staging_buffer,
                            g.vertex_buffer.buffer,
                            1,
                            &region);

            end_one_time_submit_command_buffer(command_buffer);

            vmaDestroyBuffer(
                g.allocator, staging_buffer, staging_buffer_allocation);
        }

        {
            g.index_buffer.size = indices.size() * sizeof(std::uint32_t);

            VkBufferCreateInfo buffer_create_info {};
            buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            buffer_create_info.queueFamilyIndexCount = 1;
            buffer_create_info.pQueueFamilyIndices = &g.queue_family;
            buffer_create_info.size = g.index_buffer.size;
            buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo allocation_create_info {};
            allocation_create_info.flags =
                VMA_ALLOCATION_CREATE_MAPPED_BIT |
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

            VkBuffer staging_buffer {};
            VmaAllocation staging_buffer_allocation {};
            VmaAllocationInfo staging_buffer_allocation_info {};
            result = vmaCreateBuffer(g.allocator,
                                     &buffer_create_info,
                                     &allocation_create_info,
                                     &staging_buffer,
                                     &staging_buffer_allocation,
                                     &staging_buffer_allocation_info);
            vk_check(result);

            buffer_create_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            allocation_create_info.flags = {};
            allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            result = vmaCreateBuffer(g.allocator,
                                     &buffer_create_info,
                                     &allocation_create_info,
                                     &g.index_buffer.buffer,
                                     &g.index_buffer.allocation,
                                     nullptr);
            vk_check(result);

            std::memcpy(staging_buffer_allocation_info.pMappedData,
                        indices.data(),
                        g.index_buffer.size);

            const auto command_buffer = begin_one_time_submit_command_buffer();

            const VkBufferCopy region {
                .srcOffset = 0, .dstOffset = 0, .size = g.index_buffer.size};

            vkCmdCopyBuffer(command_buffer,
                            staging_buffer,
                            g.index_buffer.buffer,
                            1,
                            &region);

            end_one_time_submit_command_buffer(command_buffer);

            vmaDestroyBuffer(
                g.allocator, staging_buffer, staging_buffer_allocation);
        }
    }

    {
        VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[3] {};

        descriptor_set_layout_bindings[0].binding = 0;
        descriptor_set_layout_bindings[0].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptor_set_layout_bindings[0].descriptorCount = 1;
        descriptor_set_layout_bindings[0].stageFlags =
            VK_SHADER_STAGE_COMPUTE_BIT;

        descriptor_set_layout_bindings[1].binding = 2;
        descriptor_set_layout_bindings[1].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptor_set_layout_bindings[1].descriptorCount = 1;
        descriptor_set_layout_bindings[1].stageFlags =
            VK_SHADER_STAGE_COMPUTE_BIT;

        descriptor_set_layout_bindings[2].binding = 3;
        descriptor_set_layout_bindings[2].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptor_set_layout_bindings[2].descriptorCount = 1;
        descriptor_set_layout_bindings[2].stageFlags =
            VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info {};
        descriptor_set_layout_create_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_create_info.bindingCount = 3;
        descriptor_set_layout_create_info.pBindings =
            descriptor_set_layout_bindings;

        result =
            vkCreateDescriptorSetLayout(g.device,
                                        &descriptor_set_layout_create_info,
                                        nullptr,
                                        &g.storage_image_descriptor_set_layout);
        vk_check(result);
    }

    {
        VkDescriptorSetLayoutBinding descriptor_set_layout_binding {};
        descriptor_set_layout_binding.binding = 0;
        descriptor_set_layout_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_set_layout_binding.descriptorCount = 1;
        descriptor_set_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info {};
        descriptor_set_layout_create_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_create_info.bindingCount = 1;
        descriptor_set_layout_create_info.pBindings =
            &descriptor_set_layout_binding;

        result =
            vkCreateDescriptorSetLayout(g.device,
                                        &descriptor_set_layout_create_info,
                                        nullptr,
                                        &g.render_target_descriptor_set_layout);
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
        descriptor_pool_create_info.maxSets =
            1000 * static_cast<std::uint32_t>(std::size(pool_sizes));
        descriptor_pool_create_info.poolSizeCount =
            static_cast<std::uint32_t>(std::size(pool_sizes));
        descriptor_pool_create_info.pPoolSizes = pool_sizes;

        result = vkCreateDescriptorPool(g.device,
                                        &descriptor_pool_create_info,
                                        nullptr,
                                        &g.descriptor_pool);
        vk_check(result);
    }

    {
        const VkDescriptorSetLayout set_layouts[] {
            g.storage_image_descriptor_set_layout,
            g.render_target_descriptor_set_layout};

        VkDescriptorSetAllocateInfo descriptor_set_allocate_info {};
        descriptor_set_allocate_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.descriptorPool = g.descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 2;
        descriptor_set_allocate_info.pSetLayouts = set_layouts;

        VkDescriptorSet sets[2];
        result = vkAllocateDescriptorSets(
            g.device, &descriptor_set_allocate_info, sets);
        vk_check(result);

        g.storage_image_descriptor_set = sets[0];
        g.render_target_descriptor_set = sets[1];

        VkDescriptorImageInfo storage_image_descriptor_image_info {};
        storage_image_descriptor_image_info.sampler = VK_NULL_HANDLE;
        storage_image_descriptor_image_info.imageView =
            g.storage_image.image_view;
        storage_image_descriptor_image_info.imageLayout =
            VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorBufferInfo vertex_buffer_descriptor_buffer_info {};
        vertex_buffer_descriptor_buffer_info.buffer = g.vertex_buffer.buffer;
        vertex_buffer_descriptor_buffer_info.offset = 0;
        vertex_buffer_descriptor_buffer_info.range = g.vertex_buffer.size;

        VkDescriptorBufferInfo index_buffer_descriptor_buffer_info {};
        index_buffer_descriptor_buffer_info.buffer = g.index_buffer.buffer;
        index_buffer_descriptor_buffer_info.offset = 0;
        index_buffer_descriptor_buffer_info.range = g.index_buffer.size;

        VkDescriptorImageInfo render_target_descriptor_image_info {};
        render_target_descriptor_image_info.sampler = g.render_target_sampler;
        render_target_descriptor_image_info.imageView =
            g.render_target.image_view;
        render_target_descriptor_image_info.imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet descriptor_writes[4] {};
        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = g.storage_image_descriptor_set;
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptor_writes[0].pImageInfo = &storage_image_descriptor_image_info;

        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet = g.storage_image_descriptor_set;
        descriptor_writes[1].dstBinding = 2;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptor_writes[1].pBufferInfo =
            &vertex_buffer_descriptor_buffer_info;

        descriptor_writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[2].dstSet = g.storage_image_descriptor_set;
        descriptor_writes[2].dstBinding = 3;
        descriptor_writes[2].dstArrayElement = 0;
        descriptor_writes[2].descriptorCount = 1;
        descriptor_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptor_writes[2].pBufferInfo = &index_buffer_descriptor_buffer_info;

        descriptor_writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[3].dstSet = g.render_target_descriptor_set;
        descriptor_writes[3].dstBinding = 0;
        descriptor_writes[3].dstArrayElement = 0;
        descriptor_writes[3].descriptorCount = 1;
        descriptor_writes[3].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[3].pImageInfo = &render_target_descriptor_image_info;

        vkUpdateDescriptorSets(
            g.device,
            static_cast<std::uint32_t>(std::size(descriptor_writes)),
            descriptor_writes,
            0,
            nullptr);
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
        result = vkCreateShaderModule(
            g.device, &shader_module_create_info, nullptr, &shader_module);
        vk_check(result);

        VkPipelineShaderStageCreateInfo shader_stage_create_info {};
        shader_stage_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shader_stage_create_info.module = shader_module;
        shader_stage_create_info.pName = "main";

        VkPushConstantRange push_constant_range {};
        push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(std::uint32_t);

        VkPipelineLayoutCreateInfo pipeline_layout_create_info {};
        pipeline_layout_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts =
            &g.storage_image_descriptor_set_layout;
        pipeline_layout_create_info.pushConstantRangeCount = 1;
        pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;

        result = vkCreatePipelineLayout(g.device,
                                        &pipeline_layout_create_info,
                                        nullptr,
                                        &g.compute_pipeline_layout);
        vk_check(result);

        VkComputePipelineCreateInfo compute_pipeline_create_info {};
        compute_pipeline_create_info.sType =
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compute_pipeline_create_info.stage = shader_stage_create_info;
        compute_pipeline_create_info.layout = g.compute_pipeline_layout;

        result = vkCreateComputePipelines(g.device,
                                          VK_NULL_HANDLE,
                                          1,
                                          &compute_pipeline_create_info,
                                          nullptr,
                                          &g.compute_pipeline);
        vk_check(result);

        vkDestroyShaderModule(g.device, shader_module, nullptr);
    }

    {
        VkAttachmentDescription attachment_description {};
        attachment_description.format = g.swapchain.format;
        attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_description.stencilStoreOp =
            VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference attachment_reference {};
        attachment_reference.attachment = 0;
        attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass_description {};
        subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_description.colorAttachmentCount = 1;
        subpass_description.pColorAttachments = &attachment_reference;

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
        render_pass_create_info.pAttachments = &attachment_description;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass_description;
        render_pass_create_info.dependencyCount = 1;
        render_pass_create_info.pDependencies = &subpass_dependency;

        result = vkCreateRenderPass(
            g.device, &render_pass_create_info, nullptr, &g.render_pass);
        vk_check(result);
    }

    create_framebuffers();

    {
        VkCommandBufferAllocateInfo command_buffer_allocate_info {};
        command_buffer_allocate_info.sType =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandPool = g.command_pool;
        command_buffer_allocate_info.commandBufferCount = frames_in_flight;

        result = vkAllocateCommandBuffers(g.device,
                                          &command_buffer_allocate_info,
                                          g.frame_command_buffers.data());
        vk_check(result);
    }

    {
        VkSemaphoreCreateInfo semaphore_create_info {};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_create_info {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (std::uint32_t i {}; i < frames_in_flight; ++i)
        {
            result = vkCreateSemaphore(g.device,
                                       &semaphore_create_info,
                                       nullptr,
                                       &g.image_available_semaphores[i]);
            vk_check(result);
            result = vkCreateSemaphore(g.device,
                                       &semaphore_create_info,
                                       nullptr,
                                       &g.render_finished_semaphores[i]);
            vk_check(result);
            result = vkCreateFence(
                g.device, &fence_create_info, nullptr, &g.in_flight_fences[i]);
            vk_check(result);
        }
    }

    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(g.window, true);

        const auto loader_func = [](const char *name, void *)
        { return vkGetInstanceProcAddr(g.instance, name); };
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
        init_info.MinImageCount = g.swapchain.min_image_count;
        init_info.ImageCount = g.swapchain.image_count;
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
        const auto result = vkDeviceWaitIdle(g.device);
        vk_check(result);
    }

    if (g.device)
    {
        if (g.index_buffer.buffer && g.index_buffer.allocation)
        {
            vmaDestroyBuffer(
                g.allocator, g.index_buffer.buffer, g.index_buffer.allocation);
        }

        if (g.vertex_buffer.buffer && g.vertex_buffer.allocation)
        {
            vmaDestroyBuffer(g.allocator,
                             g.vertex_buffer.buffer,
                             g.vertex_buffer.allocation);
        }
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (g.device)
    {
        for (std::uint32_t i {}; i < frames_in_flight; ++i)
        {
            if (g.in_flight_fences[i])
            {
                vkDestroyFence(g.device, g.in_flight_fences[i], nullptr);
            }
            if (g.render_finished_semaphores[i])
            {
                vkDestroySemaphore(
                    g.device, g.render_finished_semaphores[i], nullptr);
            }
            if (g.image_available_semaphores[i])
            {
                vkDestroySemaphore(
                    g.device, g.image_available_semaphores[i], nullptr);
            }
        }

        destroy_framebuffers();

        if (g.render_pass)
        {
            vkDestroyRenderPass(g.device, g.render_pass, nullptr);
        }

        if (g.compute_pipeline)
        {
            vkDestroyPipeline(g.device, g.compute_pipeline, nullptr);
        }

        if (g.compute_pipeline_layout)
        {
            vkDestroyPipelineLayout(
                g.device, g.compute_pipeline_layout, nullptr);
        }

        if (g.descriptor_pool)
        {
            vkDestroyDescriptorPool(g.device, g.descriptor_pool, nullptr);
        }

        if (g.render_target_descriptor_set_layout)
        {
            vkDestroyDescriptorSetLayout(
                g.device, g.render_target_descriptor_set_layout, nullptr);
        }

        if (g.storage_image_descriptor_set_layout)
        {
            vkDestroyDescriptorSetLayout(
                g.device, g.storage_image_descriptor_set_layout, nullptr);
        }

        if (g.command_pool)
        {
            vkDestroyCommandPool(g.device, g.command_pool, nullptr);
        }

        if (g.render_target.allocation)
        {
            vmaFreeMemory(g.allocator, g.render_target.allocation);
        }

        if (g.render_target_sampler)
        {
            vkDestroySampler(g.device, g.render_target_sampler, nullptr);
        }

        if (g.render_target.image_view)
        {
            vkDestroyImageView(g.device, g.render_target.image_view, nullptr);
        }

        if (g.render_target.image)
        {
            vkDestroyImage(g.device, g.render_target.image, nullptr);
        }

        if (g.storage_image.allocation)
        {
            vmaFreeMemory(g.allocator, g.storage_image.allocation);
        }

        if (g.storage_image.image_view)
        {
            vkDestroyImageView(g.device, g.storage_image.image_view, nullptr);
        }

        if (g.storage_image.image)
        {
            vkDestroyImage(g.device, g.storage_image.image, nullptr);
        }
    }

    destroy_swapchain();

    if (g.allocator)
    {
        vmaDestroyAllocator(g.allocator);
    }

    if (g.device)
    {
        vkDestroyDevice(g.device, nullptr);
    }

    if (g.instance)
    {
        if (g.surface)
        {
            vkDestroySurfaceKHR(g.instance, g.surface, nullptr);
        }

#ifndef NDEBUG
        if (g.debug_messenger)
        {
            vkDestroyDebugUtilsMessengerEXT(
                g.instance, g.debug_messenger, nullptr);
        }
#endif

        vkDestroyInstance(g.instance, nullptr);
    }

    glfwDestroyWindow(g.window);
    glfwTerminate();

    g = {};
}

void run()
{
    char input_text_buffer[256] {"image.png"};

    std::uint32_t rng_seed {1};

    while (!glfwWindowShouldClose(g.window))
    {
        glfwPollEvents();

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0.0f, 0.0f});
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.0f, 0.0f, 0.0f, 0.0f});
        if (ImGui::Begin("Viewport",
                         nullptr,
                         ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoBringToFrontOnFocus))
        {
            const auto region_size = ImGui::GetContentRegionAvail();
            if (region_size.x > 0.0f && region_size.y > 0.0f)
            {
                const auto image_aspect_ratio =
                    static_cast<float>(g.storage_image.width) /
                    static_cast<float>(g.storage_image.height);
                const auto region_aspect_ratio = region_size.x / region_size.y;
                const auto cursor_pos = ImGui::GetCursorPos();
                auto width = region_size.x;
                auto height = region_size.y;
                auto x = cursor_pos.x;
                auto y = cursor_pos.y;
                if (image_aspect_ratio >= region_aspect_ratio)
                {
                    height = width / image_aspect_ratio;
                    y += (region_size.y - height) * 0.5f;
                }
                else
                {
                    width = height * image_aspect_ratio;
                    x += (region_size.x - width) * 0.5f;
                }
                ImGui::SetCursorPos({x, y});
                ImGui::Image(
                    static_cast<ImTextureID>(g.render_target_descriptor_set),
                    {width, height});
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (ImGui::Begin("Settings"))
        {
            ImGui::Text("%.2f ms/frame, %.1f fps",
                        1000.0 / static_cast<double>(ImGui::GetIO().Framerate),
                        static_cast<double>(ImGui::GetIO().Framerate));

            VmaBudget budgets[VK_MAX_MEMORY_HEAPS] {};
            vmaGetHeapBudgets(g.allocator, budgets);
            ImGui::Text("Memory heaps:");
            for (unsigned int i {}; i < VK_MAX_MEMORY_HEAPS; ++i)
            {
                if (budgets[i].budget > 0)
                {
                    ImGui::Text("  %u: %llu MB / %llu MB",
                                i,
                                budgets[i].usage / 1'000'000u,
                                budgets[i].budget / 1'000'000u);
                }
            }

            ImGui::InputText(
                "PNG file name", input_text_buffer, sizeof(input_text_buffer));

            if (ImGui::Button("Store to PNG"))
            {
                store_to_png(input_text_buffer);
            }

            if (ImGui::Button("Change RNG seed"))
            {
                rng_seed += g.storage_image.width * g.storage_image.height;
            }
        }
        ImGui::End();

        ImGui::Render();

        draw_frame(rng_seed);
    }

    const auto result = vkDeviceWaitIdle(g.device);
    vk_check(result);
}

Vulkan_renderer::Vulkan_renderer(GLFWwindow *window,
                                 std::uint32_t framebuffer_width,
                                 std::uint32_t framebuffer_height,
                                 std::uint32_t render_width,
                                 std::uint32_t render_height)
{
}

Vulkan_renderer::~Vulkan_renderer()
{
    // This could throw, but at this point there is nothing we can do about it,
    // so let the runtime call std::terminate()
    m_device->waitIdle();
}

void Vulkan_renderer::draw_frame(std::uint32_t rng_seed)
{
}

void Vulkan_renderer::store_to_png(const char *file_name)
{
}
