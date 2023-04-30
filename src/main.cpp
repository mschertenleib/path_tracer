
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
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

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
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

struct context
{
    VkInstance instance;
#ifdef ENABLE_VALIDATION_LAYERS
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    VkPhysicalDevice physical_device;
    std::uint32_t compute_queue_family;
    VkDevice device;
    VkQueue compute_queue;
    VmaAllocator allocator;
};

[[noreturn]] void
fatal_error(std::string_view message,
            const std::source_location &loc = std::source_location::current())
{
    std::ostringstream oss;
    oss << loc.file_name() << ':' << loc.line() << ": " << message;
    throw std::runtime_error(oss.str());
}

[[nodiscard]] constexpr const char *
vk_result_to_string(VkResult result) noexcept
{
    switch (result)
    {
#define CASE(e)                                                                \
    case e: return #e;
        CASE(VK_SUCCESS)
        CASE(VK_NOT_READY)
        CASE(VK_TIMEOUT)
        CASE(VK_EVENT_SET)
        CASE(VK_EVENT_RESET)
        CASE(VK_INCOMPLETE)
        CASE(VK_ERROR_OUT_OF_HOST_MEMORY)
        CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
        CASE(VK_ERROR_INITIALIZATION_FAILED)
        CASE(VK_ERROR_DEVICE_LOST)
        CASE(VK_ERROR_MEMORY_MAP_FAILED)
        CASE(VK_ERROR_LAYER_NOT_PRESENT)
        CASE(VK_ERROR_EXTENSION_NOT_PRESENT)
        CASE(VK_ERROR_FEATURE_NOT_PRESENT)
        CASE(VK_ERROR_INCOMPATIBLE_DRIVER)
        CASE(VK_ERROR_TOO_MANY_OBJECTS)
        CASE(VK_ERROR_FORMAT_NOT_SUPPORTED)
        CASE(VK_ERROR_FRAGMENTED_POOL)
        CASE(VK_ERROR_UNKNOWN)
        CASE(VK_ERROR_OUT_OF_POOL_MEMORY)
        CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE)
        CASE(VK_ERROR_FRAGMENTATION)
        CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS)
        CASE(VK_PIPELINE_COMPILE_REQUIRED)
        CASE(VK_ERROR_SURFACE_LOST_KHR)
        CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
        CASE(VK_SUBOPTIMAL_KHR)
        CASE(VK_ERROR_OUT_OF_DATE_KHR)
        CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
        CASE(VK_ERROR_VALIDATION_FAILED_EXT)
        CASE(VK_ERROR_INVALID_SHADER_NV)
#ifdef VK_ENABLE_BETA_EXTENSIONS
        CASE(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR)
        CASE(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR)
        CASE(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR)
        CASE(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR)
        CASE(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR)
        CASE(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR)
#endif
        CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)
        CASE(VK_ERROR_NOT_PERMITTED_KHR)
        CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
        CASE(VK_THREAD_IDLE_KHR)
        CASE(VK_THREAD_DONE_KHR)
        CASE(VK_OPERATION_DEFERRED_KHR)
        CASE(VK_OPERATION_NOT_DEFERRED_KHR)
        CASE(VK_ERROR_COMPRESSION_EXHAUSTED_EXT)
#undef CASE
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

[[nodiscard]] std::vector<std::uint8_t> read_shader_file(const char *file_name)
{
    const std::filesystem::path path(file_name);
    if (!std::filesystem::exists(path))
    {
        fatal_error(std::string("File \"") + std::string(file_name) +
                    std::string("\" does not exist"));
    }

    const auto file_size = std::filesystem::file_size(path);
    const auto buffer_size =
        ((file_size + sizeof(std::uint32_t) - 1u) / sizeof(std::uint32_t)) *
        sizeof(std::uint32_t);
    std::vector<std::uint8_t> buffer(buffer_size);

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        fatal_error(std::string("Failed to open file \"") + path.string() +
                    std::string("\""));
    }

    if (!file.read(reinterpret_cast<char *>(buffer.data()),
                   static_cast<std::streamsize>(buffer_size)))
    {
        fatal_error(std::string("Reading file \"") + path.string() +
                    std::string("\" failed"));
    }

    return buffer;
}

#ifdef ENABLE_VALIDATION_LAYERS

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

void get_queue_families(VkPhysicalDevice physical_device,
                        std::uint32_t &compute_queue_family)
{
    compute_queue_family = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t property_count {};
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &property_count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &property_count, properties.data());

    for (std::uint32_t i {}; i < properties.size(); ++i)
    {
        if (properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            compute_queue_family = i;
            return;
        }
    }
}

