#include "renderer.hpp"
#include "geometry.hpp"
#include "utility.hpp"

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

    result = context.device.vkQueueSubmit(
        context.queue, context.device.queue_family_index, &submit_info, {});
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

template <typename T>
[[nodiscard]] Vulkan_buffer
create_buffer_from_host_data(const Vulkan_device &device,
                             VmaAllocator allocator,
                             VkCommandBuffer command_buffer,
                             VkBufferUsageFlags usage,
                             VmaMemoryUsage memory_usage,
                             const std::vector<T> &data)
{
    const auto size = data.size() * sizeof(T);

    VmaAllocationInfo staging_allocation_info {};
    const auto staging_buffer =
        create_buffer(allocator,
                      size,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                      &staging_allocation_info);
    SCOPE_EXIT([&] { destroy_buffer(allocator, staging_buffer); });

    auto *const mapped_data =
        static_cast<std::uint8_t *>(staging_allocation_info.pMappedData);

    std::memcpy(mapped_data, data.data(), size);

    const auto buffer =
        create_buffer(allocator, size, usage, {}, memory_usage, nullptr);

    const VkBufferCopy region {.srcOffset = 0, .dstOffset = 0, .size = size};

    device.vkCmdCopyBuffer(
        command_buffer, staging_buffer.buffer, buffer.buffer, 1, &region);

    return buffer;
}

[[nodiscard]] Vulkan_buffer
create_vertex_buffer(const Vulkan_device &device,
                     VmaAllocator allocator,
                     VkCommandBuffer command_buffer,
                     const std::vector<float> &vertices)
{
    return create_buffer_from_host_data(
        device,
        allocator,
        command_buffer,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        vertices);
}

[[nodiscard]] Vulkan_buffer
create_index_buffer(const Vulkan_device &device,
                    VmaAllocator allocator,
                    VkCommandBuffer command_buffer,
                    const std::vector<std::uint32_t> &indices)
{
    return create_buffer_from_host_data(
        device,
        allocator,
        command_buffer,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        indices);
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

    context.vertex_buffer = create_vertex_buffer(
        context.device, context.allocator, command_buffer, geometry.vertices);

    context.index_buffer = create_index_buffer(
        context.device, context.allocator, command_buffer, geometry.indices);

    end_one_time_submit_command_buffer(context, command_buffer);
}

void destroy_scene_resources(const Vulkan_context &context)
{
    destroy_buffer(context.allocator, context.index_buffer);
    destroy_buffer(context.allocator, context.vertex_buffer);
    destroy_image_view(context.device, context.storage_image_view);
    destroy_image(context.allocator, context.storage_image);
}

void render(const Vulkan_context &context)
{
}

void write_to_png(const Vulkan_context &context, const char *file_name)
{
}
