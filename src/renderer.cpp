#include "renderer.hpp"
#include "camera.hpp"
#include "utility.hpp"

#include <imgui_impl_vulkan.h>

#include <assimp/scene.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <volk.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

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

#ifdef ENABLE_VALIDATION

VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data [[maybe_unused]])
{
    std::ostringstream message;

    switch (message_severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        message << "[VERBOSE]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        message << "[INFO]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        message << "[WARNING]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        message << "[ERROR]";
        break;
    default: break;
    }

    std::ostringstream types;
    if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
    {
        types << "GENERAL|";
    }
    if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
    {
        types << "VALIDATION|";
    }
    if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
        types << "PERFORMANCE|";
    }
    if (message_type &
        VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT)
    {
        types << "DEVICE_ADDRESS_BINDING|";
    }

    if (auto types_str = types.str(); types_str.empty())
    {
        message << ' ';
    }
    else
    {
        message << '[' << types_str.erase(types_str.size() - 1) << "] ";
    }

    message << callback_data->pMessage;

    std::cout << message.str() << std::endl;

    return VK_FALSE;
}

#endif

void check_result(VkResult result, const char *message)
{
    if (result != VK_SUCCESS)
    {
        std::ostringstream oss;
        oss << message << ": " << vk::to_string(vk::Result(result));
        throw std::runtime_error(oss.str());
    }
}

