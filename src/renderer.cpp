#include "renderer.hpp"
#include "geometry.hpp"
#include "utility.hpp"

#include "stb_image_write.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#include <cassert>
#include <cstring>

namespace
{

struct Push_constants
{
    std::uint32_t placeholder;
};
static_assert(sizeof(Push_constants) <= 128);

#ifndef NDEBUG

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity [[maybe_unused]],
    VkDebugUtilsMessageTypeFlagsEXT message_type [[maybe_unused]],
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data [[maybe_unused]])
{
    std::cout << callback_data->pMessage << std::endl;
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

[[nodiscard]] Vulkan_instance
create_instance(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr)
{
    Vulkan_instance instance {};

    assert(vkGetInstanceProcAddr);
    instance.vkGetInstanceProcAddr = vkGetInstanceProcAddr;

    const auto load_global_function =
        [vkGetInstanceProcAddr]<typename F>(F &f, const char *name)
    {
        f = reinterpret_cast<F>(vkGetInstanceProcAddr(nullptr, name));
        assert(f);
    };

    load_global_function(instance.vkEnumerateInstanceLayerProperties,
                         "vkEnumerateInstanceLayerProperties");
    load_global_function(instance.vkEnumerateInstanceExtensionProperties,
                         "vkEnumerateInstanceExtensionProperties");
    load_global_function(instance.vkCreateInstance, "vkCreateInstance");

    constexpr VkApplicationInfo application_info {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = {},
        .pApplicationName = {},
        .applicationVersion = {},
        .pEngineName = {},
        .engineVersion = {},
        .apiVersion = VK_API_VERSION_1_3};

    VkResult result;

#ifndef NDEBUG

    std::uint32_t layer_property_count {};
    result = instance.vkEnumerateInstanceLayerProperties(&layer_property_count,
                                                         nullptr);
    check_result(result, "vkEnumerateInstanceLayerProperties");
    std::vector<VkLayerProperties> layer_properties(layer_property_count);
    result = instance.vkEnumerateInstanceLayerProperties(
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

    std::uint32_t extension_property_count {};
    result = instance.vkEnumerateInstanceExtensionProperties(
        nullptr, &extension_property_count, nullptr);
    check_result(result, "vkEnumerateInstanceExtensionProperties");
    std::vector<VkExtensionProperties> extension_properties(
        extension_property_count);
    result = instance.vkEnumerateInstanceExtensionProperties(
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

    const VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = {},
        .flags = {},
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
        .pfnUserCallback = &debug_callback,
        .pUserData = {}};

    constexpr VkValidationFeatureEnableEXT enabled_validation_features[] {
        VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};

    const VkValidationFeaturesEXT validation_features {
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        .pNext = &debug_utils_messenger_create_info,
        .enabledValidationFeatureCount =
            static_cast<std::uint32_t>(std::size(enabled_validation_features)),
        .pEnabledValidationFeatures = enabled_validation_features,
        .disabledValidationFeatureCount = {},
        .pDisabledValidationFeatures = {}};

    const VkInstanceCreateInfo instance_create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &validation_features,
        .flags = {},
        .pApplicationInfo = &application_info,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &khronos_validation_layer,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &debug_utils_extension};

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

#endif

    result = instance.vkCreateInstance(
        &instance_create_info, nullptr, &instance.instance);
    check_result(result, "vkCreateInstance");

    const auto load_instance_function = [&]<typename F>(F &f, const char *name)
    {
        f = reinterpret_cast<F>(vkGetInstanceProcAddr(instance.instance, name));
        assert(f);
    };

    load_instance_function(instance.vkDestroyInstance, "vkDestroyInstance");
#ifndef NDEBUG
    load_instance_function(instance.vkCreateDebugUtilsMessengerEXT,
                           "vkCreateDebugUtilsMessengerEXT");
    load_instance_function(instance.vkDestroyDebugUtilsMessengerEXT,
                           "vkDestroyDebugUtilsMessengerEXT");
#endif
    load_instance_function(instance.vkEnumeratePhysicalDevices,
                           "vkEnumeratePhysicalDevices");
    load_instance_function(instance.vkGetPhysicalDeviceQueueFamilyProperties,
                           "vkGetPhysicalDeviceQueueFamilyProperties");
    load_instance_function(instance.vkEnumerateDeviceExtensionProperties,
                           "vkEnumerateDeviceExtensionProperties");
    load_instance_function(instance.vkGetPhysicalDeviceFeatures2,
                           "vkGetPhysicalDeviceFeatures2");
    load_instance_function(instance.vkGetPhysicalDeviceFormatProperties,
                           "vkGetPhysicalDeviceFormatProperties");
    load_instance_function(instance.vkGetPhysicalDeviceProperties2,
                           "vkGetPhysicalDeviceProperties2");
    load_instance_function(instance.vkCreateDevice, "vkCreateDevice");
    load_instance_function(instance.vkGetDeviceProcAddr, "vkGetDeviceProcAddr");

#ifndef NDEBUG

    SCOPE_FAIL([&] { instance.vkDestroyInstance(instance.instance, nullptr); });

    result = instance.vkCreateDebugUtilsMessengerEXT(
        instance.instance,
        &debug_utils_messenger_create_info,
        nullptr,
        &instance.debug_messenger);
    check_result(result, "vkCreateDebugUtilsMessengerEXT");

#endif

    return instance;
}

void destroy_instance(const Vulkan_instance &instance)
{
    if (instance.instance)
    {
#ifndef NDEBUG
        instance.vkDestroyDebugUtilsMessengerEXT(
            instance.instance, instance.debug_messenger, nullptr);
#endif

        instance.vkDestroyInstance(instance.instance, nullptr);
    }
}

[[nodiscard]] std::uint32_t
get_queue_family_index(const Vulkan_instance &instance,
                       VkPhysicalDevice physical_device)
{
    std::uint32_t queue_family_property_count {};
    instance.vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_property_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_properties(
        queue_family_property_count);
    instance.vkGetPhysicalDeviceQueueFamilyProperties(
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

    std::uint32_t extension_property_count {};
    auto result = instance.vkEnumerateDeviceExtensionProperties(
        physical_device, nullptr, &extension_property_count, nullptr);
    check_result(result, "vkEnumerateDeviceExtensionProperties");
    std::vector<VkExtensionProperties> extension_properties(
        extension_property_count);
    result = instance.vkEnumerateDeviceExtensionProperties(
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

    instance.vkGetPhysicalDeviceFeatures2(physical_device, &features_2);

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

    VkFormatProperties format_properties {};
    instance.vkGetPhysicalDeviceFormatProperties(
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

[[nodiscard]] Vulkan_device create_device(const Vulkan_instance &instance)
{
    Vulkan_device device {};

    std::uint32_t physical_device_count {};
    auto result = instance.vkEnumeratePhysicalDevices(
        instance.instance, &physical_device_count, nullptr);
    check_result(result, "vkEnumeratePhysicalDevices");
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    result = instance.vkEnumeratePhysicalDevices(
        instance.instance, &physical_device_count, physical_devices.data());
    check_result(result, "vkEnumeratePhysicalDevices");

    constexpr const char *device_extension_names[] {
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, // FIXME: disable for
                                                        // release build
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};
    constexpr auto device_extension_count =
        static_cast<std::uint32_t>(std::size(device_extension_names));

    std::uint32_t selected_device_index {};
    for (std::uint32_t i {0}; i < physical_device_count; ++i)
    {
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR
            ray_tracing_pipeline_properties {};
        ray_tracing_pipeline_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

        VkPhysicalDeviceProperties2 properties_2 {};
        properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties_2.pNext = &ray_tracing_pipeline_properties;

        instance.vkGetPhysicalDeviceProperties2(physical_devices[i],
                                                &properties_2);

        std::cout << "Physical device " << i << ": "
                  << properties_2.properties.deviceName;

        std::ostringstream message;
        if (is_device_suitable(instance,
                               physical_devices[i],
                               device_extension_count,
                               device_extension_names,
                               message))
        {
            std::cout << ": suitable\n";

            if (!device.physical_device)
            {
                selected_device_index = i;
                device.physical_device = physical_devices[i];
                device.queue_family_index =
                    get_queue_family_index(instance, physical_devices[i]);
                device.properties = properties_2.properties;
                device.ray_tracing_pipeline_properties =
                    ray_tracing_pipeline_properties;
            }
        }
        else
        {
            std::cout << ": not suitable:\n" << message.str();
        }
    }
    if (device.physical_device)
    {
        std::cout << "Selected physical device " << selected_device_index
                  << ": " << device.properties.deviceName << '\n';
    }
    else
    {
        throw std::runtime_error("Failed to find a suitable physical device");
    }

    constexpr float queue_priority {1.0f};
    const VkDeviceQueueCreateInfo queue_create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .queueFamilyIndex = device.queue_family_index,
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

    result = instance.vkCreateDevice(
        device.physical_device, &device_create_info, nullptr, &device.device);
    check_result(result, "vkCreateDevice");

    const auto load = [&]<typename F>(F &f, const char *name)
    {
        f = reinterpret_cast<F>(
            instance.vkGetDeviceProcAddr(device.device, name));
        assert(f);
    };
    load(device.vkDestroyDevice, "vkDestroyDevice");
    load(device.vkGetDeviceQueue, "vkGetDeviceQueue");
    load(device.vkCreateCommandPool, "vkCreateCommandPool");
    load(device.vkDestroyCommandPool, "vkDestroyCommandPool");
    load(device.vkCreateImageView, "vkCreateImageView");
    load(device.vkDestroyImageView, "vkDestroyImageView");
    load(device.vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
    load(device.vkFreeCommandBuffers, "vkFreeCommandBuffers");
    load(device.vkBeginCommandBuffer, "vkBeginCommandBuffer");
    load(device.vkEndCommandBuffer, "vkEndCommandBuffer");
    load(device.vkQueueSubmit, "vkQueueSubmit");
    load(device.vkQueueWaitIdle, "vkQueueWaitIdle");
    load(device.vkCmdPipelineBarrier, "vkCmdPipelineBarrier");
    load(device.vkCmdCopyBuffer, "vkCmdCopyBuffer");
    load(device.vkCmdBuildAccelerationStructuresKHR,
         "vkCmdBuildAccelerationStructuresKHR");
    load(device.vkCmdBindPipeline, "vkCmdBindPipeline");
    load(device.vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets");
    load(device.vkCmdPushConstants, "vkCmdPushConstants");
    load(device.vkCmdTraceRaysKHR, "vkCmdTraceRaysKHR");
    load(device.vkCmdBlitImage, "vkCmdBlitImage");
    load(device.vkCmdCopyImageToBuffer, "vkCmdCopyImageToBuffer");
    load(device.vkGetBufferDeviceAddress, "vkGetBufferDeviceAddress");
    load(device.vkGetAccelerationStructureDeviceAddressKHR,
         "vkGetAccelerationStructureDeviceAddressKHR");
    load(device.vkGetAccelerationStructureBuildSizesKHR,
         "vkGetAccelerationStructureBuildSizesKHR");
    load(device.vkCreateAccelerationStructureKHR,
         "vkCreateAccelerationStructureKHR");
    load(device.vkDestroyAccelerationStructureKHR,
         "vkDestroyAccelerationStructureKHR");
    load(device.vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
    load(device.vkDestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout");
    load(device.vkCreateDescriptorPool, "vkCreateDescriptorPool");
    load(device.vkDestroyDescriptorPool, "vkDestroyDescriptorPool");
    load(device.vkAllocateDescriptorSets, "vkAllocateDescriptorSets");
    load(device.vkFreeDescriptorSets, "vkFreeDescriptorSets");
    load(device.vkUpdateDescriptorSets, "vkUpdateDescriptorSets");
    load(device.vkCreatePipelineLayout, "vkCreatePipelineLayout");
    load(device.vkDestroyPipelineLayout, "vkDestroyPipelineLayout");
    load(device.vkCreateShaderModule, "vkCreateShaderModule");
    load(device.vkDestroyShaderModule, "vkDestroyShaderModule");
    load(device.vkCreateRayTracingPipelinesKHR,
         "vkCreateRayTracingPipelinesKHR");
    load(device.vkDestroyPipeline, "vkDestroyPipeline");
    load(device.vkGetRayTracingShaderGroupHandlesKHR,
         "vkGetRayTracingShaderGroupHandlesKHR");

    return device;
}

void destroy_device(const Vulkan_device &device)
{
    if (device.device)
    {
        device.vkDestroyDevice(device.device, nullptr);
    }
}

[[nodiscard]] VmaAllocator create_allocator(const Vulkan_instance &instance,
                                            const Vulkan_device &device)
{
    VmaVulkanFunctions vulkan_functions {};
    vulkan_functions.vkGetInstanceProcAddr = instance.vkGetInstanceProcAddr,
    vulkan_functions.vkGetDeviceProcAddr = instance.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_create_info {};
    allocator_create_info.flags =
        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_create_info.physicalDevice = device.physical_device;
    allocator_create_info.device = device.device;
    allocator_create_info.pVulkanFunctions = &vulkan_functions;
    allocator_create_info.instance = instance.instance;
    allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;

    VmaAllocator allocator {};
    const auto result = vmaCreateAllocator(&allocator_create_info, &allocator);
    check_result(result, "vmaCreateAllocator");

    return allocator;
}

[[nodiscard]] VkCommandPool create_command_pool(const Vulkan_device &device)
{
    const VkCommandPoolCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = {},
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device.queue_family_index};

    VkCommandPool command_pool {};
    const auto result = device.vkCreateCommandPool(
        device.device, &create_info, nullptr, &command_pool);
    check_result(result, "vkCreateCommandPool");

    return command_pool;
}

void destroy_command_pool(const Vulkan_device &device,
                          VkCommandPool command_pool)
{
    if (command_pool)
    {
        device.vkDestroyCommandPool(device.device, command_pool, nullptr);
    }
}

[[nodiscard]] VkCommandBuffer
begin_one_time_submit_command_buffer(const Vulkan_context &context)
{
    const VkCommandBufferAllocateInfo allocate_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = {},
        .commandPool = context.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1};

    VkCommandBuffer command_buffer {};
    auto result = context.device.vkAllocateCommandBuffers(
        context.device.device, &allocate_info, &command_buffer);
    check_result(result, "vkAllocateCommandBuffers");

    constexpr VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = {},
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = {}};

    SCOPE_FAIL(
        [&]
        {
            context.device.vkFreeCommandBuffers(context.device.device,
                                                context.command_pool,
                                                1,
                                                &command_buffer);
        });

    result = context.device.vkBeginCommandBuffer(command_buffer, &begin_info);
    check_result(result, "vkBeginCommandBuffer");

    return command_buffer;
}

void end_one_time_submit_command_buffer(const Vulkan_context &context,
                                        VkCommandBuffer command_buffer)
{
    SCOPE_EXIT(
        [&]
        {
            context.device.vkFreeCommandBuffers(context.device.device,
                                                context.command_pool,
                                                1,
                                                &command_buffer);
        });

    auto result = context.device.vkEndCommandBuffer(command_buffer);
    check_result(result, "vkEndCommandBuffer");

    const VkSubmitInfo submit_info {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                    .pNext = {},
                                    .waitSemaphoreCount = {},
                                    .pWaitSemaphores = {},
                                    .pWaitDstStageMask = {},
                                    .commandBufferCount = 1,
                                    .pCommandBuffers = &command_buffer,
                                    .signalSemaphoreCount = {},
                                    .pSignalSemaphores = {}};

    result = context.device.vkQueueSubmit(context.queue, 1, &submit_info, {});
    check_result(result, "vkQueueSubmit");

    result = context.device.vkQueueWaitIdle(context.queue);
    check_result(result, "vkQueueWaitIdle");
}

[[nodiscard]] Vulkan_image create_image(VmaAllocator allocator,
                                        std::uint32_t width,
                                        std::uint32_t height,
                                        VkFormat format,
                                        VkImageUsageFlags usage)
{
    Vulkan_image image {};
    image.width = width;
    image.height = height;

    const VkImageCreateInfo image_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = {},
        .pQueueFamilyIndices = {},
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    const auto result = vmaCreateImage(allocator,
                                       &image_create_info,
                                       &allocation_create_info,
                                       &image.image,
                                       &image.allocation,
                                       nullptr);
    check_result(result, "vmaCreateImage");

    return image;
}

void destroy_image(VmaAllocator allocator, const Vulkan_image &image)
{
    vmaDestroyImage(allocator, image.image, image.allocation);
}

void transition_image_layout(const Vulkan_device &device,
                             VkCommandBuffer command_buffer,
                             VkImage image,
                             VkAccessFlags src_access,
                             VkAccessFlags dst_access,
                             VkImageLayout old_layout,
                             VkImageLayout new_layout,
                             VkPipelineStageFlags src_stage,
                             VkPipelineStageFlags dst_stage)
{
    constexpr VkImageSubresourceRange subresource_range {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    const VkImageMemoryBarrier image_memory_barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = {},
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = subresource_range};

    device.vkCmdPipelineBarrier(command_buffer,
                                src_stage,
                                dst_stage,
                                {},
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &image_memory_barrier);
}

[[nodiscard]] VkImageView
create_image_view(const Vulkan_device &device, VkImage image, VkFormat format)
{
    constexpr VkImageSubresourceRange subresource_range {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    const VkImageViewCreateInfo image_view_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = subresource_range};

    VkImageView image_view {};
    const auto result = device.vkCreateImageView(
        device.device, &image_view_create_info, nullptr, &image_view);
    check_result(result, "vkCreateImageView");

    return image_view;
}

void destroy_image_view(const Vulkan_device &device, VkImageView image_view)
{
    if (image_view)
    {
        device.vkDestroyImageView(device.device, image_view, nullptr);
    }
}

[[nodiscard]] Vulkan_buffer
create_buffer(VmaAllocator allocator,
              VkDeviceSize size,
              VkBufferUsageFlags usage,
              VmaAllocationCreateFlags allocation_flags,
              VmaMemoryUsage memory_usage,
              VmaAllocationInfo *allocation_info)
{
    Vulkan_buffer buffer {};
    buffer.size = size;

    const VkBufferCreateInfo buffer_create_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = {},
        .pQueueFamilyIndices = {}};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.flags = allocation_flags;
    allocation_create_info.usage = memory_usage;

    const auto result = vmaCreateBuffer(allocator,
                                        &buffer_create_info,
                                        &allocation_create_info,
                                        &buffer.buffer,
                                        &buffer.allocation,
                                        allocation_info);
    check_result(result, "vmaCreateBuffer");

    return buffer;
}

void destroy_buffer(VmaAllocator allocator, const Vulkan_buffer &buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

[[nodiscard]] Vulkan_buffer
create_buffer_from_host_data(const Vulkan_context &context,
                             VkBufferUsageFlags usage,
                             VmaMemoryUsage memory_usage,
                             const void *data,
                             std::size_t size)
{
    VmaAllocationInfo staging_allocation_info {};
    const auto staging_buffer =
        create_buffer(context.allocator,
                      size,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                      &staging_allocation_info);
    SCOPE_EXIT([&] { destroy_buffer(context.allocator, staging_buffer); });

    auto *const mapped_data =
        static_cast<std::uint8_t *>(staging_allocation_info.pMappedData);

    std::memcpy(mapped_data, data, size);

    const auto buffer = create_buffer(
        context.allocator, size, usage, {}, memory_usage, nullptr);

    const auto command_buffer = begin_one_time_submit_command_buffer(context);

    const VkBufferCopy region {.srcOffset = 0, .dstOffset = 0, .size = size};

    context.device.vkCmdCopyBuffer(
        command_buffer, staging_buffer.buffer, buffer.buffer, 1, &region);

    end_one_time_submit_command_buffer(context, command_buffer);

    return buffer;
}

[[nodiscard]] Vulkan_buffer
create_vertex_buffer(const Vulkan_context &context,
                     const std::vector<float> &vertices)
{
    return create_buffer_from_host_data(
        context,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        vertices.data(),
        vertices.size() * sizeof(float));
}

[[nodiscard]] Vulkan_buffer
create_index_buffer(const Vulkan_context &context,
                    const std::vector<std::uint32_t> &indices)
{
    return create_buffer_from_host_data(
        context,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        indices.data(),
        indices.size() * sizeof(std::uint32_t));
}

[[nodiscard]] VkDeviceAddress get_device_address(const Vulkan_device &device,
                                                 VkBuffer buffer)
{
    const VkBufferDeviceAddressInfo address_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = {},
        .buffer = buffer};
    return device.vkGetBufferDeviceAddress(device.device, &address_info);
}

[[nodiscard]] VkDeviceAddress
get_device_address(const Vulkan_device &device,
                   VkAccelerationStructureKHR acceleration_structure)
{
    const VkAccelerationStructureDeviceAddressInfoKHR address_info {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .pNext = {},
        .accelerationStructure = acceleration_structure};
    return device.vkGetAccelerationStructureDeviceAddressKHR(device.device,
                                                             &address_info);
}

[[nodiscard]] Vulkan_acceleration_structure
create_blas(const Vulkan_context &context)
{
    const auto vertex_buffer_address =
        get_device_address(context.device, context.vertex_buffer.buffer);
    constexpr auto vertex_size = 3 * sizeof(float);
    const auto vertex_count = context.vertex_buffer.size / vertex_size;

    const auto index_buffer_address =
        get_device_address(context.device, context.index_buffer.buffer);
    const auto index_count = context.index_buffer.size / sizeof(std::uint32_t);
    const auto primitive_count = index_count / 3;

    const VkAccelerationStructureGeometryTrianglesDataKHR triangles {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .pNext = {},
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = vertex_buffer_address},
        .vertexStride = vertex_size,
        .maxVertex = static_cast<std::uint32_t>(vertex_count - 1),
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {.deviceAddress = index_buffer_address},
        .transformData = {}};

    const VkAccelerationStructureGeometryKHR geometry {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = {},
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {.triangles = triangles},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR};

    const VkAccelerationStructureBuildRangeInfoKHR build_range_info {
        .primitiveCount = static_cast<std::uint32_t>(primitive_count),
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0};

    VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = {},
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = {},
        .dstAccelerationStructure = {},
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = {},
        .scratchData = {}};

    VkAccelerationStructureBuildSizesInfoKHR build_sizes_info {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = {},
        .accelerationStructureSize = {},
        .updateScratchSize = {},
        .buildScratchSize = {}};

    context.device.vkGetAccelerationStructureBuildSizesKHR(
        context.device.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build_geometry_info,
        &build_range_info.primitiveCount,
        &build_sizes_info);

    Vulkan_acceleration_structure blas {};
    blas.buffer =
        create_buffer(context.allocator,
                      build_sizes_info.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      {},
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      nullptr);
    SCOPE_FAIL([&] { destroy_buffer(context.allocator, blas.buffer); });

    const VkAccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = {},
            .createFlags = {},
            .buffer = blas.buffer.buffer,
            .offset = {},
            .size = build_sizes_info.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .deviceAddress = {}};

    const auto result = context.device.vkCreateAccelerationStructureKHR(
        context.device.device,
        &acceleration_structure_create_info,
        nullptr,
        &blas.acceleration_structure);
    check_result(result, "vkCreateAccelerationStructureKHR");
    SCOPE_FAIL(
        [&]
        {
            context.device.vkDestroyAccelerationStructureKHR(
                context.device.device, blas.acceleration_structure, nullptr);
        });

    const auto scratch_buffer =
        create_buffer(context.allocator,
                      build_sizes_info.buildScratchSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      {},
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      nullptr);
    SCOPE_EXIT([&] { destroy_buffer(context.allocator, scratch_buffer); });

    const auto scratch_buffer_address =
        get_device_address(context.device, scratch_buffer.buffer);

    build_geometry_info.dstAccelerationStructure = blas.acceleration_structure;
    build_geometry_info.scratchData.deviceAddress = scratch_buffer_address;

    const auto command_buffer = begin_one_time_submit_command_buffer(context);

    const auto *const p_build_range_info = &build_range_info;

    context.device.vkCmdBuildAccelerationStructuresKHR(
        command_buffer, 1, &build_geometry_info, &p_build_range_info);

    end_one_time_submit_command_buffer(context, command_buffer);

    return blas;
}

[[nodiscard]] Vulkan_acceleration_structure
create_tlas(const Vulkan_context &context)
{
    VkTransformMatrixKHR transform {};
    transform.matrix[0][0] = 1.0f;
    transform.matrix[1][1] = 1.0f;
    transform.matrix[2][2] = 1.0f;

    const VkAccelerationStructureInstanceKHR as_instance {
        .transform = transform,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = get_device_address(
            context.device, context.blas.acceleration_structure)};

    const auto instance_buffer = create_buffer_from_host_data(
        context,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        &as_instance,
        sizeof(VkAccelerationStructureInstanceKHR));
    SCOPE_EXIT([&] { destroy_buffer(context.allocator, instance_buffer); });

    const auto command_buffer = begin_one_time_submit_command_buffer(context);

    constexpr VkMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = {},
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR};

    context.device.vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        {},
        1,
        &barrier,
        0,
        nullptr,
        0,
        nullptr);

    const VkAccelerationStructureGeometryInstancesDataKHR geometry_instances_data {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .pNext = {},
        .arrayOfPointers = {},
        .data = {.deviceAddress = get_device_address(context.device,
                                                     instance_buffer.buffer)}};

    const VkAccelerationStructureGeometryKHR geometry {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = {},
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {.instances = geometry_instances_data},
        .flags = {}};

    VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = {},
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = {},
        .dstAccelerationStructure = {},
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = {},
        .scratchData = {}};

    VkAccelerationStructureBuildSizesInfoKHR build_sizes_info {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = {},
        .accelerationStructureSize = {},
        .updateScratchSize = {},
        .buildScratchSize = {}};

    constexpr std::uint32_t primitive_count {1};
    context.device.vkGetAccelerationStructureBuildSizesKHR(
        context.device.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build_geometry_info,
        &primitive_count,
        &build_sizes_info);

    Vulkan_acceleration_structure tlas {};
    tlas.buffer =
        create_buffer(context.allocator,
                      build_sizes_info.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      {},
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      nullptr);
    SCOPE_FAIL([&] { destroy_buffer(context.allocator, tlas.buffer); });

    const VkAccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = {},
            .createFlags = {},
            .buffer = tlas.buffer.buffer,
            .offset = {},
            .size = build_sizes_info.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .deviceAddress = {}};

    const auto result = context.device.vkCreateAccelerationStructureKHR(
        context.device.device,
        &acceleration_structure_create_info,
        nullptr,
        &tlas.acceleration_structure);
    check_result(result, "vkCreateAccelerationStructureKHR");
    SCOPE_FAIL(
        [&]
        {
            context.device.vkDestroyAccelerationStructureKHR(
                context.device.device, tlas.acceleration_structure, nullptr);
        });

    const auto scratch_buffer =
        create_buffer(context.allocator,
                      build_sizes_info.buildScratchSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      {},
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      nullptr);
    SCOPE_EXIT([&] { destroy_buffer(context.allocator, scratch_buffer); });

    const auto scratch_buffer_address =
        get_device_address(context.device, scratch_buffer.buffer);

    build_geometry_info.dstAccelerationStructure = tlas.acceleration_structure;
    build_geometry_info.scratchData.deviceAddress = scratch_buffer_address;

    constexpr VkAccelerationStructureBuildRangeInfoKHR build_range_info {
        .primitiveCount = 1,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0};

    const auto *const p_build_range_info = &build_range_info;

    context.device.vkCmdBuildAccelerationStructuresKHR(
        command_buffer, 1, &build_geometry_info, &p_build_range_info);

    end_one_time_submit_command_buffer(context, command_buffer);

    return tlas;
}

