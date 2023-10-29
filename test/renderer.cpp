#include "renderer.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#include <cassert>
#include <cstring>

namespace
{

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

[[nodiscard]] constexpr const char *result_to_string(VkResult result) noexcept
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
    case VK_RESULT_MAX_ENUM: return "VK_RESULT_MAX_ENUM";
    }
    return "unknown";
}

inline void check_result(VkResult result, const char *message)
{
    if (result != VK_SUCCESS)
    {
        std::ostringstream oss;
        oss << message << ": " << result_to_string(result);
        throw std::runtime_error(oss.str());
    }
}

[[nodiscard]] Vulkan_global_functions
load_global_functions(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr)
{
    assert(vkGetInstanceProcAddr);

    Vulkan_global_functions functions {};
    functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;

    const auto load = [=]<typename F>(F &f, const char *name)
    {
        f = reinterpret_cast<F>(vkGetInstanceProcAddr(nullptr, name));
        assert(f);
    };

    load(functions.vkEnumerateInstanceLayerProperties,
         "vkEnumerateInstanceLayerProperties");
    load(functions.vkEnumerateInstanceExtensionProperties,
         "vkEnumerateInstanceExtensionProperties");
    load(functions.vkCreateInstance, "vkCreateInstance");

    return functions;
}

[[nodiscard]] Vulkan_instance_functions
load_instance_functions(VkInstance instance,
                        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr)
{
    assert(instance);
    assert(vkGetInstanceProcAddr);

    Vulkan_instance_functions functions {};

    const auto load = [=]<typename F>(F &f, const char *name)
    {
        f = reinterpret_cast<F>(vkGetInstanceProcAddr(instance, name));
        assert(f);
    };

    load(functions.vkDestroyInstance, "vkDestroyInstance");
    load(functions.vkCreateDebugUtilsMessengerEXT,
         "vkCreateDebugUtilsMessengerEXT");
    load(functions.vkDestroyDebugUtilsMessengerEXT,
         "vkDestroyDebugUtilsMessengerEXT");
    load(functions.vkEnumeratePhysicalDevices, "vkEnumeratePhysicalDevices");
    load(functions.vkGetPhysicalDeviceQueueFamilyProperties,
         "vkGetPhysicalDeviceQueueFamilyProperties");
    load(functions.vkEnumerateDeviceExtensionProperties,
         "vkEnumerateDeviceExtensionProperties");
    load(functions.vkGetPhysicalDeviceFeatures2,
         "vkGetPhysicalDeviceFeatures2");
    load(functions.vkGetPhysicalDeviceFormatProperties,
         "vkGetPhysicalDeviceFormatProperties");
    load(functions.vkGetPhysicalDeviceProperties2,
         "vkGetPhysicalDeviceProperties2");

    return functions;
}

void destroy_vulkan_instance(Vulkan_instance &instance)
{
    if (instance.instance)
    {
#ifndef NDEBUG
        if (instance.debug_messenger)
        {
            assert(instance.functions.vkDestroyDebugUtilsMessengerEXT);
            instance.functions.vkDestroyDebugUtilsMessengerEXT(
                instance.instance, instance.debug_messenger, nullptr);
        }
#endif

        assert(instance.functions.vkDestroyInstance);
        instance.functions.vkDestroyInstance(instance.instance, nullptr);

        instance = {};
    }
}

