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
    default: return nullptr;
    }
}

void print_result(std::ostringstream &oss, VkResult result)
{
    if (const auto result_string = result_to_string(result);
        result_string != nullptr)
    {
        oss << result_string;
    }
    else
    {
        oss << static_cast<std::underlying_type_t<VkResult>>(result);
    }
}

inline void check_result(VkResult result, const char *message)
{
    if (result != VK_SUCCESS)
    {
        std::ostringstream oss;
        oss << message << ": ";
        print_result(oss, result);
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
    load(functions.vkCreateDevice, "vkCreateDevice");
    load(functions.vkGetDeviceProcAddr, "vkGetDeviceProcAddr");

    return functions;
}

[[nodiscard]] Vulkan_device_functions
load_device_functions(VkDevice device,
                      PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr)
{
    assert(device);
    assert(vkGetDeviceProcAddr);

    Vulkan_device_functions functions {};

    const auto load = [=]<typename F>(F &f, const char *name)
    {
        f = reinterpret_cast<F>(vkGetDeviceProcAddr(device, name));
        assert(f);
    };

    load(functions.vkDestroyDevice, "vkDestroyDevice");
    load(functions.vkGetDeviceQueue, "vkGetDeviceQueue");
    load(functions.vkCreateCommandPool, "vkCreateCommandPool");
    load(functions.vkDestroyCommandPool, "vkDestroyCommandPool");

    return functions;
}

[[nodiscard]] Vulkan_instance
create_instance(const Vulkan_global_functions &global_functions)
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

    assert(global_functions.vkCreateInstance);
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
        assert(instance.functions.vkDestroyInstance);
        instance.functions.vkDestroyInstance(instance.instance, nullptr);
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

void destroy_instance(const Vulkan_instance &instance)
{
    if (instance.instance)
    {
#ifndef NDEBUG
        assert(instance.functions.vkDestroyDebugUtilsMessengerEXT);
        instance.functions.vkDestroyDebugUtilsMessengerEXT(
            instance.instance, instance.debug_messenger, nullptr);
#endif

        assert(instance.functions.vkDestroyInstance);
        instance.functions.vkDestroyInstance(instance.instance, nullptr);
    }
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
        if ((queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            (queue_family_properties[i].queueCount > 0))
        {
            return i;
        }
    }

    return std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] bool is_device_suitable(const Vulkan_instance &instance,
                                      VkPhysicalDevice physical_device,
                                      std::uint32_t device_extension_count,
                                      const char *const *device_extension_names,
                                      std::ostringstream &message)
{
    bool suitable {true};

    const auto queue_family_index =
        get_queue_family_index(instance, physical_device);
    if (queue_family_index == std::numeric_limits<std::uint32_t>::max())
    {
        suitable = false;
        message << "- No queue family supports compute operations\n";
    }

    assert(instance.functions.vkEnumerateDeviceExtensionProperties);
    std::uint32_t extension_property_count {};
    auto result = instance.functions.vkEnumerateDeviceExtensionProperties(
        physical_device, nullptr, &extension_property_count, nullptr);
    check_result(result, "vkEnumerateDeviceExtensionProperties");
    std::vector<VkExtensionProperties> extension_properties(
        extension_property_count);
    result = instance.functions.vkEnumerateDeviceExtensionProperties(
        physical_device,
        nullptr,
        &extension_property_count,
        extension_properties.data());
    check_result(result, "vkEnumerateDeviceExtensionProperties");

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
            suitable = false;
            message << "- The device extension " << extension_name
                    << " is not supported\n";
        }
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
    instance.functions.vkGetPhysicalDeviceFeatures2(physical_device,
                                                    &features_2);

    if (!vulkan_1_2_features.bufferDeviceAddress)
    {
        suitable = false;
        message
            << "- The device feature bufferDeviceAddress is not supported\n";
    }

    if (!vulkan_1_2_features.scalarBlockLayout)
    {
        suitable = false;
        message << "- The device feature scalarBlockLayout is not supported\n";
    }

    if (!acceleration_structure_features.accelerationStructure)
    {
        suitable = false;
        message
            << "- The device feature accelerationStructure is not supported\n";
    }

    if (!ray_tracing_pipeline_features.rayTracingPipeline)
    {
        suitable = false;
        message << "- The device feature rayTracingPipeline is not supported\n";
    }

    assert(instance.functions.vkGetPhysicalDeviceFormatProperties);
    VkFormatProperties format_properties {};
    instance.functions.vkGetPhysicalDeviceFormatProperties(
        physical_device, VK_FORMAT_R32G32B32A32_SFLOAT, &format_properties);
    if (!(format_properties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) ||
        !(format_properties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
    {
        suitable = false;
        message << "- VK_FORMAT_R32G32B32A32_SFLOAT not supported for storage "
                   "images\n";
    }

    return suitable;
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

    for (std::uint32_t i {0}; i < physical_device_count; ++i)
    {
        std::ostringstream message;
        if (is_device_suitable(instance,
                               physical_devices[i],
                               device_extension_count,
                               device_extension_names,
                               message))
        {
            const auto queue_family_index =
                get_queue_family_index(instance, physical_devices[i]);

            VkPhysicalDeviceRayTracingPipelinePropertiesKHR
                ray_tracing_pipeline_properties {};
            ray_tracing_pipeline_properties.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

            VkPhysicalDeviceProperties2 properties_2 {};
            properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties_2.pNext = &ray_tracing_pipeline_properties;

            assert(instance.functions.vkGetPhysicalDeviceProperties2);
            instance.functions.vkGetPhysicalDeviceProperties2(
                physical_devices[i], &properties_2);

            // TODO: it might be useful to not return immediately, if one wants
            // to know why a subsequent device is not suitable
            return {.physical_device = physical_devices[i],
                    .queue_family_index = queue_family_index,
                    .properties = properties_2.properties,
                    .ray_tracing_pipeline_properties =
                        ray_tracing_pipeline_properties};
        }
        else
        {
            std::cerr << "Physical device " << i << " is not suitable:\n";
            std::cerr << message.str();
        }
    }

    throw std::runtime_error("Failed to find a suitable physical device");
}

[[nodiscard]] Vulkan_device
create_device(const Vulkan_instance &instance,
              const Vulkan_physical_device &physical_device,
              std::uint32_t device_extension_count,
              const char *const *device_extension_names)
{
    Vulkan_device device {};

    constexpr float queue_priority {1.0f};
    const VkDeviceQueueCreateInfo queue_create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .queueFamilyIndex = physical_device.queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority};

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR
        ray_tracing_pipeline_features {};
    ray_tracing_pipeline_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    ray_tracing_pipeline_features.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR
        acceleration_structure_features {};
    acceleration_structure_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    acceleration_structure_features.pNext = &ray_tracing_pipeline_features;
    acceleration_structure_features.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vulkan_1_2_features {};
    vulkan_1_2_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan_1_2_features.pNext = &acceleration_structure_features;
    vulkan_1_2_features.scalarBlockLayout = VK_TRUE;
    vulkan_1_2_features.bufferDeviceAddress = VK_TRUE;

    const VkPhysicalDeviceFeatures2 features_2 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vulkan_1_2_features,
        .features = {}};

    const VkDeviceCreateInfo device_create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features_2,
        .flags = {},
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledLayerCount = {},
        .ppEnabledLayerNames = {},
        .enabledExtensionCount = device_extension_count,
        .ppEnabledExtensionNames = device_extension_names,
        .pEnabledFeatures = {}};

    assert(instance.functions.vkCreateDevice);
    const auto result =
        instance.functions.vkCreateDevice(physical_device.physical_device,
                                          &device_create_info,
                                          nullptr,
                                          &device.device);
    check_result(result, "vkCreateDevice");

    device.functions = load_device_functions(
        device.device, instance.functions.vkGetDeviceProcAddr);

    return device;
}