void create_instance(Vulkan_context &context)
{
    VULKAN_HPP_DEFAULT_DISPATCHER.init(
        context.dl.getProcAddress<PFN_vkGetInstanceProcAddr>(
            "vkGetInstanceProcAddr"));

    constexpr vk::ApplicationInfo application_info {.apiVersion =
                                                        VK_API_VERSION_1_3};

    std::uint32_t glfw_required_extension_count {};
    const auto glfw_required_extension_names =
        glfwGetRequiredInstanceExtensions(&glfw_required_extension_count);

#ifdef ENABLE_VALIDATION

    const auto layer_properties = vk::enumerateInstanceLayerProperties();

    constexpr auto layer_name = "VK_LAYER_KHRONOS_validation";
    if (std::none_of(layer_properties.begin(),
                     layer_properties.end(),
                     [layer_name](const vk::LayerProperties &properties) {
                         return std::strcmp(properties.layerName, layer_name) ==
                                0;
                     }))
    {
        std::ostringstream message;
        message << layer_name << " is not supported\n";
        throw std::runtime_error(message.str());
    }

#endif

    const auto extension_properties =
        vk::enumerateInstanceExtensionProperties();

#ifdef ENABLE_VALIDATION

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
                [extension_name](const vk::ExtensionProperties &properties) {
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

#ifdef ENABLE_VALIDATION

    constexpr vk::DebugUtilsMessengerCreateInfoEXT
        debug_utils_messenger_create_info {
            .messageSeverity =
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType =
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding,
            .pfnUserCallback = &debug_callback};

    constexpr vk::ValidationFeatureEnableEXT enabled_validation_features[] {
        vk::ValidationFeatureEnableEXT::eDebugPrintf,
        vk::ValidationFeatureEnableEXT::eSynchronizationValidation};

    const vk::ValidationFeaturesEXT validation_features {
        .pNext = &debug_utils_messenger_create_info,
        .enabledValidationFeatureCount =
            static_cast<std::uint32_t>(std::size(enabled_validation_features)),
        .pEnabledValidationFeatures = enabled_validation_features};

    const vk::InstanceCreateInfo instance_create_info {
        .pNext = &validation_features,
        .pApplicationInfo = &application_info,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &layer_name,
        .enabledExtensionCount =
            static_cast<std::uint32_t>(required_extension_names.size()),
        .ppEnabledExtensionNames = required_extension_names.data()};

#else

    const vk::InstanceCreateInfo instance_create_info {
        .pApplicationInfo = &application_info,
        .enabledExtensionCount =
            static_cast<std::uint32_t>(required_extension_names.size()),
        .ppEnabledExtensionNames = required_extension_names.data()};

#endif

    context.instance = vk::createInstanceUnique(instance_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(context.instance.get());

#ifdef ENABLE_VALIDATION

    context.debug_messenger =
        context.instance->createDebugUtilsMessengerEXTUnique(
            debug_utils_messenger_create_info);

#endif
}

void get_queue_family_indices(
    VkInstance instance,
    VkPhysicalDevice physical_device,
    std::uint32_t &graphics_compute_queue_family_index,
    std::uint32_t &present_queue_family_index)
{
    graphics_compute_queue_family_index =
        std::numeric_limits<std::uint32_t>::max();
    present_queue_family_index = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t queue_family_property_count {};
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_property_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_properties(
        queue_family_property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_property_count,
                                             queue_family_properties.data());

    for (std::uint32_t i {0}; i < queue_family_property_count; ++i)
    {
        if ((queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            (queue_family_properties[i].queueCount > 0))
        {
            graphics_compute_queue_family_index = i;
        }
        if (glfwGetPhysicalDevicePresentationSupport(
                instance, physical_device, i))
        {
            present_queue_family_index = i;
        }
        if (graphics_compute_queue_family_index !=
                std::numeric_limits<std::uint32_t>::max() &&
            present_queue_family_index !=
                std::numeric_limits<std::uint32_t>::max())
        {
            return;
        }
    }
}

[[nodiscard]] bool is_device_suitable(VkInstance instance,
                                      VkPhysicalDevice physical_device,
                                      std::uint32_t device_extension_count,
                                      const char *const *device_extension_names)
{
    bool suitable {true};

    std::uint32_t graphics_compute_queue_family_index;
    std::uint32_t present_queue_family_index;
    get_queue_family_indices(instance,
                             physical_device,
                             graphics_compute_queue_family_index,
                             present_queue_family_index);
    if (graphics_compute_queue_family_index ==
        std::numeric_limits<std::uint32_t>::max())
    {
        suitable = false;
        std::cout
            << "    No queue family supports graphics and compute operations\n";
    }
    if (present_queue_family_index == std::numeric_limits<std::uint32_t>::max())
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
        *context.instance, &physical_device_count, nullptr);
    check_result(result, "vkEnumeratePhysicalDevices");
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    result = vkEnumeratePhysicalDevices(
        *context.instance, &physical_device_count, physical_devices.data());
    check_result(result, "vkEnumeratePhysicalDevices");

    constexpr const char *device_extension_names[] {
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

        if (is_device_suitable(*context.instance,
                               physical_devices[i],
                               device_extension_count,
                               device_extension_names) &&
            !context.physical_device)
        {
            selected_device_index = i;
            context.physical_device = physical_devices[i];
            get_queue_family_indices(
                *context.instance,
                physical_devices[i],
                context.graphics_compute_queue_family_index,
                context.present_queue_family_index);
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
         .queueFamilyIndex = context.graphics_compute_queue_family_index,
         .queueCount = 1,
         .pQueuePriorities = &queue_priority},
        {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
         .pNext = {},
         .flags = {},
         .queueFamilyIndex = context.present_queue_family_index,
         .queueCount = 1,
         .pQueuePriorities = &queue_priority}};

    const std::uint32_t queue_create_info_count {
        context.graphics_compute_queue_family_index ==
                context.present_queue_family_index
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
}

void create_surface(Vulkan_context &context, GLFWwindow *window)
{
    const auto result = glfwCreateWindowSurface(
        *context.instance, window, nullptr, &context.surface);
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
    allocator_create_info.instance = *context.instance;
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
        .queueFamilyIndex = context.graphics_compute_queue_family_index};

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

void create_swapchain(Vulkan_context &context)
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

    context.swapchain_extent.width = context.framebuffer_width;
    context.swapchain_extent.height = context.framebuffer_height;
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
    if (context.graphics_compute_queue_family_index !=
        context.present_queue_family_index)
    {
        sharing_mode = VK_SHARING_MODE_CONCURRENT;
        queue_family_index_count = 2;
    }
    const std::uint32_t queue_family_indices[] {
        context.graphics_compute_queue_family_index,
        context.present_queue_family_index};

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
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
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

    for (auto &semaphore : context.image_available_semaphores)
    {
        const auto result = vkCreateSemaphore(
            context.device, &semaphore_create_info, nullptr, &semaphore);
        check_result(result, "vkCreateSemaphore");
    }

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
        return vkGetInstanceProcAddr(*ctx->instance, function_name);
    };
    ImGui_ImplVulkan_LoadFunctions(loader_func, &context);

    const auto check_vk_result = [](VkResult result)
    { check_result(result, "ImGui Vulkan call"); };

    ImGui_ImplVulkan_InitInfo init_info {
        .Instance = *context.instance,
        .PhysicalDevice = context.physical_device,
        .Device = context.device,
        .QueueFamily = context.graphics_compute_queue_family_index,
        .Queue = context.graphics_compute_queue,
        .DescriptorPool = context.descriptor_pool,
        .RenderPass = context.render_pass,
        .MinImageCount = context.swapchain_min_image_count,
        .ImageCount =
            static_cast<std::uint32_t>(context.swapchain_images.size()),
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .PipelineCache = VK_NULL_HANDLE,
        .Subpass = 0,
        .UseDynamicRendering = false,
#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
        .PipelineRenderingCreateInfo = {},
#endif
        .Allocator = {},
        .CheckVkResultFn = check_vk_result,
        .MinAllocationSize = 1024 * 1024};

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();

    context.imgui_initialized = true;
}

void recreate_swapchain(Vulkan_context &context)
{
    const auto result = vkDeviceWaitIdle(context.device);
    check_result(result, "vkDeviceWaitIdle");

    destroy_framebuffers(context);
    destroy_swapchain(context);

    create_swapchain(context);
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
    if (image.image || image.allocation)
    {
        vmaDestroyImage(allocator, image.image, image.allocation);
    }
}

void create_sampler(const Vulkan_context &context,
                    Vulkan_render_resources &render_resources)
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

    const auto result =
        vkCreateSampler(context.device,
                        &sampler_create_info,
                        nullptr,
                        &render_resources.render_target_sampler);
    check_result(result, "vkCreateSampler");
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
    if (buffer.buffer || buffer.allocation)
    {
        vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    }
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

void create_blas(const Vulkan_context &context,
                 Vulkan_render_resources &render_resources)
{
    const auto vertex_buffer_address = get_device_address(
        context.device, render_resources.vertex_buffer.buffer);
    constexpr auto vertex_size = 3 * sizeof(float);
    const auto vertex_count = render_resources.vertex_buffer.size / vertex_size;

    const auto index_buffer_address = get_device_address(
        context.device, render_resources.index_buffer.buffer);
    const auto index_count =
        render_resources.index_buffer.size / sizeof(std::uint32_t);
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

    render_resources.blas_buffer =
        create_buffer(context.allocator,
                      build_sizes_info.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      {},
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      nullptr);

    const VkAccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = {},
            .createFlags = {},
            .buffer = render_resources.blas_buffer.buffer,
            .offset = {},
            .size = build_sizes_info.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .deviceAddress = {}};

    const auto result =
        vkCreateAccelerationStructureKHR(context.device,
                                         &acceleration_structure_create_info,
                                         nullptr,
                                         &render_resources.blas);
    check_result(result, "vkCreateAccelerationStructureKHR");

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

    build_geometry_info.dstAccelerationStructure = render_resources.blas;
    build_geometry_info.scratchData.deviceAddress = scratch_buffer_address;

    const auto command_buffer = begin_one_time_submit_command_buffer(context);

    const auto *const p_build_range_info = &build_range_info;

    vkCmdBuildAccelerationStructuresKHR(
        command_buffer, 1, &build_geometry_info, &p_build_range_info);

    end_one_time_submit_command_buffer(context, command_buffer);
}

