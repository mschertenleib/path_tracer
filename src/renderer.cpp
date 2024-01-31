#include "renderer.hpp"
#include "camera.hpp"
#include "utility.hpp"

#include "stb_image_write.h"

#include "imgui_impl_vulkan.h"

#include <assimp/scene.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <volk.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#include <cassert>
#include <cstring>

#ifndef NDEBUG
#define ENABLE_VALIDATION_LAYERS
#endif

namespace
{

struct Push_constants
{
    std::uint32_t global_frame_count;
    std::uint32_t sample_count;
    std::uint32_t samples_per_frame;
    vec3 camera_position;
    vec3 camera_dir_x;
    vec3 camera_dir_y;
    vec3 camera_dir_z;
};
static_assert(sizeof(Push_constants) <= 128);

#ifdef ENABLE_VALIDATION_LAYERS

VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data [[maybe_unused]])
{
    std::ostringstream message;
    bool use_stderr {false};

    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        message << "[VERBOSE]";
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        message << "[INFO]";
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        message << "[WARNING]";
        use_stderr = true;
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        message << "[ERROR]";
        use_stderr = true;
    }

    if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
    {
        message << "[GENERAL] ";
    }
    else if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
    {
        message << "[VALIDATION] ";
    }
    else if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
        message << "[PERFORMANCE] ";
    }
    else if (message_type &
             VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT)
    {
        message << "[DEVICE_ADDRESS_BINDING] ";
    }

    message << callback_data->pMessage;

    if (use_stderr)
    {
        std::cerr << message.str() << '\n';
    }
    else
    {
        std::cout << message.str() << '\n';
    }

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

std::ostream &operator<<(std::ostream &os, VkResult result)
{
    if (const auto result_string = result_to_string(result);
        result_string != nullptr)
    {
        os << result_string;
    }
    else
    {
        os << static_cast<std::underlying_type_t<VkResult>>(result);
    }
    return os;
}

void check_result(VkResult result, const char *message)
{
    if (result != VK_SUCCESS)
    {
        std::ostringstream oss;
        oss << message << ": " << result;
        throw std::runtime_error(oss.str());
    }
}

void create_instance(Vulkan_context &context)
{
    auto result = volkInitialize();
    check_result(result, "volkInitialize");
    SCOPE_FAIL([] { volkFinalize(); });

    constexpr VkApplicationInfo application_info {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = {},
        .pApplicationName = {},
        .applicationVersion = {},
        .pEngineName = {},
        .engineVersion = {},
        .apiVersion = VK_API_VERSION_1_3};

    std::uint32_t glfw_required_extension_count {};
    const auto glfw_required_extension_names =
        glfwGetRequiredInstanceExtensions(&glfw_required_extension_count);

#ifdef ENABLE_VALIDATION_LAYERS

    std::uint32_t layer_property_count {};
    result = vkEnumerateInstanceLayerProperties(&layer_property_count, nullptr);
    check_result(result, "vkEnumerateInstanceLayerProperties");
    std::vector<VkLayerProperties> layer_properties(layer_property_count);
    result = vkEnumerateInstanceLayerProperties(&layer_property_count,
                                                layer_properties.data());
    check_result(result, "vkEnumerateInstanceLayerProperties");

    constexpr auto layer_name = "VK_LAYER_KHRONOS_validation";
    if (std::none_of(layer_properties.begin(),
                     layer_properties.end(),
                     [layer_name](const VkLayerProperties &properties) {
                         return std::strcmp(properties.layerName, layer_name) ==
                                0;
                     }))
    {
        std::ostringstream message;
        message << layer_name << " is not supported\n";
        throw std::runtime_error(message.str());
    }

#endif

    std::uint32_t extension_property_count {};
    result = vkEnumerateInstanceExtensionProperties(
        nullptr, &extension_property_count, nullptr);
    check_result(result, "vkEnumerateInstanceExtensionProperties");
    std::vector<VkExtensionProperties> extension_properties(
        extension_property_count);
    result = vkEnumerateInstanceExtensionProperties(
        nullptr, &extension_property_count, extension_properties.data());
    check_result(result, "vkEnumerateInstanceExtensionProperties");

#ifdef ENABLE_VALIDATION_LAYERS

    std::vector<const char *> required_extension_names;
    required_extension_names.reserve(glfw_required_extension_count + 1);
    required_extension_names.assign(glfw_required_extension_names,
                                    glfw_required_extension_names +
                                        glfw_required_extension_count);
    required_extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

#else

    const std::vector<const char *> required_extension_names(
        glfw_required_extension_names,
        glfw_required_extension_names + glfw_required_extension_count);

#endif

    bool all_extensions_supported {true};
    std::ostringstream message;
    for (const char *extension_name : required_extension_names)
    {
        if (std::none_of(
                extension_properties.begin(),
                extension_properties.end(),
                [extension_name](const VkExtensionProperties &properties) {
                    return std::strcmp(properties.extensionName,
                                       extension_name) == 0;
                }))
        {
            if (all_extensions_supported)
            {
                all_extensions_supported = false;
                message << "Unsupported instance extension(s): "
                        << extension_name;
            }
            else
            {
                message << ", " << extension_name;
            }
        }
    }
    if (!all_extensions_supported)
    {
        throw std::runtime_error(message.str());
    }

#ifdef ENABLE_VALIDATION_LAYERS

    const VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = {},
        .flags = {},
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType =
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
        .ppEnabledLayerNames = &layer_name,
        .enabledExtensionCount =
            static_cast<std::uint32_t>(required_extension_names.size()),
        .ppEnabledExtensionNames = required_extension_names.data()};

#else

    const VkInstanceCreateInfo instance_create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .pApplicationInfo = &application_info,
        .enabledLayerCount = {},
        .ppEnabledLayerNames = {},
        .enabledExtensionCount =
            static_cast<std::uint32_t>(required_extension_names.size()),
        .ppEnabledExtensionNames = required_extension_names.data()};

#endif

    result =
        vkCreateInstance(&instance_create_info, nullptr, &context.instance);
    check_result(result, "vkCreateInstance");

    volkLoadInstanceOnly(context.instance);

#ifdef ENABLE_VALIDATION_LAYERS

    SCOPE_FAIL([&] { vkDestroyInstance(context.instance, nullptr); });

    result = vkCreateDebugUtilsMessengerEXT(context.instance,
                                            &debug_utils_messenger_create_info,
                                            nullptr,
                                            &context.debug_messenger);
    check_result(result, "vkCreateDebugUtilsMessengerEXT");

#endif
}