[[nodiscard]] bool
is_physical_device_suitable(VkPhysicalDevice physical_device,
                            const char *const *device_extensions,
                            std::uint32_t device_extension_count)
{
    std::uint32_t compute_queue_family;
    get_queue_families(physical_device, compute_queue_family);
    if (compute_queue_family == std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    std::uint32_t extension_property_count {};
    vk_check(vkEnumerateDeviceExtensionProperties(
        physical_device, nullptr, &extension_property_count, nullptr));
    std::vector<VkExtensionProperties> extension_properties(
        extension_property_count);
    vk_check(vkEnumerateDeviceExtensionProperties(physical_device,
                                                  nullptr,
                                                  &extension_property_count,
                                                  extension_properties.data()));

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

void init(context &ctx)
{
    {
        VkApplicationInfo application_info {};
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.apiVersion = VK_API_VERSION_1_2;

#ifdef ENABLE_VALIDATION_LAYERS

        std::uint32_t layer_property_count {};
        vk_check(
            vkEnumerateInstanceLayerProperties(&layer_property_count, nullptr));
        std::vector<VkLayerProperties> layer_properties(layer_property_count);
        vk_check(vkEnumerateInstanceLayerProperties(&layer_property_count,
                                                    layer_properties.data()));

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

        const char *const required_extensions[] {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

        std::uint32_t extension_property_count {};
        vk_check(vkEnumerateInstanceExtensionProperties(
            nullptr, &extension_property_count, nullptr));
        std::vector<VkExtensionProperties> extension_properties(
            extension_property_count);
        vk_check(vkEnumerateInstanceExtensionProperties(
            nullptr, &extension_property_count, extension_properties.data()));

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

        if (!std::all_of(std::begin(required_extensions),
                         std::end(required_extensions),
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
            static_cast<std::uint32_t>(std::size(required_extensions));
        instance_create_info.ppEnabledExtensionNames = required_extensions;

        vk_check(
            vkCreateInstance(&instance_create_info, nullptr, &ctx.instance));

        vk_check(vkCreateDebugUtilsMessengerEXT(ctx.instance,
                                                &debug_messenger_create_info,
                                                nullptr,
                                                &ctx.debug_messenger));

#else

        VkInstanceCreateInfo instance_create_info {};
        instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.pApplicationInfo = &application_info;

        vk_check(
            vkCreateInstance(&instance_create_info, nullptr, &ctx.instance));

#endif
    }

    constexpr const char *device_extensions[] {
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME};
    constexpr auto device_extension_count =
        static_cast<std::uint32_t>(std::size(device_extensions));

    {
        std::uint32_t physical_device_count {};
        vk_check(vkEnumeratePhysicalDevices(
            ctx.instance, &physical_device_count, nullptr));
        if (physical_device_count == 0)
        {
            fatal_error("Failed to find a physical device with Vulkan support");
        }

        std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
        vk_check(vkEnumeratePhysicalDevices(
            ctx.instance, &physical_device_count, physical_devices.data()));

        ctx.physical_device = {};
        for (const auto physical_device : physical_devices)
        {
            if (is_physical_device_suitable(
                    physical_device, device_extensions, device_extension_count))
            {
                ctx.physical_device = physical_device;
                break;
            }
        }
        if (!ctx.physical_device)
        {
            fatal_error("Failed to find a suitable physical device");
        }
    }

    {
        const float queue_priority {1.0f};
        VkDeviceQueueCreateInfo queue_create_info {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = ctx.compute_queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;

        VkDeviceCreateInfo device_create_info {};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.queueCreateInfoCount = 1;
        device_create_info.pQueueCreateInfos = &queue_create_info;
        device_create_info.enabledExtensionCount = device_extension_count;
        device_create_info.ppEnabledExtensionNames = device_extensions;

        vk_check(vkCreateDevice(
            ctx.physical_device, &device_create_info, nullptr, &ctx.device));
    }

    {
        vkGetDeviceQueue(
            ctx.device, ctx.compute_queue_family, 0, &ctx.compute_queue);
    }

    {
        VmaAllocatorCreateInfo allocatorCreateInfo {};
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        allocatorCreateInfo.physicalDevice = ctx.physical_device;
        allocatorCreateInfo.device = ctx.device;
        allocatorCreateInfo.instance = ctx.instance;

        vk_check(vmaCreateAllocator(&allocatorCreateInfo, &ctx.allocator));
    }
}

void shutdown(context &ctx)
{
    if (ctx.device)
    {
        vk_check(vkDeviceWaitIdle(ctx.device));
    }

    if (ctx.allocator)
    {
        vmaDestroyAllocator(ctx.allocator);
    }

    if (ctx.device)
    {
        ctx.compute_queue = {};

        vkDestroyDevice(ctx.device, nullptr);
        ctx.device = {};
    }

    ctx.compute_queue_family = {};
    ctx.physical_device = {};

    if (ctx.instance)
    {
#ifdef ENABLE_VALIDATION_LAYERS
        if (ctx.debug_messenger)
        {
            vkDestroyDebugUtilsMessengerEXT(
                ctx.instance, ctx.debug_messenger, nullptr);
            ctx.debug_messenger = {};
        };
#endif

        vkDestroyInstance(ctx.instance, nullptr);
        ctx.instance = {};
    }
}

} // namespace

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pMessenger)
{
    const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    assert(func);
    return func(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL
vkDestroyDebugUtilsMessengerEXT(VkInstance instance,
                                VkDebugUtilsMessengerEXT messenger,
                                const VkAllocationCallbacks *pAllocator)
{
    const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    assert(func);
    func(instance, messenger, pAllocator);
}

int main()
{
    try
    {
        context ctx {};
        init(ctx);
        shutdown(ctx);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception thrown\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