void destroy_acceleration_structure(
    const Vulkan_context &context,
    const Vulkan_acceleration_structure &acceleration_structure)
{
    if (acceleration_structure.acceleration_structure)
    {
        context.device.vkDestroyAccelerationStructureKHR(
            context.device.device,
            acceleration_structure.acceleration_structure,
            nullptr);
        destroy_buffer(context.allocator, acceleration_structure.buffer);
    }
}

[[nodiscard]] VkDescriptorSetLayout
create_descriptor_set_layout(const Vulkan_device &device)
{
    constexpr VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] {
        {.binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
         .pImmutableSamplers = {}},
        {.binding = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
         .pImmutableSamplers = {}},
        {.binding = 2,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                       VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
         .pImmutableSamplers = {}},
        {.binding = 3,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                       VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
         .pImmutableSamplers = {}}};

    const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .bindingCount = static_cast<std::uint32_t>(
            std::size(descriptor_set_layout_bindings)),
        .pBindings = descriptor_set_layout_bindings};

    VkDescriptorSetLayout descriptor_set_layout {};
    const auto result =
        device.vkCreateDescriptorSetLayout(device.device,
                                           &descriptor_set_layout_create_info,
                                           nullptr,
                                           &descriptor_set_layout);
    check_result(result, "vkCreateDescriptorSetLayout");

    return descriptor_set_layout;
}