[[nodiscard]] Vulkan_queue_family_indices
get_queue_family_indices(VkInstance instance, VkPhysicalDevice physical_device)
{
    std::uint32_t queue_family_property_count {};
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_property_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_properties(
        queue_family_property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_property_count,
                                             queue_family_properties.data());

    Vulkan_queue_family_indices indices {
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint32_t>::max()};

    for (std::uint32_t i {0}; i < queue_family_property_count; ++i)
    {
        if ((queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            (queue_family_properties[i].queueCount > 0))
        {
            indices.graphics_compute = i;
        }
        if (glfwGetPhysicalDevicePresentationSupport(
                instance, physical_device, i))
        {
            indices.present = i;
        }
        if (indices.graphics_compute !=
                std::numeric_limits<std::uint32_t>::max() &&
            indices.present != std::numeric_limits<std::uint32_t>::max())
        {
            break;
        }
    }

    return indices;
}

[[nodiscard]] bool is_device_suitable(VkInstance instance,
                                      VkPhysicalDevice physical_device,
                                      std::uint32_t device_extension_count,
                                      const char *const *device_extension_names)
{
    bool suitable {true};

    const auto queue_family_indices =
        get_queue_family_indices(instance, physical_device);
    if (queue_family_indices.graphics_compute ==
        std::numeric_limits<std::uint32_t>::max())
    {
        suitable = false;
        std::cout
            << "    No queue family supports graphics and compute operations\n";
    }
    if (queue_family_indices.present ==
        std::numeric_limits<std::uint32_t>::max())
    {
        suitable = false;
        std::cout << "    No queue family supports present operations\n";
    }

    std::uint32_t extension_property_count {};
    auto result = vkEnumerateDeviceExtensionProperties(
        physical_device, nullptr, &extension_property_count, nullptr);
    check_result(result, "vkEnumerateDeviceExtensionProperties");
    std::vector<VkExtensionProperties> extension_properties(
        extension_property_count);
    result = vkEnumerateDeviceExtensionProperties(physical_device,
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
            if (all_extensions_supported)
            {
                all_extensions_supported = false;
                suitable = false;
                std::cout << "    Unsupported extension(s): " << extension_name;
            }
            else
            {
                std::cout << ", " << extension_name;
            }
        }
    }

    if (!all_extensions_supported)
    {
        std::cout << '\n';
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

    vkGetPhysicalDeviceFeatures2(physical_device, &features_2);

    bool all_features_supported {true};

    const auto check_support =
        [&](bool feature_supported, const char *feature_name)
    {
        if (!feature_supported)
        {
            if (all_features_supported)
            {
                all_features_supported = false;
                suitable = false;
                std::cout << "    Unsupported feature(s): " << feature_name;
            }
            else
            {
                std::cout << ", " << feature_name;
            }
        }
    };

    check_support(vulkan_1_2_features.bufferDeviceAddress,
                  "bufferDeviceAddress");
    check_support(vulkan_1_2_features.scalarBlockLayout, "scalarBlockLayout");
    check_support(acceleration_structure_features.accelerationStructure,
                  "accelerationStructure");
    check_support(ray_tracing_pipeline_features.rayTracingPipeline,
                  "rayTracingPipeline");

    if (!all_features_supported)
    {
        std::cout << '\n';
    }

    return suitable;
}

void create_device(Vulkan_context &context)
{
    std::uint32_t physical_device_count {};
    auto result = vkEnumeratePhysicalDevices(
        context.instance, &physical_device_count, nullptr);
    check_result(result, "vkEnumeratePhysicalDevices");
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    result = vkEnumeratePhysicalDevices(
        context.instance, &physical_device_count, physical_devices.data());
    check_result(result, "vkEnumeratePhysicalDevices");

    constexpr const char *device_extension_names[] {
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, // FIXME: disable for
                                                        // release build
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
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

        vkGetPhysicalDeviceProperties2(physical_devices[i], &properties_2);

        std::cout << "Physical device " << i << ": "
                  << properties_2.properties.deviceName << '\n';

        if (is_device_suitable(context.instance,
                               physical_devices[i],
                               device_extension_count,
                               device_extension_names) &&
            !context.physical_device)
        {
            selected_device_index = i;
            context.physical_device = physical_devices[i];
            context.queue_family_indices =
                get_queue_family_indices(context.instance, physical_devices[i]);
            context.physical_device_properties = properties_2.properties;
            context.ray_tracing_pipeline_properties =
                ray_tracing_pipeline_properties;
        }
    }
    if (context.physical_device)
    {
        std::cout << "Selected physical device " << selected_device_index
                  << ": " << context.physical_device_properties.deviceName
                  << '\n';
    }
    else
    {
        throw std::runtime_error("Failed to find a suitable physical device");
    }

    constexpr float queue_priority {1.0f};
    const VkDeviceQueueCreateInfo queue_create_infos[] {
        {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
         .pNext = {},
         .flags = {},
         .queueFamilyIndex = context.queue_family_indices.graphics_compute,
         .queueCount = 1,
         .pQueuePriorities = &queue_priority},
        {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
         .pNext = {},
         .flags = {},
         .queueFamilyIndex = context.queue_family_indices.present,
         .queueCount = 1,
         .pQueuePriorities = &queue_priority}};

    const std::uint32_t queue_create_info_count {
        context.queue_family_indices.graphics_compute ==
                context.queue_family_indices.present
            ? 1u
            : 2u};

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
        .queueCreateInfoCount = queue_create_info_count,
        .pQueueCreateInfos = queue_create_infos,
        .enabledLayerCount = {},
        .ppEnabledLayerNames = {},
        .enabledExtensionCount = device_extension_count,
        .ppEnabledExtensionNames = device_extension_names,
        .pEnabledFeatures = {}};

    result = vkCreateDevice(
        context.physical_device, &device_create_info, nullptr, &context.device);
    check_result(result, "vkCreateDevice");

    volkLoadDevice(context.device);
}

void create_surface(Vulkan_context &context, GLFWwindow *window)
{
    const auto result = glfwCreateWindowSurface(
        context.instance, window, nullptr, &context.surface);
    check_result(result, "glfwCreateWindowSurface");
}

void create_allocator(Vulkan_context &context)
{
    VmaVulkanFunctions vulkan_functions {};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_create_info {};
    allocator_create_info.flags =
        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_create_info.physicalDevice = context.physical_device;
    allocator_create_info.device = context.device;
    allocator_create_info.pVulkanFunctions = &vulkan_functions;
    allocator_create_info.instance = context.instance;
    allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;

    const auto result =
        vmaCreateAllocator(&allocator_create_info, &context.allocator);
    check_result(result, "vmaCreateAllocator");
}