[[nodiscard]] Vulkan_instance
create_vulkan_instance(const Vulkan_global_functions &global_functions)
{
    Vulkan_instance instance {};

    constexpr VkApplicationInfo application_info {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = {},
        .pApplicationName = {},
        .applicationVersion = {},
        .pEngineName = {},
        .engineVersion = {},
        .apiVersion = VK_API_VERSION_1_3};

#ifndef NDEBUG

    assert(global_functions.vkEnumerateInstanceLayerProperties);
    std::uint32_t layer_property_count {};
    auto result = global_functions.vkEnumerateInstanceLayerProperties(
        &layer_property_count, nullptr);
    check_result(result, "vkEnumerateInstanceLayerProperties");
    std::vector<VkLayerProperties> layer_properties(layer_property_count);
    result = global_functions.vkEnumerateInstanceLayerProperties(
        &layer_property_count, layer_properties.data());
    check_result(result, "vkEnumerateInstanceLayerProperties");

    constexpr auto khronos_validation_layer = "VK_LAYER_KHRONOS_validation";
    if (std::none_of(layer_properties.begin(),
                     layer_properties.end(),
                     [](const VkLayerProperties &properties) {
                         return std::strcmp(properties.layerName,
                                            khronos_validation_layer) == 0;
                     }))
    {
        throw std::runtime_error(
            "VK_LAYER_KHRONOS_validation is not supported");
    }

    assert(global_functions.vkEnumerateInstanceExtensionProperties);
    std::uint32_t extension_property_count {};
    result = global_functions.vkEnumerateInstanceExtensionProperties(
        nullptr, &extension_property_count, nullptr);
    check_result(result, "vkEnumerateInstanceExtensionProperties");
    std::vector<VkExtensionProperties> extension_properties(
        extension_property_count);
    result = global_functions.vkEnumerateInstanceExtensionProperties(
        nullptr, &extension_property_count, extension_properties.data());
    check_result(result, "vkEnumerateInstanceExtensionProperties");

    constexpr auto debug_utils_extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    if (std::none_of(extension_properties.begin(),
                     extension_properties.end(),
                     [](const VkExtensionProperties &properties) {
                         return std::strcmp(properties.extensionName,
                                            debug_utils_extension) == 0;
                     }))
    {
        throw std::runtime_error(VK_EXT_DEBUG_UTILS_EXTENSION_NAME
                                 " is not supported");
    }

    constexpr VkDebugUtilsMessengerCreateInfoEXT
        debug_utils_messenger_create_info {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = {},
            .flags = {},
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = &debug_callback,
            .pUserData = {}};

    const VkInstanceCreateInfo instance_create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debug_utils_messenger_create_info,
        .flags = {},
        .pApplicationInfo = &application_info,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &khronos_validation_layer,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &debug_utils_extension};

    result = global_functions.vkCreateInstance(
        &instance_create_info, nullptr, &instance.instance);
    check_result(result, "vkCreateInstance");

    instance.functions = load_instance_functions(
        instance.instance, global_functions.vkGetInstanceProcAddr);

    try
    {
        result = instance.functions.vkCreateDebugUtilsMessengerEXT(
            instance.instance,
            &debug_utils_messenger_create_info,
            nullptr,
            &instance.debug_messenger);
        check_result(result, "vkCreateDebugUtilsMessengerEXT");
    }
    catch (...)
    {
        destroy_vulkan_instance(instance);
        throw;
    }

#else

    const VkInstanceCreateInfo instance_create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .pApplicationInfo = &application_info,
        .enabledLayerCount = {},
        .ppEnabledLayerNames = {},
        .enabledExtensionCount = {},
        .ppEnabledExtensionNames = {}};

    const auto result = global_functions.vkCreateInstance(
        &instance_create_info, nullptr, &instance.instance);
    check_result(result, "vkCreateInstance");

    instance.functions = load_instance_functions(
        instance.instance, global_functions.vkGetInstanceProcAddr);

#endif

    return instance;
}

[[nodiscard]] std::uint32_t
get_queue_family_index(const Vulkan_instance &instance,
                       VkPhysicalDevice physical_device)
{
    assert(instance.functions.vkGetPhysicalDeviceQueueFamilyProperties);
    std::uint32_t queue_family_property_count {};
    instance.functions.vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_property_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_properties(
        queue_family_property_count);
    instance.functions.vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_property_count,
        queue_family_properties.data());

    for (std::uint32_t i {0}; i < queue_family_property_count; ++i)
    {
        if (queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            return i;
        }
    }

    return std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] Vulkan_physical_device