void destroy_device(const Vulkan_device &device)
{
    if (device.device)
    {
        assert(device.functions.vkDestroyDevice);
        device.functions.vkDestroyDevice(device.device, nullptr);
    }
}

[[nodiscard]] VkCommandPool
create_command_pool(const Vulkan_physical_device &physical_device,
                    const Vulkan_device &device)
{
    const VkCommandPoolCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = {},
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = physical_device.queue_family_index};

    VkCommandPool command_pool {};
    assert(device.functions.vkCreateCommandPool);
    const auto result = device.functions.vkCreateCommandPool(
        device.device, &create_info, nullptr, &command_pool);
    check_result(result, "vkCreateCommandPool");

    return command_pool;
}

void destroy_command_pool(const Vulkan_device &device,
                          VkCommandPool command_pool)
{
    if (command_pool)
    {
        assert(device.functions.vkDestroyCommandPool);
        device.functions.vkDestroyCommandPool(
            device.device, command_pool, nullptr);
    }
}

} // namespace

Vulkan_context
create_vulkan_context(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr)
{
    Vulkan_context context {};
    try
    {
        context.global_functions = load_global_functions(vkGetInstanceProcAddr);

        context.instance = create_instance(context.global_functions);

        constexpr const char *device_extension_names[] {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};
        constexpr auto device_extension_count =
            static_cast<std::uint32_t>(std::size(device_extension_names));

        context.physical_device = select_physical_device(
            context.instance, device_extension_count, device_extension_names);

        context.device = create_device(context.instance,
                                       context.physical_device,
                                       device_extension_count,
                                       device_extension_names);

        VmaVulkanFunctions vulkan_functions {};
        vulkan_functions.vkGetInstanceProcAddr =
            context.global_functions.vkGetInstanceProcAddr,
        vulkan_functions.vkGetDeviceProcAddr =
            context.instance.functions.vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocator_create_info {};
        allocator_create_info.flags =
            VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocator_create_info.physicalDevice =
            context.physical_device.physical_device;
        allocator_create_info.device = context.device.device;
        allocator_create_info.pVulkanFunctions = &vulkan_functions;
        allocator_create_info.instance = context.instance.instance;
        allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;
        const auto result =
            vmaCreateAllocator(&allocator_create_info, &context.allocator);
        check_result(result, "vmaCreateAllocator");

        assert(context.device.functions.vkGetDeviceQueue);
        context.device.functions.vkGetDeviceQueue(
            context.device.device,
            context.physical_device.queue_family_index,
            0,
            &context.queue);

        context.command_pool =
            create_command_pool(context.physical_device, context.device);

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
    destroy_command_pool(context.device, context.command_pool);
    vmaDestroyAllocator(context.allocator);
    destroy_device(context.device);
    destroy_instance(context.instance);
    context = {};
}