void create_command_pool(Vulkan_context &context)
{
    const VkCommandPoolCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = {},
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.queue_family_indices.graphics_compute};

    const auto result = vkCreateCommandPool(
        context.device, &create_info, nullptr, &context.command_pool);
    check_result(result, "vkCreateCommandPool");
}

[[nodiscard]] VkImageView
create_image_view(VkDevice device, VkImage image, VkFormat format)
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
    const auto result = vkCreateImageView(
        device, &image_view_create_info, nullptr, &image_view);
    check_result(result, "vkCreateImageView");

    return image_view;
}

void destroy_image_view(VkDevice device, VkImageView image_view)
{
    if (image_view)
    {
        vkDestroyImageView(device, image_view, nullptr);
    }
}

void create_swapchain(Vulkan_context &context,
                      std::uint32_t framebuffer_width,
                      std::uint32_t framebuffer_height)
{
    std::uint32_t surface_format_count {};
    auto result = vkGetPhysicalDeviceSurfaceFormatsKHR(context.physical_device,
                                                       context.surface,
                                                       &surface_format_count,
                                                       nullptr);
    check_result(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(context.physical_device,
                                                  context.surface,
                                                  &surface_format_count,
                                                  surface_formats.data());
    check_result(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");

    auto surface_format = surface_formats.front();
    for (const auto &format : surface_formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
        {
            surface_format = format;
            break;
        }
    }

    context.swapchain_format = surface_format.format;

    VkSurfaceCapabilitiesKHR surface_capabilities {};
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        context.physical_device, context.surface, &surface_capabilities);
    check_result(result, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    context.swapchain_extent.width = framebuffer_width;
    context.swapchain_extent.height = framebuffer_height;
    if (surface_capabilities.currentExtent.width !=
        std::numeric_limits<std::uint32_t>::max())
    {
        context.swapchain_extent = surface_capabilities.currentExtent;
    }
    context.swapchain_extent.width =
        std::clamp(context.swapchain_extent.width,
                   surface_capabilities.minImageExtent.width,
                   surface_capabilities.maxImageExtent.width);
    context.swapchain_extent.height =
        std::clamp(context.swapchain_extent.height,
                   surface_capabilities.minImageExtent.height,
                   surface_capabilities.maxImageExtent.height);

    context.swapchain_min_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 &&
        context.swapchain_min_image_count > surface_capabilities.maxImageCount)
    {
        context.swapchain_min_image_count = surface_capabilities.maxImageCount;
    }

    VkSharingMode sharing_mode {VK_SHARING_MODE_EXCLUSIVE};
    std::uint32_t queue_family_index_count {1};
    if (context.queue_family_indices.graphics_compute !=
        context.queue_family_indices.present)
    {
        sharing_mode = VK_SHARING_MODE_CONCURRENT;
        queue_family_index_count = 2;
    }
    const std::uint32_t queue_family_indices[] {
        context.queue_family_indices.graphics_compute,
        context.queue_family_indices.present};

    const VkSwapchainCreateInfoKHR swapchain_create_info {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = {},
        .flags = {},
        .surface = context.surface,
        .minImageCount = context.swapchain_min_image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = context.swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = sharing_mode,
        .queueFamilyIndexCount = queue_family_index_count,
        .pQueueFamilyIndices = queue_family_indices,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = {}};

    result = vkCreateSwapchainKHR(
        context.device, &swapchain_create_info, nullptr, &context.swapchain);
    check_result(result, "vkCreateSwapchainKHR");
    SCOPE_FAIL(
        [&]
        { vkDestroySwapchainKHR(context.device, context.swapchain, nullptr); });

    std::uint32_t image_count {};
    result = vkGetSwapchainImagesKHR(
        context.device, context.swapchain, &image_count, nullptr);
    check_result(result, "vkGetSwapchainImagesKHR");
    context.swapchain_images.resize(image_count);
    result = vkGetSwapchainImagesKHR(context.device,
                                     context.swapchain,
                                     &image_count,
                                     context.swapchain_images.data());
    check_result(result, "vkGetSwapchainImagesKHR");

    context.swapchain_image_views.resize(image_count);
    SCOPE_FAIL(
        [&]
        {
            for (const auto image_view : context.swapchain_image_views)
            {
                destroy_image_view(context.device, image_view);
            }
        });
    for (std::uint32_t i {0}; i < image_count; ++i)
    {
        context.swapchain_image_views[i] =
            create_image_view(context.device,
                              context.swapchain_images[i],
                              context.swapchain_format);
    }
}

void destroy_swapchain(const Vulkan_context &context)
{
    for (const auto image_view : context.swapchain_image_views)
    {
        destroy_image_view(context.device, image_view);
    }

    if (context.swapchain)
    {
        vkDestroySwapchainKHR(context.device, context.swapchain, nullptr);
    }
}

void create_descriptor_pool(Vulkan_context &context)
{
    constexpr VkDescriptorPoolSize pool_sizes[] {
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
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}};

    std::uint32_t max_sets {0};
    for (const auto pool_size : pool_sizes)
    {
        max_sets += pool_size.descriptorCount;
    }

    const VkDescriptorPoolCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .maxSets = max_sets,
        .poolSizeCount = static_cast<std::uint32_t>(std::size(pool_sizes)),
        .pPoolSizes = pool_sizes};

    const auto result = vkCreateDescriptorPool(
        context.device, &create_info, nullptr, &context.descriptor_pool);
    check_result(result, "vkCreateDescriptorPool");
}