void destroy_descriptor_set_layout(const Vulkan_device &device,
                                   VkDescriptorSetLayout descriptor_set_layout)
{
    if (descriptor_set_layout)
    {
        device.vkDestroyDescriptorSetLayout(
            device.device, descriptor_set_layout, nullptr);
    }
}

[[nodiscard]] VkDescriptorPool
create_descriptor_pool(const Vulkan_device &device)
{
    constexpr VkDescriptorPoolSize pool_sizes[] {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}};

    const VkDescriptorPoolCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .maxSets = 4, // FIXME
        .poolSizeCount = static_cast<std::uint32_t>(std::size(pool_sizes)),
        .pPoolSizes = pool_sizes};

    VkDescriptorPool descriptor_pool {};
    const auto result = device.vkCreateDescriptorPool(
        device.device, &create_info, nullptr, &descriptor_pool);
    check_result(result, "vkCreateDescriptorPool");

    return descriptor_pool;
}

void destroy_descriptor_pool(const Vulkan_device &device,
                             VkDescriptorPool descriptor_pool)
{
    if (descriptor_pool)
    {
        device.vkDestroyDescriptorPool(device.device, descriptor_pool, nullptr);
    }
}

[[nodiscard]] VkDescriptorSet
allocate_descriptor_set(const Vulkan_context &context)
{
    const VkDescriptorSetAllocateInfo descriptor_set_allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = {},
        .descriptorPool = context.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &context.descriptor_set_layout};

    VkDescriptorSet descriptor_set {};
    const auto result = context.device.vkAllocateDescriptorSets(
        context.device.device, &descriptor_set_allocate_info, &descriptor_set);
    check_result(result, "vkAllocateDescriptorSets");

    const VkDescriptorImageInfo descriptor_storage_image {
        .sampler = VK_NULL_HANDLE,
        .imageView = context.storage_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

    const VkWriteDescriptorSetAccelerationStructureKHR
        descriptor_acceleration_structure {
            .sType =
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .pNext = {},
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &context.tlas.acceleration_structure};

    const VkDescriptorBufferInfo descriptor_vertices {
        .buffer = context.vertex_buffer.buffer,
        .offset = 0,
        .range = context.vertex_buffer.size};

    const VkDescriptorBufferInfo descriptor_indices {
        .buffer = context.index_buffer.buffer,
        .offset = 0,
        .range = context.index_buffer.size};

    const VkWriteDescriptorSet descriptor_writes[4] {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = {},
         .dstSet = descriptor_set,
         .dstBinding = 0,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .pImageInfo = &descriptor_storage_image,
         .pBufferInfo = {},
         .pTexelBufferView = {}},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = &descriptor_acceleration_structure,
         .dstSet = descriptor_set,
         .dstBinding = 1,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
         .pImageInfo = {},
         .pBufferInfo = {},
         .pTexelBufferView = {}},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = {},
         .dstSet = descriptor_set,
         .dstBinding = 2,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .pImageInfo = {},
         .pBufferInfo = &descriptor_vertices,
         .pTexelBufferView = {}},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = {},
         .dstSet = descriptor_set,
         .dstBinding = 3,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .pImageInfo = {},
         .pBufferInfo = &descriptor_indices,
         .pTexelBufferView = {}}};

    context.device.vkUpdateDescriptorSets(
        context.device.device,
        static_cast<std::uint32_t>(std::size(descriptor_writes)),
        descriptor_writes,
        0,
        nullptr);

    return descriptor_set;
}