void create_tlas(const Vulkan_context &context,
                 Vulkan_render_resources &render_resources)
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
        .accelerationStructureReference =
            get_device_address(context.device, render_resources.blas)};

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

    render_resources.tlas_buffer =
        create_buffer(context.allocator,
                      build_sizes_info.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      {},
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      nullptr);

    const VkAccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = {},
            .createFlags = {},
            .buffer = render_resources.tlas_buffer.buffer,
            .offset = {},
            .size = build_sizes_info.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .deviceAddress = {}};

    const auto result =
        vkCreateAccelerationStructureKHR(context.device,
                                         &acceleration_structure_create_info,
                                         nullptr,
                                         &render_resources.tlas);
    check_result(result, "vkCreateAccelerationStructureKHR");

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

    build_geometry_info.dstAccelerationStructure = render_resources.tlas;
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
}

void create_descriptor_set_layout(const Vulkan_context &context,
                                  Vulkan_render_resources &render_resources)
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

    const auto result =
        vkCreateDescriptorSetLayout(context.device,
                                    &descriptor_set_layout_create_info,
                                    nullptr,
                                    &render_resources.descriptor_set_layout);
    check_result(result, "vkCreateDescriptorSetLayout");
}

void create_final_render_descriptor_set_layout(
    const Vulkan_context &context, Vulkan_render_resources &render_resources)
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

    const auto result = vkCreateDescriptorSetLayout(
        context.device,
        &descriptor_set_layout_create_info,
        nullptr,
        &render_resources.final_render_descriptor_set_layout);
    check_result(result, "vkCreateDescriptorSetLayout");
}

