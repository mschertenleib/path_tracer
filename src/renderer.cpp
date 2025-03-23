#include "renderer.hpp"
#include "camera.hpp"
#include "utility.hpp"

#include <imgui_impl_vulkan.h>

#include <assimp/scene.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
    float sensor_distance;
    float sensor_half_width;
    float sensor_half_height;
    float focus_distance;
    float aperture_radius;
};
static_assert(sizeof(Push_constants) <= 128);

#ifdef ENABLE_VALIDATION

VKAPI_ATTR vk::Bool32 VKAPI_CALL
debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
               vk::DebugUtilsMessageTypeFlagsEXT message_type,
               const vk::DebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data [[maybe_unused]])
{
    auto type = vk::to_string(message_type);
    if (type.starts_with("{ ") && type.ends_with(" }"))
    {
        type.assign(type.begin() + 2, type.end() - 2);
    }
    std::cout << '[' << vk::to_string(message_severity) << "][" << type << "] "
              << callback_data->pMessage << '\n';

    return VK_FALSE;
}

#endif

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
    if (std::none_of(
            layer_properties.begin(),
            layer_properties.end(),
            [layer_name](const vk::LayerProperties &properties)
            { return std::strcmp(properties.layerName, layer_name) == 0; }))
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
                [extension_name](const vk::ExtensionProperties &properties)
                {
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

    constexpr vk::ValidationFeatureEnableEXT enabled_validation_features[] {
        vk::ValidationFeatureEnableEXT::eDebugPrintf,
        vk::ValidationFeatureEnableEXT::eSynchronizationValidation};

    const vk::StructureChain create_info_chain {
        vk::InstanceCreateInfo {
            .pApplicationInfo = &application_info,
            .enabledLayerCount = 1,
            .ppEnabledLayerNames = &layer_name,
            .enabledExtensionCount =
                static_cast<std::uint32_t>(required_extension_names.size()),
            .ppEnabledExtensionNames = required_extension_names.data()},
        vk::ValidationFeaturesEXT {
            .enabledValidationFeatureCount = static_cast<std::uint32_t>(
                std::size(enabled_validation_features)),
            .pEnabledValidationFeatures = enabled_validation_features},
        vk::DebugUtilsMessengerCreateInfoEXT {
            .messageSeverity =
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType =
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding,
            .pfnUserCallback = &debug_callback}};

    context.instance = vk::createInstanceUnique(
        create_info_chain.get<vk::InstanceCreateInfo>());

    VULKAN_HPP_DEFAULT_DISPATCHER.init(context.instance.get());

    context.debug_messenger =
        context.instance->createDebugUtilsMessengerEXTUnique(
            create_info_chain.get<vk::DebugUtilsMessengerCreateInfoEXT>());

#else

    const vk::InstanceCreateInfo instance_create_info {
        .pApplicationInfo = &application_info,
        .enabledExtensionCount =
            static_cast<std::uint32_t>(required_extension_names.size()),
        .ppEnabledExtensionNames = required_extension_names.data()};

    context.instance = vk::createInstanceUnique(instance_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(context.instance.get());

#endif
}

void get_queue_family_indices(
    vk::Instance instance,
    vk::PhysicalDevice physical_device,
    std::uint32_t &graphics_compute_queue_family_index,
    std::uint32_t &present_queue_family_index)
{
    graphics_compute_queue_family_index =
        std::numeric_limits<std::uint32_t>::max();
    present_queue_family_index = std::numeric_limits<std::uint32_t>::max();

    const auto queue_family_properties =
        physical_device.getQueueFamilyProperties();

    for (std::uint32_t i {0};
         i < static_cast<std::uint32_t>(queue_family_properties.size());
         ++i)
    {
        if ((queue_family_properties[i].queueFlags &
             vk::QueueFlagBits::eGraphics) &&
            (queue_family_properties[i].queueFlags &
             vk::QueueFlagBits::eCompute) &&
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

[[nodiscard]] bool is_device_suitable(vk::Instance instance,
                                      vk::PhysicalDevice physical_device,
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

    const auto extension_properties =
        physical_device.enumerateDeviceExtensionProperties();

    bool all_extensions_supported {true};
    for (std::uint32_t i {}; i < device_extension_count; ++i)
    {
        const auto extension_name = device_extension_names[i];
        if (std::none_of(
                extension_properties.begin(),
                extension_properties.end(),
                [extension_name](const vk::ExtensionProperties &properties)
                {
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

    const auto features_chain =
        physical_device
            .getFeatures2<vk::PhysicalDeviceFeatures2,
                          vk::PhysicalDeviceVulkan12Features,
                          vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
                          vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();

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

    check_support(features_chain.get<vk::PhysicalDeviceVulkan12Features>()
                      .bufferDeviceAddress,
                  "bufferDeviceAddress");
    check_support(features_chain.get<vk::PhysicalDeviceVulkan12Features>()
                      .scalarBlockLayout,
                  "scalarBlockLayout");
    check_support(
        features_chain.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>()
            .accelerationStructure,
        "accelerationStructure");
    check_support(
        features_chain.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>()
            .rayTracingPipeline,
        "rayTracingPipeline");

    if (!all_features_supported)
    {
        std::cout << '\n';
    }

    const auto limits = physical_device.getProperties().limits;
    if (limits.timestampPeriod == 0.0f)
    {
        std::cout << "    Timestamp queries not supported by the device\n";
        suitable = false;
    }
    if (!limits.timestampComputeAndGraphics)
    {
        const auto queue_family_properties =
            physical_device.getQueueFamilyProperties()
                [graphics_compute_queue_family_index];
        if (queue_family_properties.timestampValidBits == 0)
        {
            std::cout << "    Timestamp queries not supported by the "
                         "graphics/compute queue family\n";
            suitable = false;
        }
    }

    return suitable;
}

void create_device(Vulkan_context &context)
{
    const auto physical_devices = context.instance->enumeratePhysicalDevices();

    constexpr const char *device_extension_names[] {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};
    constexpr auto device_extension_count =
        static_cast<std::uint32_t>(std::size(device_extension_names));

    std::size_t selected_device_index {};
    for (std::size_t i {0}; i < physical_devices.size(); ++i)
    {
        const auto properties_chain =
            physical_devices[i]
                .getProperties2<
                    vk::PhysicalDeviceProperties2,
                    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

        const auto &properties =
            properties_chain.get<vk::PhysicalDeviceProperties2>().properties;

        std::cout << "Physical device " << i << " ("
                  << vk::to_string(properties.deviceType)
                  << "): " << properties.deviceName << '\n';

        if (is_device_suitable(context.instance.get(),
                               physical_devices[i],
                               device_extension_count,
                               device_extension_names) &&
            !context.physical_device)
        {
            selected_device_index = i;
            context.physical_device = physical_devices[i];
            get_queue_family_indices(
                context.instance.get(),
                physical_devices[i],
                context.graphics_compute_queue_family_index,
                context.present_queue_family_index);
            context.physical_device_properties = properties;
            context.physical_device_ray_tracing_pipeline_properties =
                properties_chain
                    .get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
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
    const vk::DeviceQueueCreateInfo queue_create_infos[] {
        {.queueFamilyIndex = context.graphics_compute_queue_family_index,
         .queueCount = 1,
         .pQueuePriorities = &queue_priority},
        {.queueFamilyIndex = context.present_queue_family_index,
         .queueCount = 1,
         .pQueuePriorities = &queue_priority}};

    const std::uint32_t queue_create_info_count {
        context.graphics_compute_queue_family_index ==
                context.present_queue_family_index
            ? 1u
            : 2u};

    const vk::StructureChain create_info_chain {
        vk::DeviceCreateInfo {.queueCreateInfoCount = queue_create_info_count,
                              .pQueueCreateInfos = queue_create_infos,
                              .enabledExtensionCount = device_extension_count,
                              .ppEnabledExtensionNames =
                                  device_extension_names},
        vk::PhysicalDeviceFeatures2 {},
        vk::PhysicalDeviceVulkan12Features {.scalarBlockLayout = VK_TRUE,
                                            .bufferDeviceAddress = VK_TRUE},
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR {
            .accelerationStructure = VK_TRUE},
        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR {.rayTracingPipeline =
                                                             VK_TRUE}};

    context.device = context.physical_device.createDeviceUnique(
        create_info_chain.get<vk::DeviceCreateInfo>());
}

void create_surface(Vulkan_context &context, GLFWwindow *window)
{
    VkSurfaceKHR surface {};
    const auto result = glfwCreateWindowSurface(
        context.instance.get(), window, nullptr, &surface);
    vk::detail::resultCheck(vk::Result {result}, "glfwCreateWindowSurface");
    context.surface = vk::UniqueSurfaceKHR(surface, context.instance.get());
}

void create_allocator(Vulkan_context &context)
{
    VmaVulkanFunctions vulkan_functions {};
    vulkan_functions.vkGetInstanceProcAddr =
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
    vulkan_functions.vkGetDeviceProcAddr =
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_create_info {};
    allocator_create_info.flags =
        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_create_info.physicalDevice = context.physical_device;
    allocator_create_info.device = context.device.get();
    allocator_create_info.pVulkanFunctions = &vulkan_functions;
    allocator_create_info.instance = context.instance.get();
    allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;

    VmaAllocator allocator {};
    const auto result = vmaCreateAllocator(&allocator_create_info, &allocator);
    vk::detail::resultCheck(vk::Result {result}, "vmaCreateAllocator");

    context.allocator = Unique_allocator(allocator);
}

void create_command_pool(Vulkan_context &context)
{
    const vk::CommandPoolCreateInfo create_info {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = context.graphics_compute_queue_family_index};

    context.command_pool = context.device->createCommandPoolUnique(create_info);
}

[[nodiscard]] vk::UniqueImageView
create_image_view(vk::Device device, vk::Image image, vk::Format format)
{
    constexpr vk::ImageSubresourceRange subresource_range {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    const vk::ImageViewCreateInfo image_view_create_info {
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .components = {vk::ComponentSwizzle::eIdentity,
                       vk::ComponentSwizzle::eIdentity,
                       vk::ComponentSwizzle::eIdentity,
                       vk::ComponentSwizzle::eIdentity},
        .subresourceRange = subresource_range};

    return device.createImageViewUnique(image_view_create_info);
}

void create_swapchain(Vulkan_context &context)
{
    const auto surface_formats =
        context.physical_device.getSurfaceFormatsKHR(context.surface.get());

    auto surface_format = surface_formats.front();
    for (const auto &format : surface_formats)
    {
        if (format.format == vk::Format::eB8G8R8A8Srgb &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            surface_format = format;
            break;
        }
    }

    context.swapchain_format = surface_format.format;

    const auto surface_capabilities =
        context.physical_device.getSurfaceCapabilitiesKHR(
            context.surface.get());

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

    auto sharing_mode = vk::SharingMode::eExclusive;
    std::uint32_t queue_family_index_count {1};
    if (context.graphics_compute_queue_family_index !=
        context.present_queue_family_index)
    {
        sharing_mode = vk::SharingMode::eConcurrent;
        queue_family_index_count = 2;
    }
    const std::uint32_t queue_family_indices[] {
        context.graphics_compute_queue_family_index,
        context.present_queue_family_index};

    const vk::SwapchainCreateInfoKHR swapchain_create_info {
        .surface = context.surface.get(),
        .minImageCount = context.swapchain_min_image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = context.swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = sharing_mode,
        .queueFamilyIndexCount = queue_family_index_count,
        .pQueueFamilyIndices = queue_family_indices,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eFifo,
        .clipped = VK_TRUE};

    context.swapchain =
        context.device->createSwapchainKHRUnique(swapchain_create_info);

    context.swapchain_images =
        context.device->getSwapchainImagesKHR(context.swapchain.get());

    context.swapchain_image_views.resize(context.swapchain_images.size());
    for (std::size_t i {0}; i < context.swapchain_image_views.size(); ++i)
    {
        context.swapchain_image_views[i] =
            create_image_view(context.device.get(),
                              context.swapchain_images[i],
                              context.swapchain_format);
    }
}

void create_descriptor_pool(Vulkan_context &context)
{
    // FIXME: ImGui actually only needs a single combined image sampler, so just
    // count however many samplers/buffers we need in our ray tracing shaders
    // Actually, that probably means we have to create a specific descriptor
    // pool for ray tracing render resources, since we might not know the number
    // of textures in the scene (or is there a maximum ? Maybe we could use
    // image arrays for many PBR texture cases)
    constexpr vk::DescriptorPoolSize pool_sizes[] {
        {vk::DescriptorType::eSampler, 1000},
        {vk::DescriptorType::eCombinedImageSampler, 1000},
        {vk::DescriptorType::eSampledImage, 1000},
        {vk::DescriptorType::eStorageImage, 1000},
        {vk::DescriptorType::eUniformTexelBuffer, 1000},
        {vk::DescriptorType::eStorageTexelBuffer, 1000},
        {vk::DescriptorType::eUniformBuffer, 1000},
        {vk::DescriptorType::eStorageBuffer, 1000},
        {vk::DescriptorType::eUniformBufferDynamic, 1000},
        {vk::DescriptorType::eStorageBufferDynamic, 1000},
        {vk::DescriptorType::eInputAttachment, 1000},
        {vk::DescriptorType::eAccelerationStructureKHR, 1}};

    std::uint32_t max_sets {0};
    for (const auto pool_size : pool_sizes)
    {
        max_sets += pool_size.descriptorCount;
    }

    const vk::DescriptorPoolCreateInfo create_info {
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = max_sets,
        .poolSizeCount = static_cast<std::uint32_t>(std::size(pool_sizes)),
        .pPoolSizes = pool_sizes};

    context.descriptor_pool =
        context.device->createDescriptorPoolUnique(create_info);
}

void create_render_pass(Vulkan_context &context)
{
    const vk::AttachmentDescription attachment_description {
        .format = context.swapchain_format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::ePresentSrcKHR};

    constexpr vk::AttachmentReference attachment_reference {
        .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};

    const vk::SubpassDescription subpass_description {
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_reference};

    constexpr vk::SubpassDependency subpass_dependency {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite};

    const vk::RenderPassCreateInfo render_pass_create_info {
        .attachmentCount = 1,
        .pAttachments = &attachment_description,
        .subpassCount = 1,
        .pSubpasses = &subpass_description,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency};

    context.render_pass =
        context.device->createRenderPassUnique(render_pass_create_info);
}

void create_framebuffers(Vulkan_context &context)
{
    context.framebuffers.resize(context.swapchain_image_views.size());

    for (std::size_t i {0}; i < context.framebuffers.size(); ++i)
    {
        const vk::FramebufferCreateInfo framebuffer_create_info {
            .renderPass = context.render_pass.get(),
            .attachmentCount = 1,
            .pAttachments = &context.swapchain_image_views[i].get(),
            .width = context.swapchain_extent.width,
            .height = context.swapchain_extent.height,
            .layers = 1};

        context.framebuffers[i] =
            context.device->createFramebufferUnique(framebuffer_create_info);
    }
}

void create_command_buffers(Vulkan_context &context)
{
    const vk::CommandBufferAllocateInfo allocate_info {
        .commandPool = context.command_pool.get(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = Vulkan_context::frames_in_flight};

    context.command_buffers =
        context.device->allocateCommandBuffersUnique(allocate_info);
}

void create_synchronization_objects(Vulkan_context &context)
{
    constexpr vk::SemaphoreCreateInfo semaphore_create_info {};

    for (auto &semaphore : context.image_available_semaphores)
    {
        semaphore =
            context.device->createSemaphoreUnique(semaphore_create_info);
    }

    for (auto &semaphore : context.render_finished_semaphores)
    {
        semaphore =
            context.device->createSemaphoreUnique(semaphore_create_info);
    }

    constexpr vk::FenceCreateInfo fence_create_info {
        .flags = vk::FenceCreateFlagBits::eSignaled};

    for (auto &fence : context.in_flight_fences)
    {
        fence = context.device->createFenceUnique(fence_create_info);
    }
}

void init_imgui(Vulkan_context &context)
{
    const auto loader_func = [](const char *function_name, void *user_data)
    {
        return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
            static_cast<const Vulkan_context *>(user_data)->instance.get(),
            function_name);
    };
    ImGui_ImplVulkan_LoadFunctions(loader_func, &context);

    const auto check_vk_result = [](VkResult result)
    { vk::detail::resultCheck(vk::Result {result}, "ImGui Vulkan call"); };

    ImGui_ImplVulkan_InitInfo init_info {
        .Instance = context.instance.get(),
        .PhysicalDevice = context.physical_device,
        .Device = context.device.get(),
        .QueueFamily = context.graphics_compute_queue_family_index,
        .Queue = context.graphics_compute_queue,
        .DescriptorPool = context.descriptor_pool.get(),
        .RenderPass = context.render_pass.get(),
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

    context.imgui_backend = ImGui_backend(true);
}

void recreate_swapchain(Vulkan_context &context)
{
    context.device->waitIdle();

    context.framebuffers.clear();
    context.swapchain_image_views.clear();
    context.swapchain.reset();

    create_swapchain(context);
    create_framebuffers(context);
}

[[nodiscard]] Vulkan_image create_image(VmaAllocator allocator,
                                        vk::Device device,
                                        std::uint32_t width,
                                        std::uint32_t height,
                                        vk::Format format,
                                        vk::ImageUsageFlags usage)
{
    Vulkan_image image {};
    image.width = width;
    image.height = height;

    const vk::ImageCreateInfo image_create_info {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage vk_image {};
    VmaAllocation allocation {};
    const auto result = vmaCreateImage(
        allocator,
        &static_cast<const VkImageCreateInfo &>(image_create_info),
        &allocation_create_info,
        &vk_image,
        &allocation,
        nullptr);
    vk::detail::resultCheck(vk::Result {result}, "vmaCreateImage");

    image.image = vk::UniqueImage(vk_image, device);
    image.allocation = Unique_allocation(allocation, allocator);

    return image;
}

void create_sampler(const Vulkan_context &context,
                    Vulkan_render_resources &render_resources)
{
    constexpr vk::SamplerCreateInfo sampler_create_info {
        .magFilter = vk::Filter::eNearest,
        .minFilter = vk::Filter::eNearest,
        .mipmapMode = vk::SamplerMipmapMode::eNearest,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .anisotropyEnable = VK_FALSE,
        .compareEnable = VK_FALSE,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = VK_FALSE};

    render_resources.render_target_sampler =
        context.device->createSamplerUnique(sampler_create_info);
}

[[nodiscard]] Vulkan_buffer
create_buffer(VmaAllocator allocator,
              vk::Device device,
              vk::DeviceSize size,
              vk::BufferUsageFlags usage,
              VmaAllocationCreateFlags allocation_flags,
              VmaMemoryUsage memory_usage,
              VmaAllocationInfo *allocation_info)
{
    Vulkan_buffer buffer {};
    buffer.size = size;

    const vk::BufferCreateInfo buffer_create_info {
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.flags = allocation_flags;
    allocation_create_info.usage = memory_usage;

    VkBuffer vk_buffer {};
    VmaAllocation allocation {};
    const auto result = vmaCreateBuffer(
        allocator,
        &static_cast<const VkBufferCreateInfo &>(buffer_create_info),
        &allocation_create_info,
        &vk_buffer,
        &allocation,
        allocation_info);
    vk::detail::resultCheck(vk::Result {result}, "vmaCreateBuffer");

    buffer.buffer = vk::UniqueBuffer(vk_buffer, device);
    buffer.allocation = Unique_allocation(allocation, allocator);

    return buffer;
}

[[nodiscard]] vk::UniqueCommandBuffer
begin_one_time_submit_command_buffer(const Vulkan_context &context)
{
    const vk::CommandBufferAllocateInfo allocate_info {
        .commandPool = context.command_pool.get(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1};

    auto command_buffer = std::move(
        context.device->allocateCommandBuffersUnique(allocate_info).front());

    constexpr vk::CommandBufferBeginInfo begin_info {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};

    command_buffer->begin(begin_info);

    return command_buffer;
}

void end_one_time_submit_command_buffer(
    const Vulkan_context &context,
    const vk::UniqueCommandBuffer &command_buffer)
{
    command_buffer->end();

    const vk::SubmitInfo submit_info {.commandBufferCount = 1,
                                      .pCommandBuffers = &command_buffer.get()};

    context.graphics_compute_queue.submit({submit_info}, {});

    // TODO: this is suboptimal
    context.graphics_compute_queue.waitIdle();
}

[[nodiscard]] Vulkan_buffer
create_buffer_from_host_data(const Vulkan_context &context,
                             vk::BufferUsageFlags usage,
                             VmaMemoryUsage memory_usage,
                             const void *data,
                             std::size_t size)
{
    // TODO: is this ever called with memory_usage other than
    // VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE ? If it isn't, we should probably
    // remove that parameter. If it is, then we can probably remove the staging
    // buffer when the buffer is created in CPU memory anyways.

    VmaAllocationInfo staging_allocation_info {};
    const auto staging_buffer =
        create_buffer(context.allocator.get(),
                      context.device.get(),
                      size,
                      vk::BufferUsageFlagBits::eTransferSrc,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                      &staging_allocation_info);

    auto *const mapped_data =
        static_cast<std::uint8_t *>(staging_allocation_info.pMappedData);

    std::memcpy(mapped_data, data, size);

    auto buffer = create_buffer(context.allocator.get(),
                                context.device.get(),
                                size,
                                usage,
                                {},
                                memory_usage,
                                nullptr);

    const auto command_buffer = begin_one_time_submit_command_buffer(context);

    const vk::BufferCopy region {.srcOffset = 0, .dstOffset = 0, .size = size};

    command_buffer->copyBuffer(
        staging_buffer.buffer.get(), buffer.buffer.get(), {region});

    end_one_time_submit_command_buffer(context, command_buffer);

    return buffer;
}

[[nodiscard]] Vulkan_buffer create_vertex_or_index_buffer(
    const Vulkan_context &context, const void *data, std::size_t size)
{
    return create_buffer_from_host_data(
        context,
        vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eShaderDeviceAddress |
            vk::BufferUsageFlagBits::
                eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        data,
        size);
}

[[nodiscard]] vk::DeviceAddress get_device_address(vk::Device device,
                                                   vk::Buffer buffer) noexcept
{
    const vk::BufferDeviceAddressInfo address_info {.buffer = buffer};
    return device.getBufferAddress(address_info);
}

[[nodiscard]] vk::DeviceAddress
get_device_address(vk::Device device,
                   vk::AccelerationStructureKHR acceleration_structure) noexcept
{
    const vk::AccelerationStructureDeviceAddressInfoKHR address_info {
        .accelerationStructure = acceleration_structure};
    return device.getAccelerationStructureAddressKHR(address_info);
}

void create_blas(const Vulkan_context &context,
                 Vulkan_render_resources &render_resources)
{
    const auto vertex_buffer_address = get_device_address(
        context.device.get(), render_resources.vertex_buffer.buffer.get());
    constexpr auto vertex_size = 3 * sizeof(float);
    const auto vertex_count = render_resources.vertex_buffer.size / vertex_size;

    const auto index_buffer_address = get_device_address(
        context.device.get(), render_resources.index_buffer.buffer.get());
    const auto index_count =
        render_resources.index_buffer.size / sizeof(std::uint32_t);
    const auto primitive_count = index_count / 3;

    const vk::AccelerationStructureGeometryTrianglesDataKHR triangles {
        .vertexFormat = vk::Format::eR32G32B32Sfloat,
        .vertexData = {.deviceAddress = vertex_buffer_address},
        .vertexStride = vertex_size,
        .maxVertex = static_cast<std::uint32_t>(vertex_count - 1),
        .indexType = vk::IndexType::eUint32,
        .indexData = {.deviceAddress = index_buffer_address},
        .transformData = {}};

    const vk::AccelerationStructureGeometryKHR geometry {
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry = {.triangles = triangles},
        .flags = vk::GeometryFlagBitsKHR::eOpaque};

    const vk::AccelerationStructureBuildRangeInfoKHR build_range_info {
        .primitiveCount = static_cast<std::uint32_t>(primitive_count),
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0};

    vk::AccelerationStructureBuildGeometryInfoKHR build_geometry_info {
        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        .flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .srcAccelerationStructure = {},
        .dstAccelerationStructure = {},
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = {},
        .scratchData = {}};

    const auto build_sizes_info =
        context.device->getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice,
            build_geometry_info,
            {build_range_info.primitiveCount});

    render_resources.blas_buffer = create_buffer(
        context.allocator.get(),
        context.device.get(),
        build_sizes_info.accelerationStructureSize,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
        {},
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        nullptr);

    const vk::AccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .createFlags = {},
            .buffer = render_resources.blas_buffer.buffer.get(),
            .offset = {},
            .size = build_sizes_info.accelerationStructureSize,
            .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
            .deviceAddress = {}};

    render_resources.blas =
        context.device->createAccelerationStructureKHRUnique(
            acceleration_structure_create_info);

    const auto scratch_buffer =
        create_buffer(context.allocator.get(),
                      context.device.get(),
                      build_sizes_info.buildScratchSize,
                      vk::BufferUsageFlagBits::eStorageBuffer |
                          vk::BufferUsageFlagBits::eShaderDeviceAddress,
                      {},
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      nullptr);

    const auto scratch_buffer_address =
        get_device_address(context.device.get(), scratch_buffer.buffer.get());

    build_geometry_info.dstAccelerationStructure = render_resources.blas.get();
    build_geometry_info.scratchData.deviceAddress = scratch_buffer_address;

    const auto command_buffer = begin_one_time_submit_command_buffer(context);

    const auto *const p_build_range_info = &build_range_info;

    command_buffer->buildAccelerationStructuresKHR(
        1, &build_geometry_info, &p_build_range_info);

    end_one_time_submit_command_buffer(context, command_buffer);
}

void create_tlas(const Vulkan_context &context,
                 Vulkan_render_resources &render_resources)
{
    const std::vector<vk::TransformMatrixKHR> transforms {
        {std::array {std::array {1.0f, 0.0f, 0.0f, -1.0f},
                     std::array {0.0f, 1.0f, 0.0f, 0.0f},
                     std::array {0.0f, 0.0f, 1.0f, 0.0f}}},
        {std::array {std::array {0.7f, 0.0f, 0.0f, 1.0f},
                     std::array {0.0f, 0.7f, 0.0f, 0.0f},
                     std::array {0.0f, 0.0f, 0.7f, 0.0f}}},
        {std::array {std::array {0.2f, 0.0f, 0.0f, 0.0f},
                     std::array {0.0f, 0.2f, 0.0f, 0.0f},
                     std::array {0.0f, 0.0f, 0.2f, 0.0f}}},
        {std::array {std::array {0.8f, 0.0f, 0.0f, 0.0f},
                     std::array {0.0f, 0.8f, 0.0f, 0.0f},
                     std::array {0.0f, 0.0f, 0.8f, 0.8f}}}};

    std::vector<vk::AccelerationStructureInstanceKHR> instances;
    instances.reserve(transforms.size());
    for (std::uint32_t i {0}; i < static_cast<std::uint32_t>(transforms.size());
         ++i)
    {
        instances.push_back(
            {.transform = transforms[i],
             .instanceCustomIndex = 0,
             .mask = 0xFF,
             .instanceShaderBindingTableRecordOffset = (i % 4) & 0xffffff,
             .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
             .accelerationStructureReference = get_device_address(
                 context.device.get(), render_resources.blas.get())});
    }

    const auto instance_buffer = create_buffer_from_host_data(
        context,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
            vk::BufferUsageFlagBits::eShaderDeviceAddress |
            vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        instances.data(),
        instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));

    const auto command_buffer = begin_one_time_submit_command_buffer(context);

    constexpr vk::MemoryBarrier barrier {
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR};

    command_buffer->pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
        {},
        {barrier},
        {},
        {});

    const vk::AccelerationStructureGeometryInstancesDataKHR
        geometry_instances_data {
            .arrayOfPointers = {},
            .data = {.deviceAddress = get_device_address(
                         context.device.get(), instance_buffer.buffer.get())}};

    const vk::AccelerationStructureGeometryKHR geometry {
        .geometryType = vk::GeometryTypeKHR::eInstances,
        .geometry = {.instances = geometry_instances_data},
        .flags = {}};

    vk::AccelerationStructureBuildGeometryInfoKHR build_geometry_info {
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        .flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .srcAccelerationStructure = {},
        .dstAccelerationStructure = {},
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = {},
        .scratchData = {}};

    const auto num_instances = static_cast<std::uint32_t>(instances.size());
    const auto build_sizes_info =
        context.device->getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice,
            build_geometry_info,
            {num_instances} // NOTE: this array should have size equal to
                            // build_geometry_info.pGeometryCount
        );

    render_resources.tlas_buffer = create_buffer(
        context.allocator.get(),
        context.device.get(),
        build_sizes_info.accelerationStructureSize,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
        {},
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        nullptr);

    const vk::AccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .createFlags = {},
            .buffer = render_resources.tlas_buffer.buffer.get(),
            .offset = {},
            .size = build_sizes_info.accelerationStructureSize,
            .type = vk::AccelerationStructureTypeKHR::eTopLevel,
            .deviceAddress = {}};

    render_resources.tlas =
        context.device->createAccelerationStructureKHRUnique(
            acceleration_structure_create_info);

    const auto scratch_buffer =
        create_buffer(context.allocator.get(),
                      context.device.get(),
                      build_sizes_info.buildScratchSize,
                      vk::BufferUsageFlagBits::eStorageBuffer |
                          vk::BufferUsageFlagBits::eShaderDeviceAddress,
                      {},
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      nullptr);

    const auto scratch_buffer_address =
        get_device_address(context.device.get(), scratch_buffer.buffer.get());

    build_geometry_info.dstAccelerationStructure = render_resources.tlas.get();
    build_geometry_info.scratchData.deviceAddress = scratch_buffer_address;

    const vk::AccelerationStructureBuildRangeInfoKHR build_range_info {
        .primitiveCount = num_instances,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0};

    const auto *const p_build_range_info = &build_range_info;

    command_buffer->buildAccelerationStructuresKHR({build_geometry_info},
                                                   {p_build_range_info});

    end_one_time_submit_command_buffer(context, command_buffer);
}

void create_descriptor_set_layout(const Vulkan_context &context,
                                  Vulkan_render_resources &render_resources)
{
    constexpr vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[] {
        {.binding = 0,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR},
        {.binding = 1,
         .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR},
        {.binding = 2,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR |
                       vk::ShaderStageFlagBits::eClosestHitKHR},
        {.binding = 3,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR |
                       vk::ShaderStageFlagBits::eClosestHitKHR},
        {.binding = 4,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR}};

    const vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info {
        .bindingCount = static_cast<std::uint32_t>(
            std::size(descriptor_set_layout_bindings)),
        .pBindings = descriptor_set_layout_bindings};

    render_resources.descriptor_set_layout =
        context.device->createDescriptorSetLayoutUnique(
            descriptor_set_layout_create_info);
}

void create_final_render_descriptor_set_layout(
    const Vulkan_context &context, Vulkan_render_resources &render_resources)
{
    constexpr vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[] {
        {.binding = 0,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment}};

    const vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info {
        .bindingCount = static_cast<std::uint32_t>(
            std::size(descriptor_set_layout_bindings)),
        .pBindings = descriptor_set_layout_bindings};

    render_resources.final_render_descriptor_set_layout =
        context.device->createDescriptorSetLayoutUnique(
            descriptor_set_layout_create_info);
}

void create_descriptor_set(const Vulkan_context &context,
                           Vulkan_render_resources &render_resources)
{
    const vk::DescriptorSetAllocateInfo descriptor_set_allocate_info {
        .descriptorPool = context.descriptor_pool.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = &render_resources.descriptor_set_layout.get()};

    render_resources.descriptor_set = std::move(
        context.device
            ->allocateDescriptorSetsUnique(descriptor_set_allocate_info)
            .front());

    const vk::DescriptorImageInfo descriptor_storage_image {
        .sampler = VK_NULL_HANDLE,
        .imageView = render_resources.storage_image_view.get(),
        .imageLayout = vk::ImageLayout::eGeneral};

    const vk::WriteDescriptorSetAccelerationStructureKHR
        descriptor_acceleration_structure {.accelerationStructureCount = 1,
                                           .pAccelerationStructures =
                                               &render_resources.tlas.get()};

    const vk::DescriptorBufferInfo descriptor_vertices {
        .buffer = render_resources.vertex_buffer.buffer.get(),
        .offset = 0,
        .range = render_resources.vertex_buffer.size};

    const vk::DescriptorBufferInfo descriptor_indices {
        .buffer = render_resources.index_buffer.buffer.get(),
        .offset = 0,
        .range = render_resources.index_buffer.size};

    const vk::DescriptorImageInfo descriptor_render_target {
        .sampler = VK_NULL_HANDLE,
        .imageView = render_resources.render_target_view.get(),
        .imageLayout = vk::ImageLayout::eGeneral};

    const vk::WriteDescriptorSet descriptor_writes[] {
        {.dstSet = render_resources.descriptor_set.get(),
         .dstBinding = 0,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &descriptor_storage_image},
        {.pNext = &descriptor_acceleration_structure,
         .dstSet = render_resources.descriptor_set.get(),
         .dstBinding = 1,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eAccelerationStructureKHR},
        {.dstSet = render_resources.descriptor_set.get(),
         .dstBinding = 2,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &descriptor_vertices},
        {.dstSet = render_resources.descriptor_set.get(),
         .dstBinding = 3,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &descriptor_indices},
        {.dstSet = render_resources.descriptor_set.get(),
         .dstBinding = 4,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &descriptor_render_target}};

    context.device->updateDescriptorSets({descriptor_writes}, {});
}

void create_final_render_descriptor_set(
    const Vulkan_context &context, Vulkan_render_resources &render_resources)
{
    const vk::DescriptorSetAllocateInfo descriptor_set_allocate_info {
        .descriptorPool = context.descriptor_pool.get(),
        .descriptorSetCount = 1,
        .pSetLayouts =
            &render_resources.final_render_descriptor_set_layout.get()};

    render_resources.final_render_descriptor_set = std::move(
        context.device
            ->allocateDescriptorSetsUnique(descriptor_set_allocate_info)
            .front());

    const vk::DescriptorImageInfo descriptor_render_target {
        .sampler = render_resources.render_target_sampler.get(),
        .imageView = render_resources.render_target_view.get(),
        .imageLayout = vk::ImageLayout::eGeneral};

    const vk::WriteDescriptorSet descriptor_write {
        .dstSet = render_resources.final_render_descriptor_set.get(),
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &descriptor_render_target};

    context.device->updateDescriptorSets({descriptor_write}, {});
}

void create_ray_tracing_pipeline_layout(
    const Vulkan_context &context, Vulkan_render_resources &render_resources)
{
    constexpr vk::PushConstantRange push_constant_range {
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR,
        .offset = 0,
        .size = sizeof(Push_constants)};

    const vk::PipelineLayoutCreateInfo pipeline_layout_create_info {
        .setLayoutCount = 1,
        .pSetLayouts = &render_resources.descriptor_set_layout.get(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range};

    render_resources.ray_tracing_pipeline_layout =
        context.device->createPipelineLayoutUnique(pipeline_layout_create_info);
}

[[nodiscard]] vk::UniqueShaderModule create_shader_module(vk::Device device,
                                                          const char *file_name)
{
    const auto shader_code = read_binary_file(file_name);

    const vk::ShaderModuleCreateInfo shader_module_create_info {
        .codeSize = shader_code.size() * sizeof(std::uint32_t),
        .pCode = shader_code.data()};

    return device.createShaderModuleUnique(shader_module_create_info);
}

void create_ray_tracing_pipeline(const Vulkan_context &context,
                                 Vulkan_render_resources &render_resources)
{
    const auto rgen_shader_module =
        create_shader_module(context.device.get(), "shader.rgen.spv");
    const auto rmiss_shader_module =
        create_shader_module(context.device.get(), "shader.rmiss.spv");
    const auto rchit_diffuse_shader_module =
        create_shader_module(context.device.get(), "diffuse.rchit.spv");
    const auto rchit_specular_shader_module =
        create_shader_module(context.device.get(), "specular.rchit.spv");
    const auto rchit_emissive_shader_module =
        create_shader_module(context.device.get(), "emissive.rchit.spv");
    const auto rchit_dielectric_shader_module =
        create_shader_module(context.device.get(), "dielectric.rchit.spv");

    const vk::PipelineShaderStageCreateInfo shader_stage_create_infos[] {
        {.stage = vk::ShaderStageFlagBits::eRaygenKHR,
         .module = rgen_shader_module.get(),
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eMissKHR,
         .module = rmiss_shader_module.get(),
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eClosestHitKHR,
         .module = rchit_diffuse_shader_module.get(),
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eClosestHitKHR,
         .module = rchit_specular_shader_module.get(),
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eClosestHitKHR,
         .module = rchit_emissive_shader_module.get(),
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eClosestHitKHR,
         .module = rchit_dielectric_shader_module.get(),
         .pName = "main"}};

    const vk::RayTracingShaderGroupCreateInfoKHR ray_tracing_shader_groups[] {
        {.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
         .generalShader = 0,
         .closestHitShader = VK_SHADER_UNUSED_KHR,
         .anyHitShader = VK_SHADER_UNUSED_KHR,
         .intersectionShader = VK_SHADER_UNUSED_KHR},
        {.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
         .generalShader = 1,
         .closestHitShader = VK_SHADER_UNUSED_KHR,
         .anyHitShader = VK_SHADER_UNUSED_KHR,
         .intersectionShader = VK_SHADER_UNUSED_KHR},
        {.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
         .generalShader = VK_SHADER_UNUSED_KHR,
         .closestHitShader = 2,
         .anyHitShader = VK_SHADER_UNUSED_KHR,
         .intersectionShader = VK_SHADER_UNUSED_KHR},
        {.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
         .generalShader = VK_SHADER_UNUSED_KHR,
         .closestHitShader = 3,
         .anyHitShader = VK_SHADER_UNUSED_KHR,
         .intersectionShader = VK_SHADER_UNUSED_KHR},
        {.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
         .generalShader = VK_SHADER_UNUSED_KHR,
         .closestHitShader = 4,
         .anyHitShader = VK_SHADER_UNUSED_KHR,
         .intersectionShader = VK_SHADER_UNUSED_KHR},
        {.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
         .generalShader = VK_SHADER_UNUSED_KHR,
         .closestHitShader = 5,
         .anyHitShader = VK_SHADER_UNUSED_KHR,
         .intersectionShader = VK_SHADER_UNUSED_KHR}};

    const vk::RayTracingPipelineCreateInfoKHR ray_tracing_pipeline_create_info {
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
        .layout = render_resources.ray_tracing_pipeline_layout.get(),
        .basePipelineHandle = {},
        .basePipelineIndex = {}};

    auto result = context.device->createRayTracingPipelineKHRUnique(
        {}, {}, ray_tracing_pipeline_create_info);
    vk::detail::resultCheck(result.result,
                            "vk::Device::createRayTracingPipelineKHRUnique");

    render_resources.ray_tracing_pipeline = std::move(result.value);
}

void create_shader_binding_table(const Vulkan_context &context,
                                 Vulkan_render_resources &render_resources)
{
    const auto handle_size =
        context.physical_device_ray_tracing_pipeline_properties
            .shaderGroupHandleSize;
    const auto handle_alignment =
        context.physical_device_ray_tracing_pipeline_properties
            .shaderGroupHandleAlignment;
    const auto base_alignment =
        context.physical_device_ray_tracing_pipeline_properties
            .shaderGroupBaseAlignment;
    const auto handle_size_aligned = align_up(handle_size, handle_alignment);

    const std::uint32_t miss_count {1};
    const std::uint32_t hit_count {4}; // FIXME: this should not be hardcoded
    const std::uint32_t handle_count {1 + miss_count + hit_count};

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

    const std::size_t data_size {handle_count * handle_size};
    const auto handles =
        context.device->getRayTracingShaderGroupHandlesKHR<std::uint8_t>(
            render_resources.ray_tracing_pipeline.get(),
            0,
            handle_count,
            data_size);

    const auto sbt_size = render_resources.sbt_raygen_region.size +
                          render_resources.sbt_miss_region.size +
                          render_resources.sbt_hit_region.size +
                          render_resources.sbt_callable_region.size;

    VmaAllocationInfo sbt_allocation_info {};
    render_resources.sbt_buffer =
        create_buffer(context.allocator.get(),
                      context.device.get(),
                      sbt_size,
                      vk::BufferUsageFlagBits::eTransferSrc |
                          vk::BufferUsageFlagBits::eShaderBindingTableKHR |
                          vk::BufferUsageFlagBits::eShaderDeviceAddress,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                      &sbt_allocation_info);
    auto *const sbt_buffer_mapped =
        static_cast<std::uint8_t *>(sbt_allocation_info.pMappedData);

    const auto sbt_address = get_device_address(
        context.device.get(), render_resources.sbt_buffer.buffer.get());

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

ImGui_backend::~ImGui_backend()
{
    if (m_initialized)
    {
        ImGui_ImplVulkan_Shutdown();
    }
}

Vulkan_context create_context(GLFWwindow *window)
{
    Vulkan_context context {};

    create_instance(context);

    create_device(context);

    context.graphics_compute_queue = context.device->getQueue(
        context.graphics_compute_queue_family_index, 0);
    context.present_queue =
        context.device->getQueue(context.present_queue_family_index, 0);

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

    constexpr vk::QueryPoolCreateInfo query_pool_info {
        .queryType = vk::QueryType::eTimestamp, .queryCount = 2};
    context.query_pool = context.device->createQueryPoolUnique(query_pool_info);

    init_imgui(context);

    return context;
}

Vulkan_render_resources create_render_resources(const Vulkan_context &context,
                                                std::uint32_t render_width,
                                                std::uint32_t render_height,
                                                const aiScene *scene)
{
    Vulkan_render_resources render_resources {};

    constexpr auto storage_image_format = vk::Format::eR32G32B32A32Sfloat;
    render_resources.storage_image =
        create_image(context.allocator.get(),
                     context.device.get(),
                     render_width,
                     render_height,
                     storage_image_format,
                     vk::ImageUsageFlagBits::eStorage |
                         vk::ImageUsageFlagBits::eTransferDst);
    render_resources.storage_image_view =
        create_image_view(context.device.get(),
                          render_resources.storage_image.image.get(),
                          storage_image_format);

    // FIXME: why isn't this sRGB ?
    constexpr auto render_target_format = vk::Format::eR8G8B8A8Unorm;
    render_resources.render_target = create_image(
        context.allocator.get(),
        context.device.get(),
        render_width,
        render_height,
        render_target_format,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc);
    render_resources.render_target_view =
        create_image_view(context.device.get(),
                          render_resources.render_target.image.get(),
                          render_target_format);

    {
        const auto command_buffer =
            begin_one_time_submit_command_buffer(context);

        constexpr vk::ImageSubresourceRange subresource_range {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1};

        const vk::ImageMemoryBarrier image_memory_barriers[] {
            {.srcAccessMask = vk::AccessFlagBits::eNone,
             .dstAccessMask = vk::AccessFlagBits::eShaderRead |
                              vk::AccessFlagBits::eShaderWrite,
             .oldLayout = vk::ImageLayout::eUndefined,
             .newLayout = vk::ImageLayout::eGeneral,
             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .image = render_resources.storage_image.image.get(),
             .subresourceRange = subresource_range},
            {.srcAccessMask = vk::AccessFlagBits::eNone,
             .dstAccessMask = vk::AccessFlagBits::eShaderRead |
                              vk::AccessFlagBits::eShaderWrite,
             .oldLayout = vk::ImageLayout::eUndefined,
             .newLayout = vk::ImageLayout::eGeneral,
             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .image = render_resources.render_target.image.get(),
             .subresourceRange = subresource_range}};

        command_buffer->pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eRayTracingShaderKHR,
            {},
            {},
            {},
            image_memory_barriers);

        end_one_time_submit_command_buffer(context, command_buffer);
    }

    create_sampler(context, render_resources);

    const auto *const mesh = scene->mMeshes[0];
    std::vector<std::uint32_t> indices(mesh->mNumFaces * 3);
    for (unsigned int i {0}; i < mesh->mNumFaces; ++i)
    {
        indices[i * 3 + 0] = mesh->mFaces[i].mIndices[0];
        indices[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
        indices[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
    }

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

void draw_frame(Vulkan_context &context,
                Vulkan_render_resources &render_resources,
                const Camera &camera)
{
    if (context.framebuffer_width == 0 || context.framebuffer_height == 0)
    {
        // FIXME: We want to
        // continue tracing when the window is minimized, just not draw to
        // the framebuffer. But this requires a better separation of the
        // tracing vs drawing code. And this separation would probably be
        // best anyways.
        return;
    }

    auto result = context.device->waitForFences(
        {context.in_flight_fences[context.current_frame_in_flight].get()},
        VK_TRUE,
        std::numeric_limits<std::uint64_t>::max());
    vk::detail::resultCheck(result, "vk::Device::waitForFences");

    const auto image_index = context.device->acquireNextImageKHR(
        context.swapchain.get(),
        std::numeric_limits<std::uint64_t>::max(),
        context.image_available_semaphores[context.current_frame_in_flight]
            .get(),
        {});
    if (image_index.result == vk::Result::eErrorOutOfDateKHR)
    {
        recreate_swapchain(context);
        return;
    }
    else if (image_index.result != vk::Result::eSuboptimalKHR)
    {
        vk::detail::resultCheck(result, "vk::Device::acquireNextImageKHR");
    }

    context.device->resetFences(
        {context.in_flight_fences[context.current_frame_in_flight].get()});

    const auto command_buffer =
        context.command_buffers[context.current_frame_in_flight].get();

    command_buffer.reset();

    constexpr vk::CommandBufferBeginInfo begin_info {};
    command_buffer.begin(begin_info);

    command_buffer.resetQueryPool(context.query_pool.get(), 0, 2);
    command_buffer.writeTimestamp(
        vk::PipelineStageFlagBits::eTopOfPipe, context.query_pool.get(), 0);

    constexpr vk::ImageSubresourceRange subresource_range {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    // If a scene is loaded
    if (render_resources.storage_image.image)
    {
        if (render_resources.sample_count == 0)
        {
            // FIXME: this is actually not necessary on each reset, we
            // should only do it once at creation

            constexpr vk::ClearColorValue clear_value {
                .float32 = std::array {0.0f, 0.0f, 0.0f, 1.0f}};

            vk::ImageMemoryBarrier image_memory_barrier {
                .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = render_resources.storage_image.image.get(),
                .subresourceRange = subresource_range};

            command_buffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                vk::PipelineStageFlagBits::eTransfer,
                {},
                {},
                {},
                {image_memory_barrier});

            command_buffer.clearColorImage(
                render_resources.storage_image.image.get(),
                vk::ImageLayout::eGeneral,
                clear_value,
                {subresource_range});

            image_memory_barrier.srcAccessMask =
                vk::AccessFlagBits::eTransferWrite;
            image_memory_barrier.dstAccessMask =
                vk::AccessFlagBits::eShaderRead;

            command_buffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                {},
                {},
                {},
                {image_memory_barrier});
        }

        if (render_resources.sample_count < render_resources.samples_to_render)
        {
            command_buffer.bindPipeline(
                vk::PipelineBindPoint::eRayTracingKHR,
                render_resources.ray_tracing_pipeline.get());

            command_buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eRayTracingKHR,
                render_resources.ray_tracing_pipeline_layout.get(),
                0,
                {render_resources.descriptor_set.get()},
                {});

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
                .camera_dir_z = camera.direction_z,
                .sensor_distance = camera.sensor_distance,
                .sensor_half_width = camera.sensor_half_width,
                .sensor_half_height = camera.sensor_half_height,
                .focus_distance = camera.focus_distance,
                .aperture_radius = camera.aperture_radius};

            command_buffer.pushConstants(
                render_resources.ray_tracing_pipeline_layout.get(),
                vk::ShaderStageFlagBits::eRaygenKHR,
                0,
                sizeof(push_constants),
                &push_constants);
            render_resources.sample_count += samples_this_frame;

            command_buffer.traceRaysKHR(render_resources.sbt_raygen_region,
                                        render_resources.sbt_miss_region,
                                        render_resources.sbt_hit_region,
                                        render_resources.sbt_callable_region,
                                        render_resources.storage_image.width,
                                        render_resources.storage_image.height,
                                        1);

            const vk::ImageMemoryBarrier image_memory_barrier {
                .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = render_resources.render_target.image.get(),
                .subresourceRange = subresource_range};

            command_buffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                vk::PipelineStageFlagBits::eFragmentShader,
                {},
                {},
                {},
                {image_memory_barrier});
        }
    }

    command_buffer.writeTimestamp(
        vk::PipelineStageFlagBits::eBottomOfPipe, context.query_pool.get(), 1);

    constexpr vk::ClearValue clear_value {
        .color = {.float32 = std::array {0.0f, 0.0f, 0.0f, 1.0f}}};

    const vk::RenderPassBeginInfo render_pass_begin_info {
        .renderPass = context.render_pass.get(),
        .framebuffer = context.framebuffers[image_index.value].get(),
        .renderArea = {.offset = {0, 0}, .extent = context.swapchain_extent},
        .clearValueCount = 1,
        .pClearValues = &clear_value};

    command_buffer.beginRenderPass(render_pass_begin_info,
                                   vk::SubpassContents::eInline);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);

    command_buffer.endRenderPass();

    command_buffer.end();

    constexpr vk::PipelineStageFlags wait_stage {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};

    const vk::SubmitInfo submit_info {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores =
            &context.image_available_semaphores[context.current_frame_in_flight]
                 .get(),
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores =
            &context.render_finished_semaphores[context.current_frame_in_flight]
                 .get()};

    context.graphics_compute_queue.submit(
        {submit_info},
        context.in_flight_fences[context.current_frame_in_flight].get());

    // FIXME: we shouldn't have the "eWait" flag here, see the bottom of
    // https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html
    /*std::uint64_t timestamps[2] {};
    vk::detail::resultCheck(
        context.device->getQueryPoolResults(context.query_pool.get(),
                                            0,
                                            2,
                                            sizeof(timestamps),
                                            timestamps,
                                            sizeof(std::uint64_t),
                                            vk::QueryResultFlagBits::e64 |
                                                vk::QueryResultFlagBits::eWait),
        "vk::Device::getQueryPoolResults");
    const auto period =
        context.physical_device_properties.limits.timestampPeriod;
    const float elapsed_ms =
        static_cast<float>(timestamps[1] - timestamps[0]) * period / 1e6f;
    std::cout << elapsed_ms << " ms\n";*/

    const vk::PresentInfoKHR present_info {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores =
            &context.render_finished_semaphores[context.current_frame_in_flight]
                 .get(),
        .swapchainCount = 1,
        .pSwapchains = &context.swapchain.get(),
        .pImageIndices = &image_index.value,
        .pResults = {}};

    // NOTE: we use the noexcept version of presentKHR that takes the
    // vk::PresentInfoKHR by pointer, because we don't want the call to
    // throw an exception in case of vk::Result::ErrorOutOfDateKHR
    result = context.present_queue.presentKHR(&present_info);

    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR || context.framebuffer_resized)
    {
        context.framebuffer_resized = false;
        // FIXME: we should really understand the difference between this
        // and the former call to recreate_swapchain at the beginning of
        // this function
        recreate_swapchain(context);
    }
    else
    {
        vk::detail::resultCheck(result, "vk::Queue::presentKHR");
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
        create_buffer(context.allocator.get(),
                      context.device.get(),
                      render_resources.render_target.width *
                          render_resources.render_target.height * 4,
                      vk::BufferUsageFlagBits::eTransferDst,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                      &staging_allocation_info);

    const auto *const mapped_data =
        static_cast<std::uint8_t *>(staging_allocation_info.pMappedData);

    const auto command_buffer = begin_one_time_submit_command_buffer(context);

    constexpr vk::ImageSubresourceRange subresource_range {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    // FIXME: these are probably wrong since we changed the way we write to
    // the final image
    vk::ImageMemoryBarrier image_memory_barrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderRead,
        .dstAccessMask = vk::AccessFlagBits::eTransferRead,
        .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .newLayout = vk::ImageLayout::eTransferSrcOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = render_resources.render_target.image.get(),
        .subresourceRange = subresource_range};

    command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                                    vk::PipelineStageFlagBits::eTransfer,
                                    {},
                                    {},
                                    {},
                                    {image_memory_barrier});

    constexpr vk::ImageSubresourceLayers subresource_layers {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1};

    const vk::BufferImageCopy copy_region {
        .bufferOffset = 0,
        .bufferRowLength = {},
        .bufferImageHeight = render_resources.render_target.height,
        .imageSubresource = subresource_layers,
        .imageOffset = {0, 0, 0},
        .imageExtent = {render_resources.render_target.width,
                        render_resources.render_target.height,
                        1}};

    command_buffer->copyImageToBuffer(
        render_resources.render_target.image.get(),
        vk::ImageLayout::eTransferSrcOptimal,
        staging_buffer.buffer.get(),
        {copy_region});

    std::swap(image_memory_barrier.srcAccessMask,
              image_memory_barrier.dstAccessMask);
    std::swap(image_memory_barrier.oldLayout, image_memory_barrier.newLayout);

    command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                    vk::PipelineStageFlagBits::eFragmentShader,
                                    {},
                                    {},
                                    {},
                                    {image_memory_barrier});

    end_one_time_submit_command_buffer(context, command_buffer);

    return write_png(file_name,
                     mapped_data,
                     static_cast<int>(render_resources.render_target.width),
                     static_cast<int>(render_resources.render_target.height));
}