[[nodiscard]] VkPipelineLayout
create_ray_tracing_pipeline_layout(const Vulkan_context &context)
{
    constexpr VkPushConstantRange push_constant_range {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .offset = 0,
        .size = sizeof(Push_constants)};

    const VkPipelineLayoutCreateInfo pipeline_layout_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .setLayoutCount = 1,
        .pSetLayouts = &context.descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range};

    VkPipelineLayout pipeline_layout {};
    const auto result =
        context.device.vkCreatePipelineLayout(context.device.device,
                                              &pipeline_layout_create_info,
                                              nullptr,
                                              &pipeline_layout);
    check_result(result, "vkCreatePipelineLayout");

    return pipeline_layout;
}

void destroy_pipeline_layout(const Vulkan_device &device,
                             VkPipelineLayout pipeline_layout)
{
    if (pipeline_layout)
    {
        device.vkDestroyPipelineLayout(device.device, pipeline_layout, nullptr);
    }
}

[[nodiscard]] VkShaderModule create_shader_module(const Vulkan_device &device,
                                                  const char *file_name)
{
    const auto shader_code = read_binary_file(file_name);

    const VkShaderModuleCreateInfo shader_module_create_info {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .codeSize = shader_code.size() * sizeof(std::uint32_t),
        .pCode = shader_code.data()};

    VkShaderModule shader_module {};
    const auto result = device.vkCreateShaderModule(
        device.device, &shader_module_create_info, nullptr, &shader_module);
    check_result(result, "vkCreateShaderModule");

    return shader_module;
}