void create_render_pass(Vulkan_context &context)
{
    const VkAttachmentDescription attachment_description {
        .flags = {},
        .format = context.swapchain_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

    constexpr VkAttachmentReference attachment_reference {
        .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    const VkSubpassDescription subpass_description {
        .flags = {},
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = {},
        .pInputAttachments = {},
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_reference,
        .pResolveAttachments = {},
        .pDepthStencilAttachment = {},
        .preserveAttachmentCount = {},
        .pPreserveAttachments = {}};

    constexpr VkSubpassDependency subpass_dependency {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = {}};

    const VkRenderPassCreateInfo render_pass_create_info {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .attachmentCount = 1,
        .pAttachments = &attachment_description,
        .subpassCount = 1,
        .pSubpasses = &subpass_description,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency};

    const auto result = vkCreateRenderPass(context.device,
                                           &render_pass_create_info,
                                           nullptr,
                                           &context.render_pass);
    check_result(result, "vkCreateRenderPass");
}

void create_framebuffers(Vulkan_context &context)
{
    context.framebuffers.resize(context.swapchain_image_views.size());

    SCOPE_FAIL(
        [&]
        {
            for (const auto framebuffer : context.framebuffers)
            {
                if (framebuffer)
                {
                    vkDestroyFramebuffer(context.device, framebuffer, nullptr);
                }
            }
        });

    for (std::size_t i {0}; i < context.framebuffers.size(); ++i)
    {
        const VkFramebufferCreateInfo framebuffer_create_info {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = {},
            .flags = {},
            .renderPass = context.render_pass,
            .attachmentCount = 1,
            .pAttachments = &context.swapchain_image_views[i],
            .width = context.swapchain_extent.width,
            .height = context.swapchain_extent.height,
            .layers = 1};

        const auto result = vkCreateFramebuffer(context.device,
                                                &framebuffer_create_info,
                                                nullptr,
                                                &context.framebuffers[i]);
        check_result(result, "vkCreateFramebuffer");
    }
}

void destroy_framebuffers(const Vulkan_context &context)
{
    for (const auto framebuffer : context.framebuffers)
    {
        if (framebuffer)
        {
            vkDestroyFramebuffer(context.device, framebuffer, nullptr);
        }
    }
}

void create_command_buffers(Vulkan_context &context)
{
    const VkCommandBufferAllocateInfo allocate_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = {},
        .commandPool = context.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = Vulkan_context::frames_in_flight};

    const auto result = vkAllocateCommandBuffers(
        context.device, &allocate_info, context.command_buffers.data());
    check_result(result, "vkAllocateCommandBuffers");
}

void create_synchronization_objects(Vulkan_context &context)
{
    constexpr VkSemaphoreCreateInfo semaphore_create_info {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = {},
        .flags = {}};

    // TODO: since we are directly initializing the handles of the context
    // struct, we could actually omit all the SCOPE_FAILs since we have one
    // general SCOPE_FAIL in create_context (there are no longer handles local
    // to the create_* function)

    SCOPE_FAIL(
        [&]
        {
            for (const auto semaphore : context.image_available_semaphores)
            {
                if (semaphore)
                {
                    vkDestroySemaphore(context.device, semaphore, nullptr);
                }
            }
        });

    for (auto &semaphore : context.image_available_semaphores)
    {
        const auto result = vkCreateSemaphore(
            context.device, &semaphore_create_info, nullptr, &semaphore);
        check_result(result, "vkCreateSemaphore");
    }

    SCOPE_FAIL(
        [&]
        {
            for (const auto semaphore : context.render_finished_semaphores)
            {
                if (semaphore)
                {
                    vkDestroySemaphore(context.device, semaphore, nullptr);
                }
            }
        });

    for (auto &semaphore : context.render_finished_semaphores)
    {
        const auto result = vkCreateSemaphore(
            context.device, &semaphore_create_info, nullptr, &semaphore);
        check_result(result, "vkCreateSemaphore");
    }

    constexpr VkFenceCreateInfo fence_create_info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = {},
        .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    SCOPE_FAIL(
        [&]
        {
            for (const auto fence : context.in_flight_fences)
            {
                if (fence)
                {
                    vkDestroyFence(context.device, fence, nullptr);
                }
            }
        });

    for (auto &fence : context.in_flight_fences)
    {
        const auto result =
            vkCreateFence(context.device, &fence_create_info, nullptr, &fence);
        check_result(result, "vkCreateFence");
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
    auto result = vkAllocateCommandBuffers(
        context.device, &allocate_info, &command_buffer);
    check_result(result, "vkAllocateCommandBuffers");

    constexpr VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = {},
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = {}};

    SCOPE_FAIL(
        [&]
        {
            vkFreeCommandBuffers(
                context.device, context.command_pool, 1, &command_buffer);
        });

    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    check_result(result, "vkBeginCommandBuffer");

    return command_buffer;
}

void end_one_time_submit_command_buffer(const Vulkan_context &context,
                                        VkCommandBuffer command_buffer)
{
    SCOPE_EXIT(
        [&]
        {
            vkFreeCommandBuffers(
                context.device, context.command_pool, 1, &command_buffer);
        });

    auto result = vkEndCommandBuffer(command_buffer);
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

    result = vkQueueSubmit(context.graphics_compute_queue, 1, &submit_info, {});
    check_result(result, "vkQueueSubmit");

    result = vkQueueWaitIdle(context.graphics_compute_queue);
    check_result(result, "vkQueueWaitIdle");
}

void init_imgui(Vulkan_context &context)
{
    const auto loader_func = [](const char *function_name, void *user_data)
    {
        const auto ctx = static_cast<const Vulkan_context *>(user_data);
        return vkGetInstanceProcAddr(ctx->instance, function_name);
    };
    ImGui_ImplVulkan_LoadFunctions(loader_func, &context);

    const auto check_vk_result = [](VkResult result)
    { check_result(result, "ImGui Vulkan call"); };

    ImGui_ImplVulkan_InitInfo init_info {
        .Instance = context.instance,
        .PhysicalDevice = context.physical_device,
        .Device = context.device,
        .QueueFamily = context.queue_family_indices.graphics_compute,
        .Queue = context.graphics_compute_queue,
        .PipelineCache = VK_NULL_HANDLE,
        .DescriptorPool = context.descriptor_pool,
        .Subpass = 0,
        .MinImageCount = context.swapchain_min_image_count,
        .ImageCount =
            static_cast<std::uint32_t>(context.swapchain_images.size()),
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .UseDynamicRendering = false,
        .ColorAttachmentFormat = context.swapchain_format,
        .Allocator = nullptr,
        .CheckVkResultFn = check_vk_result};

    ImGui_ImplVulkan_Init(&init_info, context.render_pass);

    const auto command_buffer = begin_one_time_submit_command_buffer(context);
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    end_one_time_submit_command_buffer(context, command_buffer);
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    context.imgui_initialized = true;
}

void recreate_swapchain(Vulkan_context &context)
{
    const auto result = vkDeviceWaitIdle(context.device);
    check_result(result, "vkDeviceWaitIdle");

    destroy_swapchain(context);
    context.swapchain = {};
    create_swapchain(
        context, context.framebuffer_width, context.framebuffer_height);

    destroy_framebuffers(context);
    context.framebuffers = {};
    create_framebuffers(context);
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

void transition_image_layout(VkCommandBuffer command_buffer,
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

    vkCmdPipelineBarrier(command_buffer,
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

[[nodiscard]] VkSampler create_sampler(VkDevice device)
{
    constexpr VkSamplerCreateInfo sampler_create_info {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = {},
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = {},
        .compareEnable = VK_FALSE,
        .compareOp = {},
        .minLod = {},
        .maxLod = {},
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE};

    VkSampler sampler {};
    const auto result =
        vkCreateSampler(device, &sampler_create_info, nullptr, &sampler);
    check_result(result, "vkCreateSampler");

    return sampler;
}

void destroy_sampler(VkDevice device, VkSampler sampler)
{
    if (sampler)
    {
        vkDestroySampler(device, sampler, nullptr);
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

    vkCmdCopyBuffer(
        command_buffer, staging_buffer.buffer, buffer.buffer, 1, &region);

    end_one_time_submit_command_buffer(context, command_buffer);

    return buffer;
}

[[nodiscard]] Vulkan_buffer create_vertex_or_index_buffer(
    const Vulkan_context &context, const void *data, std::size_t size)
{
    return create_buffer_from_host_data(
        context,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        data,
        size);
}

[[nodiscard]] VkDeviceAddress get_device_address(VkDevice device,
                                                 VkBuffer buffer)
{
    const VkBufferDeviceAddressInfo address_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = {},
        .buffer = buffer};
    return vkGetBufferDeviceAddress(device, &address_info);
}

[[nodiscard]] VkDeviceAddress
get_device_address(VkDevice device,
                   VkAccelerationStructureKHR acceleration_structure)
{
    const VkAccelerationStructureDeviceAddressInfoKHR address_info {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .pNext = {},
        .accelerationStructure = acceleration_structure};
    return vkGetAccelerationStructureDeviceAddressKHR(device, &address_info);
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

    vkGetAccelerationStructureBuildSizesKHR(
        context.device,
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

    const auto result =
        vkCreateAccelerationStructureKHR(context.device,
                                         &acceleration_structure_create_info,
                                         nullptr,
                                         &blas.acceleration_structure);
    check_result(result, "vkCreateAccelerationStructureKHR");
    SCOPE_FAIL(
        [&]
        {
            vkDestroyAccelerationStructureKHR(
                context.device, blas.acceleration_structure, nullptr);
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

    vkCmdBuildAccelerationStructuresKHR(
        command_buffer, 1, &build_geometry_info, &p_build_range_info);

    end_one_time_submit_command_buffer(context, command_buffer);

    return blas;
}

[[nodiscard]] Vulkan_acceleration_structure
create_tlas(const Vulkan_context &context)
{
    constexpr VkTransformMatrixKHR transform {
        .matrix = {{1.0f, 0.0f, 0.0f, 0.0f},
                   {0.0f, 1.0f, 0.0f, 0.0f},
                   {0.0f, 0.0f, 1.0f, 0.0f}}};

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

    vkCmdPipelineBarrier(command_buffer,
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
    vkGetAccelerationStructureBuildSizesKHR(
        context.device,
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

    const auto result =
        vkCreateAccelerationStructureKHR(context.device,
                                         &acceleration_structure_create_info,
                                         nullptr,
                                         &tlas.acceleration_structure);
    check_result(result, "vkCreateAccelerationStructureKHR");
    SCOPE_FAIL(
        [&]
        {
            vkDestroyAccelerationStructureKHR(
                context.device, tlas.acceleration_structure, nullptr);
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

    vkCmdBuildAccelerationStructuresKHR(
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
        vkDestroyAccelerationStructureKHR(
            context.device,
            acceleration_structure.acceleration_structure,
            nullptr);
        destroy_buffer(context.allocator, acceleration_structure.buffer);
    }
}

[[nodiscard]] VkDescriptorSetLayout
create_descriptor_set_layout(VkDevice device)
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
        vkCreateDescriptorSetLayout(device,
                                    &descriptor_set_layout_create_info,
                                    nullptr,
                                    &descriptor_set_layout);
    check_result(result, "vkCreateDescriptorSetLayout");

    return descriptor_set_layout;
}

[[nodiscard]] VkDescriptorSetLayout
create_final_render_descriptor_set_layout(VkDevice device)
{
    constexpr VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] {
        {.binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
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
        vkCreateDescriptorSetLayout(device,
                                    &descriptor_set_layout_create_info,
                                    nullptr,
                                    &descriptor_set_layout);
    check_result(result, "vkCreateDescriptorSetLayout");

    return descriptor_set_layout;
}

void destroy_descriptor_set_layout(VkDevice device,
                                   VkDescriptorSetLayout descriptor_set_layout)
{
    if (descriptor_set_layout)
    {
        vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
    }
}

[[nodiscard]] VkDescriptorSet
create_descriptor_set(const Vulkan_context &context)
{
    const VkDescriptorSetAllocateInfo descriptor_set_allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = {},
        .descriptorPool = context.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &context.descriptor_set_layout};

    VkDescriptorSet descriptor_set {};
    const auto result = vkAllocateDescriptorSets(
        context.device, &descriptor_set_allocate_info, &descriptor_set);
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

    vkUpdateDescriptorSets(
        context.device,
        static_cast<std::uint32_t>(std::size(descriptor_writes)),
        descriptor_writes,
        0,
        nullptr);

    return descriptor_set;
}

[[nodiscard]] VkDescriptorSet
create_final_render_descriptor_set(const Vulkan_context &context)
{
    const VkDescriptorSetAllocateInfo descriptor_set_allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = {},
        .descriptorPool = context.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &context.final_render_descriptor_set_layout};

    VkDescriptorSet descriptor_set {};
    const auto result = vkAllocateDescriptorSets(
        context.device, &descriptor_set_allocate_info, &descriptor_set);
    check_result(result, "vkAllocateDescriptorSets");

    const VkDescriptorImageInfo descriptor_render_target {
        .sampler = context.render_target_sampler,
        .imageView = context.render_target_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    const VkWriteDescriptorSet descriptor_write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = {},
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &descriptor_render_target,
        .pBufferInfo = {},
        .pTexelBufferView = {}};

    vkUpdateDescriptorSets(context.device, 1, &descriptor_write, 0, nullptr);

    return descriptor_set;
}

[[nodiscard]] VkPipelineLayout
create_ray_tracing_pipeline_layout(const Vulkan_context &context)
{
    constexpr VkPushConstantRange push_constant_range {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
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
    const auto result = vkCreatePipelineLayout(context.device,
                                               &pipeline_layout_create_info,
                                               nullptr,
                                               &pipeline_layout);
    check_result(result, "vkCreatePipelineLayout");

    return pipeline_layout;
}

void destroy_pipeline_layout(VkDevice device, VkPipelineLayout pipeline_layout)
{
    if (pipeline_layout)
    {
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    }
}

[[nodiscard]] VkShaderModule create_shader_module(VkDevice device,
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
    const auto result = vkCreateShaderModule(
        device, &shader_module_create_info, nullptr, &shader_module);
    check_result(result, "vkCreateShaderModule");

    return shader_module;
}

void destroy_shader_module(VkDevice device, VkShaderModule shader_module)
{
    if (shader_module)
    {
        vkDestroyShaderModule(device, shader_module, nullptr);
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
    const auto result =
        vkCreateRayTracingPipelinesKHR(context.device,
                                       {},
                                       {},
                                       1,
                                       &ray_tracing_pipeline_create_info,
                                       nullptr,
                                       &ray_tracing_pipeline);
    check_result(result, "vkCreateRayTracingPipelinesKHR");

    return ray_tracing_pipeline;
}

void destroy_pipeline(VkDevice device, VkPipeline pipeline)
{
    if (pipeline)
    {
        vkDestroyPipeline(device, pipeline, nullptr);
    }
}

void create_shader_binding_table(Vulkan_context &context)
{
    const auto handle_size =
        context.ray_tracing_pipeline_properties.shaderGroupHandleSize;
    const auto handle_alignment =
        context.ray_tracing_pipeline_properties.shaderGroupHandleAlignment;
    const auto base_alignment =
        context.ray_tracing_pipeline_properties.shaderGroupBaseAlignment;
    const auto handle_size_aligned = align_up(handle_size, handle_alignment);

    constexpr std::uint32_t miss_count {1};
    constexpr std::uint32_t hit_count {1};
    constexpr std::uint32_t handle_count {1 + miss_count + hit_count};

    context.sbt_raygen_region.stride =
        align_up(handle_size_aligned, base_alignment);
    context.sbt_raygen_region.size = context.sbt_raygen_region.stride;

    context.sbt_miss_region.stride = handle_size_aligned;
    context.sbt_miss_region.size =
        align_up(miss_count * handle_size_aligned, base_alignment);

    context.sbt_hit_region.stride = handle_size_aligned;
    context.sbt_hit_region.size =
        align_up(hit_count * handle_size_aligned, base_alignment);

    const auto data_size = handle_count * handle_size;
    std::vector<std::uint8_t> handles(data_size);
    const auto result =
        vkGetRayTracingShaderGroupHandlesKHR(context.device,
                                             context.ray_tracing_pipeline,
                                             0,
                                             handle_count,
                                             data_size,
                                             handles.data());
    check_result(result, "vkGetRayTracingShaderGroupHandlesKHR");

    const auto sbt_size =
        context.sbt_raygen_region.size + context.sbt_miss_region.size +
        context.sbt_hit_region.size + context.sbt_callable_region.size;

    VmaAllocationInfo sbt_allocation_info {};
    context.sbt_buffer =
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
        get_device_address(context.device, context.sbt_buffer.buffer);
    context.sbt_raygen_region.deviceAddress = sbt_address;
    context.sbt_miss_region.deviceAddress =
        sbt_address + context.sbt_raygen_region.size;
    context.sbt_hit_region.deviceAddress = sbt_address +
                                           context.sbt_raygen_region.size +
                                           context.sbt_miss_region.size;

    const auto get_handle_pointer = [&](std::uint32_t i)
    { return handles.data() + i * handle_size; };

    std::uint32_t handle_index {0};
    std::memcpy(
        sbt_buffer_mapped, get_handle_pointer(handle_index), handle_size);
    ++handle_index;

    auto p_data = sbt_buffer_mapped + context.sbt_raygen_region.size;
    for (std::uint32_t i {}; i < miss_count; ++i)
    {
        std::memcpy(p_data, get_handle_pointer(handle_index), handle_size);
        ++handle_index;
        p_data += context.sbt_miss_region.stride;
    }

    p_data = sbt_buffer_mapped + context.sbt_raygen_region.size +
             context.sbt_miss_region.size;
    for (std::uint32_t i {}; i < hit_count; ++i)
    {
        std::memcpy(p_data, get_handle_pointer(handle_index), handle_size);
        ++handle_index;
        p_data += context.sbt_hit_region.stride;
    }
}

void destroy_shader_binding_table(const Vulkan_context &context)
{
    destroy_buffer(context.allocator, context.sbt_buffer);
}

} // namespace

Vulkan_context create_vulkan_context(GLFWwindow *window)
{
    Vulkan_context context {};

    SCOPE_FAIL([&] { destroy_vulkan_context(context); });

    create_instance(context);
    create_device(context);
    vkGetDeviceQueue(context.device,
                     context.queue_family_indices.graphics_compute,
                     0,
                     &context.graphics_compute_queue);
    vkGetDeviceQueue(context.device,
                     context.queue_family_indices.present,
                     0,
                     &context.present_queue);
    create_surface(context, window);
    create_allocator(context);
    create_command_pool(context);

    int width {};
    int height {};
    glfwGetFramebufferSize(window, &width, &height);
    context.framebuffer_width = static_cast<std::uint32_t>(width);
    context.framebuffer_height = static_cast<std::uint32_t>(height);
    create_swapchain(
        context, context.framebuffer_width, context.framebuffer_height);

    create_descriptor_pool(context);
    create_render_pass(context);
    create_framebuffers(context);
    create_command_buffers(context);
    create_synchronization_objects(context);

    init_imgui(context);

    return context;
}

void destroy_vulkan_context(Vulkan_context &context)
{
    if (context.imgui_initialized)
    {
        ImGui_ImplVulkan_Shutdown();
    }

    for (const auto semaphore : context.render_finished_semaphores)
    {
        if (semaphore)
        {
            vkDestroySemaphore(context.device, semaphore, nullptr);
        }
    }

    for (const auto semaphore : context.image_available_semaphores)
    {
        if (semaphore)
        {
            vkDestroySemaphore(context.device, semaphore, nullptr);
        }
    }

    for (const auto fence : context.in_flight_fences)
    {
        if (fence)
        {
            vkDestroyFence(context.device, fence, nullptr);
        }
    }

    vkFreeCommandBuffers(
        context.device,
        context.command_pool,
        static_cast<std::uint32_t>(context.command_buffers.size()),
        context.command_buffers.data());

    destroy_framebuffers(context);

    if (context.render_pass)
    {
        vkDestroyRenderPass(context.device, context.render_pass, nullptr);
    }

    if (context.descriptor_pool)
    {
        vkDestroyDescriptorPool(
            context.device, context.descriptor_pool, nullptr);
    }

    destroy_swapchain(context);

    if (context.command_pool)
    {
        vkDestroyCommandPool(context.device, context.command_pool, nullptr);
    }

    vmaDestroyAllocator(context.allocator);

    if (context.surface)
    {
        vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    }

    if (context.device)
    {
        vkDestroyDevice(context.device, nullptr);
    }

    if (context.instance)
    {
#ifdef ENABLE_VALIDATION_LAYERS
        vkDestroyDebugUtilsMessengerEXT(
            context.instance, context.debug_messenger, nullptr);
#endif
        vkDestroyInstance(context.instance, nullptr);

        volkFinalize();
    }

    context = {};
}

void draw_frame(Vulkan_context &context, const Camera &camera)
{
    if (context.framebuffer_width == 0 || context.framebuffer_height == 0)
    {
        // FIXME: we really shouldn't be waiting here, because we want to
        // continue tracing when the window is minimized, just not draw to the
        // framebuffer
        glfwWaitEvents();
        return;
    }

    auto result = vkWaitForFences(
        context.device,
        1,
        &context.in_flight_fences[context.current_frame_in_flight],
        VK_TRUE,
        std::numeric_limits<std::uint64_t>::max());
    check_result(result, "vkWaitForFences");

    std::uint32_t image_index {};
    result = vkAcquireNextImageKHR(
        context.device,
        context.swapchain,
        std::numeric_limits<std::uint64_t>::max(),
        context.image_available_semaphores[context.current_frame_in_flight],
        {},
        &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate_swapchain(context);
        return;
    }
    else if (result != VK_SUBOPTIMAL_KHR)
    {
        check_result(result, "vkAcquireNextImageKHR");
    }

    result = vkResetFences(
        context.device,
        1,
        &context.in_flight_fences[context.current_frame_in_flight]);
    check_result(result, "vkResetFences");

    const auto command_buffer =
        context.command_buffers[context.current_frame_in_flight];

    result = vkResetCommandBuffer(command_buffer, {});
    check_result(result, "vkResetCommandBuffer");

    constexpr VkCommandBufferBeginInfo command_buffer_begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = {},
        .flags = {},
        .pInheritanceInfo = {}};

    result = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
    check_result(result, "vkBeginCommandBuffer");

    constexpr VkImageSubresourceRange subresource_range {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    // If a scene is loaded
    if (context.storage_image.image)
    {
        if (context.sample_count == 0)
        {
            constexpr VkClearColorValue clear_value {
                .float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
            vkCmdClearColorImage(command_buffer,
                                 context.storage_image.image,
                                 VK_IMAGE_LAYOUT_GENERAL,
                                 &clear_value,
                                 1,
                                 &subresource_range);

            const VkImageMemoryBarrier image_memory_barrier {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = {},
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask =
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = context.storage_image.image,
                .subresourceRange = subresource_range};

            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                 {},
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &image_memory_barrier);
        }

        if (context.sample_count < context.samples_to_render)
        {
            vkCmdBindPipeline(command_buffer,
                              VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                              context.ray_tracing_pipeline);

            vkCmdBindDescriptorSets(command_buffer,
                                    VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                    context.ray_tracing_pipeline_layout,
                                    0,
                                    1,
                                    &context.descriptor_set,
                                    0,
                                    nullptr);

            const auto samples_this_frame =
                std::min(context.samples_to_render - context.sample_count,
                         context.samples_per_frame);

            const Push_constants push_constants {
                .global_frame_count = context.global_frame_count,
                .sample_count = context.sample_count,
                .samples_per_frame = samples_this_frame,
                .camera_position = camera.position,
                .camera_dir_x = camera.direction_x,
                .camera_dir_y = camera.direction_y,
                .camera_dir_z = camera.direction_z};

            vkCmdPushConstants(command_buffer,
                               context.ray_tracing_pipeline_layout,
                               VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                               0,
                               sizeof(Push_constants),
                               &push_constants);
            context.sample_count += samples_this_frame;

            vkCmdTraceRaysKHR(command_buffer,
                              &context.sbt_raygen_region,
                              &context.sbt_miss_region,
                              &context.sbt_hit_region,
                              &context.sbt_callable_region,
                              context.storage_image.width,
                              context.storage_image.height,
                              1);

            VkImageMemoryBarrier image_memory_barriers[] {
                {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                 .pNext = {},
                 .srcAccessMask =
                     VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                 .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                 .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                 .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                 .image = context.storage_image.image,
                 .subresourceRange = subresource_range},
                {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                 .pNext = {},
                 .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                 .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                 .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                 .image = context.render_target.image,
                 .subresourceRange = subresource_range}};

            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
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
                .srcOffsets =
                    {{0, 0, 0},
                     {static_cast<std::int32_t>(context.render_target.width),
                      static_cast<std::int32_t>(context.render_target.height),
                      1}},
                .dstSubresource = subresource_layers,
                .dstOffsets = {
                    {0, 0, 0},
                    {static_cast<std::int32_t>(context.render_target.width),
                     static_cast<std::int32_t>(context.render_target.height),
                     1}}};

            vkCmdBlitImage(command_buffer,
                           context.storage_image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           context.render_target.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &image_blit,
                           VK_FILTER_NEAREST);

            image_memory_barriers[0].srcAccessMask =
                VK_ACCESS_TRANSFER_READ_BIT;
            image_memory_barriers[0].dstAccessMask =
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            image_memory_barriers[0].oldLayout =
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            image_memory_barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            image_memory_barriers[1].srcAccessMask =
                VK_ACCESS_TRANSFER_WRITE_BIT;
            image_memory_barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            image_memory_barriers[1].oldLayout =
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            image_memory_barriers[1].newLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                {},
                0,
                nullptr,
                0,
                nullptr,
                static_cast<std::uint32_t>(std::size(image_memory_barriers)),
                image_memory_barriers);
        }
    }

    constexpr VkClearValue clear_value {
        .color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}};

    const VkRenderPassBeginInfo render_pass_begin_info {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = {},
        .renderPass = context.render_pass,
        .framebuffer = context.framebuffers[image_index],
        .renderArea = {.offset = {0, 0}, .extent = context.swapchain_extent},
        .clearValueCount = 1,
        .pClearValues = &clear_value};

    vkCmdBeginRenderPass(
        command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);

    vkCmdEndRenderPass(command_buffer);

    result = vkEndCommandBuffer(command_buffer);
    check_result(result, "vkEndCommandBuffer");

    constexpr VkPipelineStageFlags wait_stage {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    const VkSubmitInfo submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = {},
        .waitSemaphoreCount = 1,
        .pWaitSemaphores =
            &context
                 .image_available_semaphores[context.current_frame_in_flight],
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores =
            &context
                 .render_finished_semaphores[context.current_frame_in_flight]};

    result = vkQueueSubmit(
        context.graphics_compute_queue,
        1,
        &submit_info,
        context.in_flight_fences[context.current_frame_in_flight]);
    check_result(result, "vkQueueSubmit");

    const VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = {},
        .waitSemaphoreCount = 1,
        .pWaitSemaphores =
            &context
                 .render_finished_semaphores[context.current_frame_in_flight],
        .swapchainCount = 1,
        .pSwapchains = &context.swapchain,
        .pImageIndices = &image_index,
        .pResults = {}};

    result = vkQueuePresentKHR(context.present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        context.framebuffer_resized)
    {
        context.framebuffer_resized = false;
        recreate_swapchain(context);
    }
    else
    {
        check_result(result, "vkQueuePresentKHR");
    }

    context.current_frame_in_flight = (context.current_frame_in_flight + 1) %
                                      Vulkan_context::frames_in_flight;
    ++context.global_frame_count;
}

void resize_framebuffer(Vulkan_context &context,
                        std::uint32_t width,
                        std::uint32_t height)
{
    context.framebuffer_resized = true;
    context.framebuffer_width = width;
    context.framebuffer_height = height;
}

void wait_idle(const Vulkan_context &context)
{
    const auto result = vkDeviceWaitIdle(context.device);
    check_result(result, "vkDeviceWaitIdle");
}

void create_scene_resources(Vulkan_context &context,
                            std::uint32_t render_width,
                            std::uint32_t render_height,
                            const aiScene *scene)
{
    const auto *const mesh = scene->mMeshes[0];
    std::vector<std::uint32_t> indices(mesh->mNumFaces * 3);
    for (unsigned int i {0}; i < mesh->mNumFaces; ++i)
    {
        indices[i * 3 + 0] = mesh->mFaces[i].mIndices[0];
        indices[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
        indices[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
    }

    SCOPE_FAIL([&] { destroy_scene_resources(context); });

    constexpr VkFormat storage_image_format {VK_FORMAT_R32G32B32A32_SFLOAT};
    context.storage_image = create_image(context.allocator,
                                         render_width,
                                         render_height,
                                         storage_image_format,
                                         VK_IMAGE_USAGE_STORAGE_BIT |
                                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    context.storage_image_view = create_image_view(
        context.device, context.storage_image.image, storage_image_format);

    constexpr VkFormat render_target_format {VK_FORMAT_R8G8B8A8_SRGB};
    context.render_target = create_image(context.allocator,
                                         render_width,
                                         render_height,
                                         render_target_format,
                                         VK_IMAGE_USAGE_SAMPLED_BIT |
                                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    context.render_target_view = create_image_view(
        context.device, context.render_target.image, render_target_format);

    {
        const auto command_buffer =
            begin_one_time_submit_command_buffer(context);
        transition_image_layout(command_buffer,
                                context.storage_image.image,
                                VK_ACCESS_NONE,
                                VK_ACCESS_SHADER_READ_BIT |
                                    VK_ACCESS_SHADER_WRITE_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_GENERAL,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
                                    VK_PIPELINE_STAGE_TRANSFER_BIT);
        transition_image_layout(command_buffer,
                                context.render_target.image,
                                VK_ACCESS_NONE,
                                VK_ACCESS_SHADER_READ_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
        end_one_time_submit_command_buffer(context, command_buffer);
    }

    context.render_target_sampler = create_sampler(context.device);

    context.vertex_buffer = create_vertex_or_index_buffer(
        context,
        mesh->mVertices,
        mesh->mNumVertices * sizeof(mesh->mVertices[0]));

    context.index_buffer = create_vertex_or_index_buffer(
        context, indices.data(), indices.size() * sizeof(indices.front()));

    context.blas = create_blas(context);

    context.tlas = create_tlas(context);

    context.descriptor_set_layout =
        create_descriptor_set_layout(context.device);

    context.final_render_descriptor_set_layout =
        create_final_render_descriptor_set_layout(context.device);

    context.descriptor_set = create_descriptor_set(context);

    context.final_render_descriptor_set =
        create_final_render_descriptor_set(context);

    context.ray_tracing_pipeline_layout =
        create_ray_tracing_pipeline_layout(context);

    context.ray_tracing_pipeline = create_ray_tracing_pipeline(context);

    create_shader_binding_table(context);

    context.samples_to_render = 1000;
    context.sample_count = 0;
    context.samples_per_frame = 1;
}

void destroy_scene_resources(const Vulkan_context &context)
{
    destroy_shader_binding_table(context);
    destroy_pipeline(context.device, context.ray_tracing_pipeline);
    destroy_pipeline_layout(context.device,
                            context.ray_tracing_pipeline_layout);
    destroy_descriptor_set_layout(context.device,
                                  context.final_render_descriptor_set_layout);
    destroy_descriptor_set_layout(context.device,
                                  context.descriptor_set_layout);
    destroy_acceleration_structure(context, context.tlas);
    destroy_acceleration_structure(context, context.blas);
    destroy_buffer(context.allocator, context.index_buffer);
    destroy_buffer(context.allocator, context.vertex_buffer);
    destroy_sampler(context.device, context.render_target_sampler);
    destroy_image_view(context.device, context.render_target_view);
    destroy_image(context.allocator, context.render_target);
    destroy_image_view(context.device, context.storage_image_view);
    destroy_image(context.allocator, context.storage_image);
}

void reset_render(Vulkan_context &context)
{
    context.sample_count = 0;
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

    // Transition for the blit
    VkImageMemoryBarrier image_memory_barriers[] {
        {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
         .pNext = {},
         .srcAccessMask =
             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
         .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
         .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
         .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .image = context.storage_image.image,
         .subresourceRange = subresource_range},
        {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
         .pNext = {},
         .srcAccessMask = VK_ACCESS_NONE,
         .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
         .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
         .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .image = final_image.image,
         .subresourceRange = subresource_range}};

    vkCmdPipelineBarrier(
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

    vkCmdBlitImage(command_buffer,
                   context.storage_image.image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   final_image.image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &image_blit,
                   VK_FILTER_NEAREST);

    // Revert the storage image transition, and transition the final image for
    // the transfer to CPU memory
    image_memory_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_memory_barriers[0].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    image_memory_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_memory_barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_memory_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_memory_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_memory_barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_memory_barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT |
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        {},
        0,
        nullptr,
        0,
        nullptr,
        static_cast<std::uint32_t>(std::size(image_memory_barriers)),
        image_memory_barriers);

    const VkBufferImageCopy copy_region {
        .bufferOffset = 0,
        .bufferRowLength = {},
        .bufferImageHeight = final_image.height,
        .imageSubresource = subresource_layers,
        .imageOffset = {0, 0, 0},
        .imageExtent = {final_image.width, final_image.height, 1}};

    vkCmdCopyImageToBuffer(command_buffer,
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