void create_descriptor_set(const Vulkan_context &context,
                           Vulkan_render_resources &render_resources)
{
    const VkDescriptorSetAllocateInfo descriptor_set_allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = {},
        .descriptorPool = context.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &render_resources.descriptor_set_layout};

    const auto result =
        vkAllocateDescriptorSets(context.device,
                                 &descriptor_set_allocate_info,
                                 &render_resources.descriptor_set);
    check_result(result, "vkAllocateDescriptorSets");

    const VkDescriptorImageInfo descriptor_storage_image {
        .sampler = VK_NULL_HANDLE,
        .imageView = render_resources.storage_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

    const VkWriteDescriptorSetAccelerationStructureKHR
        descriptor_acceleration_structure {
            .sType =
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .pNext = {},
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &render_resources.tlas};

    const VkDescriptorBufferInfo descriptor_vertices {
        .buffer = render_resources.vertex_buffer.buffer,
        .offset = 0,
        .range = render_resources.vertex_buffer.size};

    const VkDescriptorBufferInfo descriptor_indices {
        .buffer = render_resources.index_buffer.buffer,
        .offset = 0,
        .range = render_resources.index_buffer.size};

    const VkWriteDescriptorSet descriptor_writes[4] {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = {},
         .dstSet = render_resources.descriptor_set,
         .dstBinding = 0,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .pImageInfo = &descriptor_storage_image,
         .pBufferInfo = {},
         .pTexelBufferView = {}},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = &descriptor_acceleration_structure,
         .dstSet = render_resources.descriptor_set,
         .dstBinding = 1,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
         .pImageInfo = {},
         .pBufferInfo = {},
         .pTexelBufferView = {}},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = {},
         .dstSet = render_resources.descriptor_set,
         .dstBinding = 2,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .pImageInfo = {},
         .pBufferInfo = &descriptor_vertices,
         .pTexelBufferView = {}},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = {},
         .dstSet = render_resources.descriptor_set,
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
}