void destroy_shader_module(const Vulkan_device &device,
                           VkShaderModule shader_module)
{
    if (shader_module)
    {
        device.vkDestroyShaderModule(device.device, shader_module, nullptr);
    }
}

[[nodiscard]] VkPipeline
create_ray_tracing_pipeline(const Vulkan_context &context)
{
    const auto rgen_shader_module =
        create_shader_module(context.device, "shader.rgen.spv");
    SCOPE_EXIT([&]
               { destroy_shader_module(context.device, rgen_shader_module); });
    const auto rmiss_shader_module =
        create_shader_module(context.device, "shader.rmiss.spv");
    SCOPE_EXIT([&]
               { destroy_shader_module(context.device, rmiss_shader_module); });
    const auto rchit_shader_module =
        create_shader_module(context.device, "shader.rchit.spv");
    SCOPE_EXIT([&]
               { destroy_shader_module(context.device, rchit_shader_module); });

    const VkPipelineShaderStageCreateInfo shader_stage_create_infos[] {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .pNext = {},
         .flags = {},
         .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
         .module = rgen_shader_module,
         .pName = "main",
         .pSpecializationInfo = {}},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .pNext = {},
         .flags = {},
         .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
         .module = rmiss_shader_module,
         .pName = "main",
         .pSpecializationInfo = {}},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .pNext = {},
         .flags = {},
         .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
         .module = rchit_shader_module,
         .pName = "main",
         .pSpecializationInfo = {}}};

    const VkRayTracingShaderGroupCreateInfoKHR ray_tracing_shader_groups[] {
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
         .pNext = {},
         .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
         .generalShader = 0,
         .closestHitShader = VK_SHADER_UNUSED_KHR,
         .anyHitShader = VK_SHADER_UNUSED_KHR,
         .intersectionShader = VK_SHADER_UNUSED_KHR,
         .pShaderGroupCaptureReplayHandle = {}},
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
         .pNext = {},
         .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
         .generalShader = 1,
         .closestHitShader = VK_SHADER_UNUSED_KHR,
         .anyHitShader = VK_SHADER_UNUSED_KHR,
         .intersectionShader = VK_SHADER_UNUSED_KHR,
         .pShaderGroupCaptureReplayHandle = {}},
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
         .pNext = {},
         .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
         .generalShader = VK_SHADER_UNUSED_KHR,
         .closestHitShader = 2,
         .anyHitShader = VK_SHADER_UNUSED_KHR,
         .intersectionShader = VK_SHADER_UNUSED_KHR,
         .pShaderGroupCaptureReplayHandle = {}}};

    const VkRayTracingPipelineCreateInfoKHR ray_tracing_pipeline_create_info {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .pNext = {},
        .flags = {},
        .stageCount =
            static_cast<std::uint32_t>(std::size(shader_stage_create_infos)),
        .pStages = shader_stage_create_infos,
        .groupCount =
            static_cast<std::uint32_t>(std::size(ray_tracing_shader_groups)),
        .pGroups = ray_tracing_shader_groups,
        .maxPipelineRayRecursionDepth = 1,
        .pLibraryInfo = {},
        .pLibraryInterface = {},
        .pDynamicState = {},
        .layout = context.ray_tracing_pipeline_layout,
        .basePipelineHandle = {},
        .basePipelineIndex = {}};

    VkPipeline ray_tracing_pipeline {};
    const auto result = context.device.vkCreateRayTracingPipelinesKHR(
        context.device.device,
        {},
        {},
        1,
        &ray_tracing_pipeline_create_info,
        nullptr,
        &ray_tracing_pipeline);
    check_result(result, "vkCreateRayTracingPipelinesKHR");

    return ray_tracing_pipeline;
}