select_physical_device(const Vulkan_instance &instance,
                       std::uint32_t device_extension_count,
                       const char *const *device_extension_names)
{
    assert(instance.functions.vkEnumeratePhysicalDevices);
    std::uint32_t physical_device_count {};
    auto result = instance.functions.vkEnumeratePhysicalDevices(
        instance.instance, &physical_device_count, nullptr);
    check_result(result, "vkEnumeratePhysicalDevices");
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    result = instance.functions.vkEnumeratePhysicalDevices(
        instance.instance, &physical_device_count, physical_devices.data());
    check_result(result, "vkEnumeratePhysicalDevices");

    for (const auto available_device : physical_devices)
    {
        const auto queue_family_index =
            get_queue_family_index(instance, available_device);
        if (queue_family_index == std::numeric_limits<std::uint32_t>::max())
        {
            continue;
        }

        assert(instance.functions.vkEnumerateDeviceExtensionProperties);
        std::uint32_t extension_property_count {};
        result = instance.functions.vkEnumerateDeviceExtensionProperties(
            available_device, nullptr, &extension_property_count, nullptr);
        check_result(result, "vkEnumerateDeviceExtensionProperties");
        std::vector<VkExtensionProperties> extension_properties(
            extension_property_count);
        result = instance.functions.vkEnumerateDeviceExtensionProperties(
            available_device,
            nullptr,
            &extension_property_count,
            extension_properties.data());
        check_result(result, "vkEnumerateDeviceExtensionProperties");

        bool all_extensions_supported {true};
        for (std::uint32_t i {}; i < device_extension_count; ++i)
        {
            const auto extension_name = device_extension_names[i];
            if (std::none_of(
                    extension_properties.begin(),
                    extension_properties.end(),
                    [extension_name](const VkExtensionProperties &properties) {
                        return std::strcmp(properties.extensionName,
                                           extension_name) == 0;
                    }))
            {
                all_extensions_supported = false;
                break;
            }
        }
        if (!all_extensions_supported)
        {
            continue;
        }

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR
            ray_tracing_pipeline_features {};
        ray_tracing_pipeline_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR
            acceleration_structure_features {};
        acceleration_structure_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        acceleration_structure_features.pNext = &ray_tracing_pipeline_features;

        VkPhysicalDeviceVulkan12Features vulkan_1_2_features {};
        vulkan_1_2_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vulkan_1_2_features.pNext = &acceleration_structure_features;

        VkPhysicalDeviceFeatures2 features_2 {};
        features_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features_2.pNext = &vulkan_1_2_features;

        assert(instance.functions.vkGetPhysicalDeviceFeatures2);
        instance.functions.vkGetPhysicalDeviceFeatures2(available_device,
                                                        &features_2);

        if (!vulkan_1_2_features.bufferDeviceAddress ||
            !vulkan_1_2_features.scalarBlockLayout ||
            !acceleration_structure_features.accelerationStructure ||
            !ray_tracing_pipeline_features.rayTracingPipeline)
        {
            continue;
        }

        assert(instance.functions.vkGetPhysicalDeviceFormatProperties);
        VkFormatProperties format_properties {};
        instance.functions.vkGetPhysicalDeviceFormatProperties(
            available_device,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            &format_properties);
        if (!(format_properties.optimalTilingFeatures &
              VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) ||
            !(format_properties.optimalTilingFeatures &
              VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
        {
            continue;
        }

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR
            ray_tracing_pipeline_properties {};
        ray_tracing_pipeline_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

        VkPhysicalDeviceProperties2 properties_2 {};
        properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties_2.pNext = &ray_tracing_pipeline_properties;

        assert(instance.functions.vkGetPhysicalDeviceProperties2);
        instance.functions.vkGetPhysicalDeviceProperties2(available_device,
                                                          &properties_2);

        return {.physical_device = available_device,
                .queue_family_index = queue_family_index,
                .properties = properties_2.properties,
                .ray_tracing_pipeline_properties =
                    ray_tracing_pipeline_properties};
    }

    throw std::runtime_error("Failed to find a suitable physical device");
}

} // namespace

Vulkan_context
create_vulkan_context(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr)
{
    Vulkan_context context {};
    try
    {
        context.global_functions = load_global_functions(vkGetInstanceProcAddr);
        context.instance = create_vulkan_instance(context.global_functions);
        constexpr const char *device_extension_names[] {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};
        constexpr auto device_extension_count =
            static_cast<std::uint32_t>(std::size(device_extension_names));
        context.physical_device = select_physical_device(
            context.instance, device_extension_count, device_extension_names);

        return context;
    }
    catch (...)
    {
        destroy_vulkan_context(context);
        throw;
    }
}

void destroy_vulkan_context(Vulkan_context &context)
{
    context.physical_device = {};
    destroy_vulkan_instance(context.instance);
    context.global_functions = {};
}