void create_final_render_descriptor_set(
    const Vulkan_context &context, Vulkan_render_resources &render_resources)
{
    const VkDescriptorSetAllocateInfo descriptor_set_allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = {},
        .descriptorPool = context.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &render_resources.final_render_descriptor_set_layout};

    const auto result =
        vkAllocateDescriptorSets(context.device,
                                 &descriptor_set_allocate_info,
                                 &render_resources.final_render_descriptor_set);
    check_result(result, "vkAllocateDescriptorSets");

    const VkDescriptorImageInfo descriptor_render_target {
        .sampler = render_resources.render_target_sampler,
        .imageView = render_resources.render_target_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    const VkWriteDescriptorSet descriptor_write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = {},
        .dstSet = render_resources.final_render_descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &descriptor_render_target,
        .pBufferInfo = {},
        .pTexelBufferView = {}};

    vkUpdateDescriptorSets(context.device, 1, &descriptor_write, 0, nullptr);
}

void create_ray_tracing_pipeline_layout(
    const Vulkan_context &context, Vulkan_render_resources &render_resources)
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
        .pSetLayouts = &render_resources.descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range};

    const auto result =
        vkCreatePipelineLayout(context.device,
                               &pipeline_layout_create_info,
                               nullptr,
                               &render_resources.ray_tracing_pipeline_layout);
    check_result(result, "vkCreatePipelineLayout");
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

void create_ray_tracing_pipeline(const Vulkan_context &context,
                                 Vulkan_render_resources &render_resources)
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
        .layout = render_resources.ray_tracing_pipeline_layout,
        .basePipelineHandle = {},
        .basePipelineIndex = {}};

    const auto result =
        vkCreateRayTracingPipelinesKHR(context.device,
                                       {},
                                       {},
                                       1,
                                       &ray_tracing_pipeline_create_info,
                                       nullptr,
                                       &render_resources.ray_tracing_pipeline);
    check_result(result, "vkCreateRayTracingPipelinesKHR");
}

void create_shader_binding_table(const Vulkan_context &context,
                                 Vulkan_render_resources &render_resources)
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

    render_resources.sbt_raygen_region.stride =
        align_up(handle_size_aligned, base_alignment);
    render_resources.sbt_raygen_region.size =
        render_resources.sbt_raygen_region.stride;

    render_resources.sbt_miss_region.stride = handle_size_aligned;
    render_resources.sbt_miss_region.size =
        align_up(miss_count * handle_size_aligned, base_alignment);

    render_resources.sbt_hit_region.stride = handle_size_aligned;
    render_resources.sbt_hit_region.size =
        align_up(hit_count * handle_size_aligned, base_alignment);

    const auto data_size = handle_count * handle_size;
    std::vector<std::uint8_t> handles(data_size);
    const auto result = vkGetRayTracingShaderGroupHandlesKHR(
        context.device,
        render_resources.ray_tracing_pipeline,
        0,
        handle_count,
        data_size,
        handles.data());
    check_result(result, "vkGetRayTracingShaderGroupHandlesKHR");

    const auto sbt_size = render_resources.sbt_raygen_region.size +
                          render_resources.sbt_miss_region.size +
                          render_resources.sbt_hit_region.size +
                          render_resources.sbt_callable_region.size;

    VmaAllocationInfo sbt_allocation_info {};
    render_resources.sbt_buffer =
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
        get_device_address(context.device, render_resources.sbt_buffer.buffer);
    render_resources.sbt_raygen_region.deviceAddress = sbt_address;
    render_resources.sbt_miss_region.deviceAddress =
        sbt_address + render_resources.sbt_raygen_region.size;
    render_resources.sbt_hit_region.deviceAddress =
        sbt_address + render_resources.sbt_raygen_region.size +
        render_resources.sbt_miss_region.size;

    const auto get_handle_pointer = [&](std::uint32_t i)
    { return handles.data() + i * handle_size; };

    std::uint32_t handle_index {0};
    std::memcpy(
        sbt_buffer_mapped, get_handle_pointer(handle_index), handle_size);
    ++handle_index;

    auto p_data = sbt_buffer_mapped + render_resources.sbt_raygen_region.size;
    for (std::uint32_t i {}; i < miss_count; ++i)
    {
        std::memcpy(p_data, get_handle_pointer(handle_index), handle_size);
        ++handle_index;
        p_data += render_resources.sbt_miss_region.stride;
    }

    p_data = sbt_buffer_mapped + render_resources.sbt_raygen_region.size +
             render_resources.sbt_miss_region.size;
    for (std::uint32_t i {}; i < hit_count; ++i)
    {
        std::memcpy(p_data, get_handle_pointer(handle_index), handle_size);
        ++handle_index;
        p_data += render_resources.sbt_hit_region.stride;
    }
}

} // namespace