void destroy_pipeline(const Vulkan_device &device, VkPipeline pipeline)
{
    if (pipeline)
    {
        device.vkDestroyPipeline(device.device, pipeline, nullptr);
    }
}

[[nodiscard]] Vulkan_shader_binding_table
create_shader_binding_table(const Vulkan_context &context)
{
    const auto handle_size =
        context.device.ray_tracing_pipeline_properties.shaderGroupHandleSize;
    const auto handle_alignment = context.device.ray_tracing_pipeline_properties
                                      .shaderGroupHandleAlignment;
    const auto base_alignment =
        context.device.ray_tracing_pipeline_properties.shaderGroupBaseAlignment;
    const auto handle_size_aligned = align_up(handle_size, handle_alignment);

    constexpr std::uint32_t miss_count {1};
    constexpr std::uint32_t hit_count {1};
    constexpr std::uint32_t handle_count {1 + miss_count + hit_count};

    Vulkan_shader_binding_table sbt {};

    sbt.raygen_region.stride = align_up(handle_size_aligned, base_alignment);
    sbt.raygen_region.size = sbt.raygen_region.stride;

    sbt.miss_region.stride = handle_size_aligned;
    sbt.miss_region.size =
        align_up(miss_count * handle_size_aligned, base_alignment);

    sbt.hit_region.stride = handle_size_aligned;
    sbt.hit_region.size =
        align_up(hit_count * handle_size_aligned, base_alignment);

    const auto data_size = handle_count * handle_size;
    std::vector<std::uint8_t> handles(data_size);
    const auto result = context.device.vkGetRayTracingShaderGroupHandlesKHR(
        context.device.device,
        context.ray_tracing_pipeline,
        0,
        handle_count,
        data_size,
        handles.data());
    check_result(result, "vkGetRayTracingShaderGroupHandlesKHR");

    const auto sbt_size = sbt.raygen_region.size + sbt.miss_region.size +
                          sbt.hit_region.size + sbt.callable_region.size;

    VmaAllocationInfo sbt_allocation_info {};
    sbt.buffer =
        create_buffer(context.allocator,
                      sbt_size,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                          VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                      &sbt_allocation_info);
    auto *const sbt_buffer_mapped =
        static_cast<std::uint8_t *>(sbt_allocation_info.pMappedData);

    const auto sbt_address =
        get_device_address(context.device, sbt.buffer.buffer);
    sbt.raygen_region.deviceAddress = sbt_address;
    sbt.miss_region.deviceAddress = sbt_address + sbt.raygen_region.size;
    sbt.hit_region.deviceAddress =
        sbt_address + sbt.raygen_region.size + sbt.miss_region.size;

    const auto get_handle_pointer = [&](std::uint32_t i)
    { return handles.data() + i * handle_size; };

    std::uint32_t handle_index {0};
    std::memcpy(
        sbt_buffer_mapped, get_handle_pointer(handle_index), handle_size);
    ++handle_index;

    auto p_data = sbt_buffer_mapped + sbt.raygen_region.size;
    for (std::uint32_t i {}; i < miss_count; ++i)
    {
        std::memcpy(p_data, get_handle_pointer(handle_index), handle_size);
        ++handle_index;
        p_data += sbt.miss_region.stride;
    }

    p_data = sbt_buffer_mapped + sbt.raygen_region.size + sbt.miss_region.size;
    for (std::uint32_t i {}; i < hit_count; ++i)
    {
        std::memcpy(p_data, get_handle_pointer(handle_index), handle_size);
        ++handle_index;
        p_data += sbt.hit_region.stride;
    }

    return sbt;
}

void destroy_shader_binding_table(
    VmaAllocator allocator,
    const Vulkan_shader_binding_table &shader_binding_table)
{
    destroy_buffer(allocator, shader_binding_table.buffer);
}

} // namespace

Vulkan_context
create_vulkan_context(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr)
{
    Vulkan_context context {};

    SCOPE_FAIL([&] { destroy_vulkan_context(context); });

    context.instance = create_instance(vkGetInstanceProcAddr);

    context.device = create_device(context.instance);

    context.allocator = create_allocator(context.instance, context.device);

    context.device.vkGetDeviceQueue(context.device.device,
                                    context.device.queue_family_index,
                                    0,
                                    &context.queue);

    context.command_pool = create_command_pool(context.device);

    return context;
}

void destroy_vulkan_context(Vulkan_context &context)
{
    destroy_command_pool(context.device, context.command_pool);
    vmaDestroyAllocator(context.allocator);
    destroy_device(context.device);
    destroy_instance(context.instance);
    context = {};
}

void load_scene(Vulkan_context &context,
                std::uint32_t render_width,
                std::uint32_t render_height,
                const struct Geometry &geometry)
{
    SCOPE_FAIL([&] { destroy_scene_resources(context); });

    constexpr auto image_format = VK_FORMAT_R32G32B32A32_SFLOAT;

    context.storage_image = create_image(context.allocator,
                                         render_width,
                                         render_height,
                                         image_format,
                                         VK_IMAGE_USAGE_STORAGE_BIT |
                                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    context.storage_image_view = create_image_view(
        context.device, context.storage_image.image, image_format);

    const auto command_buffer = begin_one_time_submit_command_buffer(context);
    transition_image_layout(context.device,
                            command_buffer,
                            context.storage_image.image,
                            VK_ACCESS_NONE,
                            VK_ACCESS_SHADER_READ_BIT |
                                VK_ACCESS_SHADER_WRITE_BIT,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
    end_one_time_submit_command_buffer(context, command_buffer);

    context.vertex_buffer = create_vertex_buffer(context, geometry.vertices);

    context.index_buffer = create_index_buffer(context, geometry.indices);

    context.blas = create_blas(context);

    context.tlas = create_tlas(context);

    context.descriptor_set_layout =
        create_descriptor_set_layout(context.device);

    context.descriptor_pool = create_descriptor_pool(context.device);

    context.descriptor_set = allocate_descriptor_set(context);

    context.ray_tracing_pipeline_layout =
        create_ray_tracing_pipeline_layout(context);

    context.ray_tracing_pipeline = create_ray_tracing_pipeline(context);

    context.shader_binding_table = create_shader_binding_table(context);
}

void destroy_scene_resources(const Vulkan_context &context)
{
    destroy_shader_binding_table(context.allocator,
                                 context.shader_binding_table);
    destroy_pipeline(context.device, context.ray_tracing_pipeline);
    destroy_pipeline_layout(context.device,
                            context.ray_tracing_pipeline_layout);
    destroy_descriptor_pool(context.device, context.descriptor_pool);
    destroy_descriptor_set_layout(context.device,
                                  context.descriptor_set_layout);
    destroy_acceleration_structure(context, context.tlas);
    destroy_acceleration_structure(context, context.blas);
    destroy_buffer(context.allocator, context.index_buffer);
    destroy_buffer(context.allocator, context.vertex_buffer);
    destroy_image_view(context.device, context.storage_image_view);
    destroy_image(context.allocator, context.storage_image);
}

void render(const Vulkan_context &context)
{
    const auto command_buffer = begin_one_time_submit_command_buffer(context);

    context.device.vkCmdBindPipeline(command_buffer,
                                     VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                     context.ray_tracing_pipeline);

    context.device.vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        context.ray_tracing_pipeline_layout,
        0,
        1,
        &context.descriptor_set,
        0,
        nullptr);

    const Push_constants push_constants {};

    context.device.vkCmdPushConstants(command_buffer,
                                      context.ray_tracing_pipeline_layout,
                                      VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                          VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                      0,
                                      sizeof(Push_constants),
                                      &push_constants);

    context.device.vkCmdTraceRaysKHR(
        command_buffer,
        &context.shader_binding_table.raygen_region,
        &context.shader_binding_table.miss_region,
        &context.shader_binding_table.hit_region,
        &context.shader_binding_table.callable_region,
        context.storage_image.width,
        context.storage_image.height,
        1);

    end_one_time_submit_command_buffer(context, command_buffer);
}

void write_to_png(const Vulkan_context &context, const char *file_name)
{
    constexpr auto format = VK_FORMAT_R8G8B8A8_SRGB;

    const auto final_image = create_image(context.allocator,
                                          context.storage_image.width,
                                          context.storage_image.height,
                                          format,
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    SCOPE_EXIT([&] { destroy_image(context.allocator, final_image); });

    const auto image_size = final_image.width * final_image.height * 4;

    VmaAllocationInfo staging_allocation_info {};
    const auto staging_buffer =
        create_buffer(context.allocator,
                      image_size,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                      &staging_allocation_info);
    SCOPE_EXIT([&] { destroy_buffer(context.allocator, staging_buffer); });

    const auto *const mapped_data =
        static_cast<std::uint8_t *>(staging_allocation_info.pMappedData);

    const auto command_buffer = begin_one_time_submit_command_buffer(context);

    constexpr VkImageSubresourceRange subresource_range {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    const VkImageMemoryBarrier storage_image_memory_barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = {},
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = context.storage_image.image,
        .subresourceRange = subresource_range};

    VkImageMemoryBarrier final_image_memory_barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = {},
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = final_image.image,
        .subresourceRange = subresource_range};

    const VkImageMemoryBarrier image_memory_barriers[] {
        storage_image_memory_barrier, final_image_memory_barrier};

    context.device.vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT |
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        {},
        0,
        nullptr,
        0,
        nullptr,
        static_cast<std::uint32_t>(std::size(image_memory_barriers)),
        image_memory_barriers);

    constexpr VkImageSubresourceLayers subresource_layers {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1};

    const VkImageBlit image_blit {
        .srcSubresource = subresource_layers,
        .srcOffsets = {{0, 0, 0},
                       {static_cast<std::int32_t>(final_image.width),
                        static_cast<std::int32_t>(final_image.height),
                        1}},
        .dstSubresource = subresource_layers,
        .dstOffsets = {{0, 0, 0},
                       {static_cast<std::int32_t>(final_image.width),
                        static_cast<std::int32_t>(final_image.height),
                        1}}};

    context.device.vkCmdBlitImage(command_buffer,
                                  context.storage_image.image,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  final_image.image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  1,
                                  &image_blit,
                                  VK_FILTER_NEAREST);

    final_image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    final_image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    final_image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    final_image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    context.device.vkCmdPipelineBarrier(command_buffer,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        {},
                                        0,
                                        nullptr,
                                        0,
                                        nullptr,
                                        1,
                                        &final_image_memory_barrier);

    const VkBufferImageCopy copy_region {
        .bufferOffset = 0,
        .bufferRowLength = {},
        .bufferImageHeight = final_image.height,
        .imageSubresource = subresource_layers,
        .imageOffset = {0, 0, 0},
        .imageExtent = {final_image.width, final_image.height, 1}};

    context.device.vkCmdCopyImageToBuffer(command_buffer,
                                          final_image.image,
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                          staging_buffer.buffer,
                                          1,
                                          &copy_region);

    end_one_time_submit_command_buffer(context, command_buffer);

    const auto write_result =
        stbi_write_png(file_name,
                       static_cast<int>(final_image.width),
                       static_cast<int>(final_image.height),
                       4,
                       mapped_data,
                       static_cast<int>(final_image.width * 4));
    if (write_result == 0)
    {
        throw std::runtime_error("Failed to write PNG image");
    }
}