Vulkan_context create_context(GLFWwindow *window)
{
    Vulkan_context context {};

    SCOPE_FAIL([&] { destroy_context(context); });

    create_instance(context);

    volkInitializeCustom(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr);
    volkLoadInstanceOnly(*context.instance);

    create_device(context);

    volkLoadDevice(context.device);

    vkGetDeviceQueue(context.device,
                     context.graphics_compute_queue_family_index,
                     0,
                     &context.graphics_compute_queue);
    vkGetDeviceQueue(context.device,
                     context.present_queue_family_index,
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
    create_swapchain(context);

    create_descriptor_pool(context);
    create_render_pass(context);
    create_framebuffers(context);
    create_command_buffers(context);
    create_synchronization_objects(context);

    init_imgui(context);

    return context;
}

void destroy_context(Vulkan_context &context)
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

    if (context.command_buffers.front())
    {
        vkFreeCommandBuffers(
            context.device,
            context.command_pool,
            static_cast<std::uint32_t>(context.command_buffers.size()),
            context.command_buffers.data());
    }

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
        vkDestroySurfaceKHR(*context.instance, context.surface, nullptr);
    }

    if (context.device)
    {
        vkDestroyDevice(context.device, nullptr);
    }
}

Vulkan_render_resources create_render_resources(const Vulkan_context &context,
                                                std::uint32_t render_width,
                                                std::uint32_t render_height,
                                                const aiScene *scene)
{
    Vulkan_render_resources render_resources {};

    const auto *const mesh = scene->mMeshes[0];
    std::vector<std::uint32_t> indices(mesh->mNumFaces * 3);
    for (unsigned int i {0}; i < mesh->mNumFaces; ++i)
    {
        indices[i * 3 + 0] = mesh->mFaces[i].mIndices[0];
        indices[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
        indices[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
    }

    SCOPE_FAIL([&] { destroy_render_resources(context, render_resources); });

    constexpr VkFormat storage_image_format {VK_FORMAT_R32G32B32A32_SFLOAT};
    render_resources.storage_image = create_image(
        context.allocator,
        render_width,
        render_height,
        storage_image_format,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    render_resources.storage_image_view =
        create_image_view(context.device,
                          render_resources.storage_image.image,
                          storage_image_format);

    constexpr VkFormat render_target_format {VK_FORMAT_R8G8B8A8_SRGB};
    render_resources.render_target = create_image(
        context.allocator,
        render_width,
        render_height,
        render_target_format,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    render_resources.render_target_view =
        create_image_view(context.device,
                          render_resources.render_target.image,
                          render_target_format);

    {
        const auto command_buffer =
            begin_one_time_submit_command_buffer(context);

        constexpr VkImageSubresourceRange subresource_range {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1};

        const VkImageMemoryBarrier image_memory_barriers[] {
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
             .pNext = {},
             .srcAccessMask = VK_ACCESS_NONE,
             .dstAccessMask =
                 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_GENERAL,
             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .image = render_resources.storage_image.image,
             .subresourceRange = subresource_range},
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
             .pNext = {},
             .srcAccessMask = VK_ACCESS_NONE,
             .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .image = render_resources.render_target.image,
             .subresourceRange = subresource_range}};

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
                VK_PIPELINE_STAGE_TRANSFER_BIT,
            {},
            0,
            nullptr,
            0,
            nullptr,
            static_cast<std::uint32_t>(std::size(image_memory_barriers)),
            image_memory_barriers);

        end_one_time_submit_command_buffer(context, command_buffer);
    }

    create_sampler(context, render_resources);

    render_resources.vertex_buffer = create_vertex_or_index_buffer(
        context,
        mesh->mVertices,
        mesh->mNumVertices * sizeof(mesh->mVertices[0]));

    render_resources.index_buffer = create_vertex_or_index_buffer(
        context, indices.data(), indices.size() * sizeof(indices.front()));

    create_blas(context, render_resources);
    create_tlas(context, render_resources);
    create_descriptor_set_layout(context, render_resources);
    create_final_render_descriptor_set_layout(context, render_resources);
    create_descriptor_set(context, render_resources);
    create_final_render_descriptor_set(context, render_resources);
    create_ray_tracing_pipeline_layout(context, render_resources);
    create_ray_tracing_pipeline(context, render_resources);
    create_shader_binding_table(context, render_resources);

    render_resources.samples_to_render = 1000;
    render_resources.sample_count = 0;
    render_resources.samples_per_frame = 1;

    return render_resources;
}

void destroy_render_resources(const Vulkan_context &context,
                              Vulkan_render_resources &render_resources)
{
    destroy_buffer(context.allocator, render_resources.sbt_buffer);

    if (render_resources.ray_tracing_pipeline)
    {
        vkDestroyPipeline(
            context.device, render_resources.ray_tracing_pipeline, nullptr);
    }

    if (render_resources.ray_tracing_pipeline_layout)
    {
        vkDestroyPipelineLayout(context.device,
                                render_resources.ray_tracing_pipeline_layout,
                                nullptr);
    }

    if (render_resources.final_render_descriptor_set_layout)
    {
        vkDestroyDescriptorSetLayout(
            context.device,
            render_resources.final_render_descriptor_set_layout,
            nullptr);
    }

    if (render_resources.descriptor_set_layout)
    {
        vkDestroyDescriptorSetLayout(
            context.device, render_resources.descriptor_set_layout, nullptr);
    }

    if (render_resources.tlas)
    {
        vkDestroyAccelerationStructureKHR(
            context.device, render_resources.tlas, nullptr);
    }
    destroy_buffer(context.allocator, render_resources.tlas_buffer);

    if (render_resources.blas)
    {
        vkDestroyAccelerationStructureKHR(
            context.device, render_resources.blas, nullptr);
    }
    destroy_buffer(context.allocator, render_resources.blas_buffer);

    destroy_buffer(context.allocator, render_resources.index_buffer);
    destroy_buffer(context.allocator, render_resources.vertex_buffer);

    if (render_resources.render_target_sampler)
    {
        vkDestroySampler(
            context.device, render_resources.render_target_sampler, nullptr);
    }

    destroy_image_view(context.device, render_resources.render_target_view);
    destroy_image(context.allocator, render_resources.render_target);
    destroy_image_view(context.device, render_resources.storage_image_view);
    destroy_image(context.allocator, render_resources.storage_image);

    render_resources = {};
}

void draw_frame(Vulkan_context &context,
                Vulkan_render_resources &render_resources,
                const Camera &camera)
{
    if (context.framebuffer_width == 0 || context.framebuffer_height == 0)
    {
        // FIXME: We want to
        // continue tracing when the window is minimized, just not draw to the
        // framebuffer. But this requires a better separation of the tracing vs
        // drawing code.
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
    if (render_resources.storage_image.image)
    {
        if (render_resources.sample_count == 0)
        {
            constexpr VkClearColorValue clear_value {
                .float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
            vkCmdClearColorImage(command_buffer,
                                 render_resources.storage_image.image,
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
                .image = render_resources.storage_image.image,
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

        if (render_resources.sample_count < render_resources.samples_to_render)
        {
            vkCmdBindPipeline(command_buffer,
                              VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                              render_resources.ray_tracing_pipeline);

            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                render_resources.ray_tracing_pipeline_layout,
                0,
                1,
                &render_resources.descriptor_set,
                0,
                nullptr);

            const auto samples_this_frame =
                std::min(render_resources.samples_to_render -
                             render_resources.sample_count,
                         render_resources.samples_per_frame);

            const Push_constants push_constants {
                .global_frame_count = context.global_frame_count,
                .sample_count = render_resources.sample_count,
                .samples_per_frame = samples_this_frame,
                .camera_position = camera.position,
                .camera_dir_x = camera.direction_x,
                .camera_dir_y = camera.direction_y,
                .camera_dir_z = camera.direction_z};

            vkCmdPushConstants(command_buffer,
                               render_resources.ray_tracing_pipeline_layout,
                               VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                               0,
                               sizeof(Push_constants),
                               &push_constants);
            render_resources.sample_count += samples_this_frame;

            vkCmdTraceRaysKHR(command_buffer,
                              &render_resources.sbt_raygen_region,
                              &render_resources.sbt_miss_region,
                              &render_resources.sbt_hit_region,
                              &render_resources.sbt_callable_region,
                              render_resources.storage_image.width,
                              render_resources.storage_image.height,
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
                 .image = render_resources.storage_image.image,
                 .subresourceRange = subresource_range},
                {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                 .pNext = {},
                 .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                 .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                 .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                 .image = render_resources.render_target.image,
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
                .srcOffsets = {{0, 0, 0},
                               {static_cast<std::int32_t>(
                                    render_resources.render_target.width),
                                static_cast<std::int32_t>(
                                    render_resources.render_target.height),
                                1}},
                .dstSubresource = subresource_layers,
                .dstOffsets = {{0, 0, 0},
                               {static_cast<std::int32_t>(
                                    render_resources.render_target.width),
                                static_cast<std::int32_t>(
                                    render_resources.render_target.height),
                                1}}};

            vkCmdBlitImage(command_buffer,
                           render_resources.storage_image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           render_resources.render_target.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &image_blit,
                           VK_FILTER_NEAREST);

            std::swap(image_memory_barriers[0].srcAccessMask,
                      image_memory_barriers[0].dstAccessMask);
            std::swap(image_memory_barriers[0].oldLayout,
                      image_memory_barriers[0].newLayout);
            std::swap(image_memory_barriers[1].srcAccessMask,
                      image_memory_barriers[1].dstAccessMask);
            std::swap(image_memory_barriers[1].oldLayout,
                      image_memory_barriers[1].newLayout);

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

void reset_render(Vulkan_render_resources &render_resources)
{
    render_resources.sample_count = 0;
}

std::string write_to_png(const Vulkan_context &context,
                         const Vulkan_render_resources &render_resources,
                         const char *file_name)
{
    VmaAllocationInfo staging_allocation_info {};
    const auto staging_buffer =
        create_buffer(context.allocator,
                      render_resources.render_target.width *
                          render_resources.render_target.height * 4,
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

    VkImageMemoryBarrier image_memory_barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = {},
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = render_resources.render_target.image,
        .subresourceRange = subresource_range};

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

    constexpr VkImageSubresourceLayers subresource_layers {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1};

    const VkBufferImageCopy copy_region {
        .bufferOffset = 0,
        .bufferRowLength = {},
        .bufferImageHeight = render_resources.render_target.height,
        .imageSubresource = subresource_layers,
        .imageOffset = {0, 0, 0},
        .imageExtent = {render_resources.render_target.width,
                        render_resources.render_target.height,
                        1}};

    vkCmdCopyImageToBuffer(command_buffer,
                           render_resources.render_target.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           staging_buffer.buffer,
                           1,
                           &copy_region);

    std::swap(image_memory_barrier.srcAccessMask,
              image_memory_barrier.dstAccessMask);
    std::swap(image_memory_barrier.oldLayout, image_memory_barrier.newLayout);

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

    end_one_time_submit_command_buffer(context, command_buffer);

    return write_png(file_name,
                     mapped_data,
                     static_cast<int>(render_resources.render_target.width),
                     static_cast<int>(render_resources.render_target.height));
}
