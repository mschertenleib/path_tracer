#include "renderer.hpp"

#include "utils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace
{

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

constexpr vk::DebugUtilsMessengerCreateInfoEXT
    g_debug_utils_messenger_create_info {
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = &debug_callback};

[[nodiscard]] bool khronos_validation_layer_supported()
{
    const auto available_layers = vk::enumerateInstanceLayerProperties();

    return std::any_of(available_layers.begin(),
                       available_layers.end(),
                       [](const vk::LayerProperties &layer_properties)
                       {
                           return std::strcmp(layer_properties.layerName,
                                              "VK_LAYER_KHRONOS_validation") ==
                                  0;
                       });
}

#endif

[[nodiscard]] std::string to_string(VkResult result)
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
    default: return std::to_string(result);
    }
}

void check_vk_result(
    VkResult result,
    const std::source_location &loc = std::source_location::current())
{
    if (result != VK_SUCCESS)
    {
        fatal_error("Result is " + to_string(result), loc);
    }
}

constexpr auto g_compute_shader = "render.comp.spv";
constexpr auto g_vertex_shader = "fullscreen.vert.spv";
constexpr auto g_fragment_shader = "fullscreen.frag.spv";

[[nodiscard]] bool instance_extensions_supported(
    const std::vector<const char *> &required_extensions)
{
    const auto available_extensions =
        vk::enumerateInstanceExtensionProperties();

    const auto is_extension_supported = [&](const char *extension)
    {
        return std::any_of(
            available_extensions.begin(),
            available_extensions.end(),
            [&](const vk::ExtensionProperties &extension_properties) {
                return std::strcmp(extension_properties.extensionName,
                                   extension) == 0;
            });
    };

    return std::all_of(required_extensions.begin(),
                       required_extensions.end(),
                       is_extension_supported);
}

[[nodiscard]] bool
swapchain_extension_supported(const vk::PhysicalDevice &physical_device)
{
    const auto available_extensions =
        physical_device.enumerateDeviceExtensionProperties();

    return std::any_of(available_extensions.begin(),
                       available_extensions.end(),
                       [](const vk::ExtensionProperties &extension_properties)
                       {
                           return std::strcmp(
                                      extension_properties.extensionName,
                                      VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
                       });
}

[[nodiscard]] std::optional<Queue_family_indices>
get_queue_family_indices(const vk::PhysicalDevice &physical_device,
                         const vk::UniqueSurfaceKHR &surface)
{
    const auto queue_family_properties =
        physical_device.getQueueFamilyProperties();

    std::optional<std::uint32_t> graphics_queue_family_index;
    std::optional<std::uint32_t> present_queue_family_index;

    for (std::uint32_t i {}; const auto &queue_family : queue_family_properties)
    {
        if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics)
        {
            graphics_queue_family_index = i;
        }

        if (physical_device.getSurfaceSupportKHR(i, *surface))
        {
            present_queue_family_index = i;
        }

        if (graphics_queue_family_index.has_value() &&
            present_queue_family_index.has_value())
        {
            return Queue_family_indices {graphics_queue_family_index.value(),
                                         present_queue_family_index.value()};
        }

        ++i;
    }

    return std::nullopt;
}

[[nodiscard]] bool
is_physical_device_suitable(const vk::PhysicalDevice &physical_device,
                            const vk::UniqueSurfaceKHR &surface)
{
    if (!swapchain_extension_supported(physical_device))
    {
        return false;
    }

    if (physical_device.getSurfaceFormatsKHR(*surface).empty())
    {
        return false;
    }

    if (physical_device.getSurfacePresentModesKHR(*surface).empty())
    {
        return false;
    }

    if (!get_queue_family_indices(physical_device, surface).has_value())
    {
        return false;
    }

    return true;
}

[[nodiscard]] vk::UniqueInstance create_instance()
{
    const auto vkGetInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(glfwGetInstanceProcAddress(
            VK_NULL_HANDLE, "vkGetInstanceProcAddr"));
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    constexpr vk::ApplicationInfo application_info {.pApplicationName = "",
                                                    .applicationVersion = {},
                                                    .pEngineName = "",
                                                    .engineVersion = {},
                                                    .apiVersion =
                                                        VK_API_VERSION_1_3};

    std::uint32_t extension_count {};
    const auto extensions = glfwGetRequiredInstanceExtensions(&extension_count);
    std::vector<const char *> required_extensions(extensions,
                                                  extensions + extension_count);

#ifdef ENABLE_VALIDATION_LAYERS

    if (!khronos_validation_layer_supported())
    {
        fatal_error("Validation layers are not supported");
    }

    required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (!instance_extensions_supported(required_extensions))
    {
        fatal_error("Unsupported instance extension");
    }

    constexpr auto khronos_validation_layer = "VK_LAYER_KHRONOS_validation";
    const vk::InstanceCreateInfo instance_create_info {
        .pNext = &g_debug_utils_messenger_create_info,
        .pApplicationInfo = &application_info,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &khronos_validation_layer,
        .enabledExtensionCount =
            static_cast<std::uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data()};

    auto instance = vk::createInstanceUnique(instance_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    return instance;

#else

    if (!instance_extensions_supported(required_extensions))
    {
        fatal_error("Unsupported instance extension");
    }

    const vk::InstanceCreateInfo instance_create_info {
        .pApplicationInfo = &application_info,
        .enabledExtensionCount =
            static_cast<std::uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data()};

    auto instance = vk::createInstanceUnique(instance_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    return instance;

#endif
}

#ifdef ENABLE_VALIDATION_LAYERS
[[nodiscard]] vk::UniqueDebugUtilsMessengerEXT
create_debug_utils_messenger(const vk::UniqueInstance &instance)
{
    return instance->createDebugUtilsMessengerEXTUnique(
        g_debug_utils_messenger_create_info);
}
#endif

[[nodiscard]] vk::UniqueSurfaceKHR
create_surface(const vk::UniqueInstance &instance, GLFWwindow *window)
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &surface) !=
        VK_SUCCESS)
    {
        fatal_error("Failed to create surface");
    }
    return vk::UniqueSurfaceKHR(surface, *instance);
}

[[nodiscard]] vk::PhysicalDevice
select_physical_device(const vk::UniqueInstance &instance,
                       const vk::UniqueSurfaceKHR &surface)
{
    const auto physical_devices = instance->enumeratePhysicalDevices();
    if (physical_devices.empty())
    {
        fatal_error("Failed to find a device with Vulkan support");
    }

    std::vector<vk::PhysicalDevice> suitable_devices;
    for (const auto &physical_device : physical_devices)
    {
        if (is_physical_device_suitable(physical_device, surface))
        {
            suitable_devices.push_back(physical_device);
        }
    }

    if (suitable_devices.empty())
    {
        fatal_error("Failed to find a suitable device");
    }

    const auto it =
        std::find_if(suitable_devices.begin(),
                     suitable_devices.end(),
                     [](const auto &physical_device)
                     {
                         return physical_device.getProperties().deviceType ==
                                vk::PhysicalDeviceType::eDiscreteGpu;
                     });
    if (it != suitable_devices.end())
    {
        // Return the first suitable discrete GPU, if there is one
        return *it;
    }

    // Else return the first suitable device
    return suitable_devices.front();
}

[[nodiscard]] vk::UniqueDevice
create_device(const vk::PhysicalDevice &physical_device,
              const Queue_family_indices &queue_family_indices)
{
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;

    constexpr float queue_priority {1.0f};

    const vk::DeviceQueueCreateInfo graphics_queue_create_info {
        .queueFamilyIndex = queue_family_indices.graphics,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority};
    queue_create_infos.push_back(graphics_queue_create_info);

    if (queue_family_indices.graphics != queue_family_indices.present)
    {
        const vk::DeviceQueueCreateInfo present_queue_create_info {
            .queueFamilyIndex = queue_family_indices.present,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority};
        queue_create_infos.push_back(present_queue_create_info);
    }

    constexpr auto swapchain_extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    // FIXME: check that the device actually supports the extension
    const vk::DeviceCreateInfo device_create_info {
        .queueCreateInfoCount =
            static_cast<std::uint32_t>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &swapchain_extension};

    auto device = physical_device.createDeviceUnique(device_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    return device;
}

[[nodiscard]] Swapchain
create_swapchain(const vk::UniqueDevice &device,
                 const vk::PhysicalDevice &physical_device,
                 const vk::UniqueSurfaceKHR &surface,
                 const Queue_family_indices &queue_family_indices,
                 std::uint32_t width,
                 std::uint32_t height)
{
    const auto &surface_formats =
        physical_device.getSurfaceFormatsKHR(*surface);
    const auto surface_format_it = std::find_if(
        surface_formats.begin(),
        surface_formats.end(),
        [](const vk::SurfaceFormatKHR &format)
        {
            return format.format == vk::Format::eB8G8R8A8Unorm &&
                   format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
    const auto surface_format = (surface_format_it != surface_formats.end())
                                    ? *surface_format_it
                                    : surface_formats.front();

    const auto &surface_capabilities =
        physical_device.getSurfaceCapabilitiesKHR(*surface);
    vk::Extent2D extent;
    if (surface_capabilities.currentExtent.width !=
        std::numeric_limits<std::uint32_t>::max())
    {
        extent = surface_capabilities.currentExtent;
    }
    else
    {
        extent = vk::Extent2D {width, height};
    }
    extent.width = std::clamp(extent.width,
                              surface_capabilities.minImageExtent.width,
                              surface_capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
                               surface_capabilities.minImageExtent.height,
                               surface_capabilities.maxImageExtent.height);

    std::uint32_t min_image_count {3};
    if (min_image_count < surface_capabilities.minImageCount)
    {
        min_image_count = surface_capabilities.minImageCount;
    }
    if (surface_capabilities.maxImageCount > 0 &&
        min_image_count > surface_capabilities.maxImageCount)
    {
        min_image_count = surface_capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR swapchain_create_info {
        .surface = *surface,
        .minImageCount = min_image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eFifo,
        .clipped = VK_TRUE};

    const std::uint32_t queue_family_indices_array[] {
        queue_family_indices.graphics, queue_family_indices.present};
    if (queue_family_indices.graphics != queue_family_indices.present)
    {
        swapchain_create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices_array;
    }
    else
    {
        swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
        swapchain_create_info.queueFamilyIndexCount = 1;
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices_array;
    }

    return Swapchain {
        .swapchain = device->createSwapchainKHRUnique(swapchain_create_info),
        .format = surface_format.format,
        .extent = extent,
        .min_image_count = min_image_count};
}

[[nodiscard]] vk::UniqueImageView create_image_view(
    const vk::UniqueDevice &device, vk::Image image, vk::Format format)
{
    const vk::ImageViewCreateInfo create_info {
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    return device->createImageViewUnique(create_info);
}

[[nodiscard]] std::vector<vk::UniqueImageView>
create_swapchain_image_views(const vk::UniqueDevice &device,
                             const std::vector<vk::Image> &swapchain_images,
                             vk::Format swapchain_format)
{
    std::vector<vk::UniqueImageView> swapchain_image_views;
    swapchain_image_views.reserve(swapchain_images.size());

    for (const auto &swapchain_image : swapchain_images)
    {
        swapchain_image_views.push_back(
            create_image_view(device, swapchain_image, swapchain_format));
    }

    return swapchain_image_views;
}

[[nodiscard]] vk::UniqueCommandPool
create_command_pool(const vk::UniqueDevice &device,
                    std::uint32_t graphics_family_index)
{
    const vk::CommandPoolCreateInfo create_info {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = graphics_family_index};

    return device->createCommandPoolUnique(create_info);
}

[[nodiscard]] vk::UniqueCommandBuffer
begin_one_time_submit_command_buffer(const vk::UniqueDevice &device,
                                     const vk::UniqueCommandPool &command_pool)
{
    const vk::CommandBufferAllocateInfo allocate_info {
        .commandPool = *command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1};

    auto command_buffers = device->allocateCommandBuffersUnique(allocate_info);
    auto command_buffer = std::move(command_buffers.front());

    constexpr vk::CommandBufferBeginInfo begin_info {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};

    command_buffer->begin(begin_info);

    return command_buffer;
}

void end_one_time_submit_command_buffer(
    const vk::UniqueCommandBuffer &command_buffer,
    const vk::Queue &graphics_queue)
{
    command_buffer->end();

    const vk::SubmitInfo submit_info {.commandBufferCount = 1,
                                      .pCommandBuffers = &*command_buffer};

    graphics_queue.submit(submit_info);
    graphics_queue.waitIdle();
}

[[nodiscard]] std::uint32_t
find_memory_type(const vk::PhysicalDevice &physical_device,
                 std::uint32_t type_filter,
                 vk::MemoryPropertyFlags requested_properties)
{
    const auto memory_properties = physical_device.getMemoryProperties();

    for (std::uint32_t i {}; i < memory_properties.memoryTypeCount; ++i)
    {
        if ((type_filter & (1 << i)) &&
            (memory_properties.memoryTypes[i].propertyFlags &
             requested_properties) == requested_properties)
        {
            return i;
        }
    }

    fatal_error("Failed to find a suitable memory type");
}

[[nodiscard]] Buffer create_buffer(const vk::UniqueDevice &device,
                                   const vk::PhysicalDevice &physical_device,
                                   vk::DeviceSize size,
                                   vk::BufferUsageFlags usage,
                                   vk::MemoryPropertyFlags properties)
{
    const vk::BufferCreateInfo buffer_create_info {
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive};

    auto buffer = device->createBufferUnique(buffer_create_info);

    const auto memory_requirements =
        device->getBufferMemoryRequirements(*buffer);

    const vk::MemoryAllocateInfo allocate_info {
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = find_memory_type(
            physical_device, memory_requirements.memoryTypeBits, properties)};

    auto memory = device->allocateMemoryUnique(allocate_info);

    device->bindBufferMemory(*buffer, *memory, 0);

    return {std::move(buffer), std::move(memory)};
}

[[nodiscard]] Image create_image(const vk::UniqueDevice &device,
                                 const vk::PhysicalDevice &physical_device,
                                 std::uint32_t width,
                                 std::uint32_t height,
                                 vk::Format format,
                                 vk::ImageUsageFlags usage,
                                 vk::MemoryPropertyFlags properties)
{
    const vk::ImageCreateInfo image_create_info {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {.width = width, .height = height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined};

    auto image = device->createImageUnique(image_create_info);

    const auto memory_requirements = device->getImageMemoryRequirements(*image);

    const vk::MemoryAllocateInfo allocate_info {
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = find_memory_type(
            physical_device, memory_requirements.memoryTypeBits, properties)};

    auto memory = device->allocateMemoryUnique(allocate_info);

    device->bindImageMemory(*image, *memory, 0);

    auto view = create_image_view(device, *image, format);

    return {std::move(image), std::move(view), std::move(memory)};
}

void copy_buffer_to_image(const vk::UniqueCommandBuffer &command_buffer,
                          const vk::UniqueBuffer &buffer,
                          const vk::UniqueImage &image,
                          std::uint32_t width,
                          std::uint32_t height)
{
    const vk::BufferImageCopy region {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .mipLevel = 0,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}};

    command_buffer->copyBufferToImage(
        *buffer, *image, vk::ImageLayout::eTransferDstOptimal, region);
}

void transition_image_layout(const vk::UniqueCommandBuffer &command_buffer,
                             const vk::UniqueImage &image,
                             vk::ImageLayout old_layout,
                             vk::ImageLayout new_layout,
                             vk::PipelineStageFlags src_stage_mask,
                             vk::PipelineStageFlags dst_stage_mask,
                             vk::AccessFlags src_access_mask,
                             vk::AccessFlags dst_access_mask)
{
    const vk::ImageMemoryBarrier memory_barrier {
        .srcAccessMask = src_access_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *image,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    command_buffer->pipelineBarrier(
        src_stage_mask, dst_stage_mask, {}, {}, {}, memory_barrier);
}

[[nodiscard]] vk::UniqueRenderPass
create_render_pass(const vk::UniqueDevice &device,
                   vk::Format color_attachment_format)
{
    const vk::AttachmentDescription color_attachment_description {
        .format = color_attachment_format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::ePresentSrcKHR};

    constexpr vk::AttachmentReference color_attachment_reference {
        .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};

    const vk::SubpassDescription subpass_description {
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_reference};

    constexpr vk::SubpassDependency subpass_dependency {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = {},
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite};

    const vk::RenderPassCreateInfo create_info {
        .attachmentCount = 1,
        .pAttachments = &color_attachment_description,
        .subpassCount = 1,
        .pSubpasses = &subpass_description,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency};

    return device->createRenderPassUnique(create_info);
}

[[nodiscard]] vk::UniqueRenderPass
create_offscreen_render_pass(const vk::UniqueDevice &device,
                             vk::Format color_attachment_format)
{
    const vk::AttachmentDescription color_attachment_description {
        .format = color_attachment_format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

    constexpr vk::AttachmentReference color_attachment_reference {
        .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};

    const vk::SubpassDescription subpass_description {
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_reference};

    // Use subpass dependencies for layout transitions
    constexpr vk::SubpassDependency subpass_dependencies[] {
        {.srcSubpass = VK_SUBPASS_EXTERNAL,
         .dstSubpass = 0,
         .srcStageMask = vk::PipelineStageFlagBits::eFragmentShader,
         .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
         .srcAccessMask = vk::AccessFlagBits::eShaderRead,
         .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
         .dependencyFlags = vk::DependencyFlagBits::eByRegion},
        {.srcSubpass = 0,
         .dstSubpass = VK_SUBPASS_EXTERNAL,
         .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
         .dstStageMask = vk::PipelineStageFlagBits::eFragmentShader,
         .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
         .dstAccessMask = vk::AccessFlagBits::eShaderRead,
         .dependencyFlags = vk::DependencyFlagBits::eByRegion}};

    const vk::RenderPassCreateInfo create_info {
        .attachmentCount = 1,
        .pAttachments = &color_attachment_description,
        .subpassCount = 1,
        .pSubpasses = &subpass_description,
        .dependencyCount = std::size(subpass_dependencies),
        .pDependencies = subpass_dependencies};

    return device->createRenderPassUnique(create_info);
}

[[nodiscard]] vk::UniqueDescriptorSetLayout
create_offscreen_descriptor_set_layout(const vk::UniqueDevice &device)
{
    constexpr vk::DescriptorSetLayoutBinding sampler_layout_binding {
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment};

    const vk::DescriptorSetLayoutCreateInfo create_info {
        .bindingCount = 1, .pBindings = &sampler_layout_binding};

    return device->createDescriptorSetLayoutUnique(create_info);
}

[[nodiscard]] vk::UniqueDescriptorSetLayout
create_descriptor_set_layout(const vk::UniqueDevice &device)
{
    constexpr vk::DescriptorSetLayoutBinding sampler_layout_binding {
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment};

    const vk::DescriptorSetLayoutCreateInfo create_info {
        .bindingCount = 1, .pBindings = &sampler_layout_binding};

    return device->createDescriptorSetLayoutUnique(create_info);
}

[[nodiscard]] vk::UniquePipelineLayout create_offscreen_pipeline_layout(
    const vk::UniqueDevice &device,
    const vk::UniqueDescriptorSetLayout &descriptor_set_layout)
{
    const vk::PushConstantRange push_constant_range {
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .offset = 0,
        .size = sizeof(Push_constants),
    };

    const vk::PipelineLayoutCreateInfo create_info {
        .setLayoutCount = 1,
        .pSetLayouts = &*descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range};

    return device->createPipelineLayoutUnique(create_info);
}

[[nodiscard]] vk::UniquePipelineLayout create_pipeline_layout(
    const vk::UniqueDevice &device,
    const vk::UniqueDescriptorSetLayout &descriptor_set_layout)
{
    const vk::PipelineLayoutCreateInfo create_info {
        .setLayoutCount = 1, .pSetLayouts = &*descriptor_set_layout};

    return device->createPipelineLayoutUnique(create_info);
}

[[nodiscard]] vk::UniqueShaderModule
create_shader_module(const vk::UniqueDevice &device, const char *filename)
{
    const auto shader_code = read_binary_file(filename);
    if (shader_code.empty())
    {
        fatal_error(std::string("Failed to load shader \"") +
                    std::string(filename) + std::string("\""));
    }

    const vk::ShaderModuleCreateInfo create_info {
        .codeSize = shader_code.size() * 4, .pCode = shader_code.data()};

    return device->createShaderModuleUnique(create_info);
}

[[nodiscard]] vk::UniquePipeline
create_pipeline(const vk::UniqueDevice &device,
                const vk::UniquePipelineLayout &pipeline_layout,
                const vk::UniqueRenderPass &render_pass,
                vk::Offset2D viewport_offset,
                vk::Extent2D viewport_extent,
                vk::Extent2D framebuffer_extent)
{
    const auto vertex_shader_module =
        create_shader_module(device, g_vertex_shader);
    const auto fragment_shader_module =
        create_shader_module(device, g_fragment_shader);

    const vk::PipelineShaderStageCreateInfo shader_stage_create_infos[] {
        {.stage = vk::ShaderStageFlagBits::eVertex,
         .module = *vertex_shader_module,
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eFragment,
         .module = *fragment_shader_module,
         .pName = "main"}};

    const vk::PipelineVertexInputStateCreateInfo
        vertex_input_state_create_info {};

    const vk::PipelineInputAssemblyStateCreateInfo
        input_assembly_state_create_info {
            .topology = vk::PrimitiveTopology::eTriangleStrip,
            .primitiveRestartEnable = VK_FALSE};

    const vk::Viewport viewport {
        .x = static_cast<float>(viewport_offset.x),
        .y = static_cast<float>(viewport_offset.y),
        .width = static_cast<float>(viewport_extent.width),
        .height = static_cast<float>(viewport_extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};

    const vk::Rect2D scissor {.offset = {0, 0}, .extent = framebuffer_extent};

    const vk::PipelineViewportStateCreateInfo viewport_state_create_info {
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor};

    constexpr vk::PipelineRasterizationStateCreateInfo
        rasterization_state_create_info {
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f};

    constexpr vk::PipelineMultisampleStateCreateInfo
        multisample_state_create_info {.rasterizationSamples =
                                           vk::SampleCountFlagBits::e1,
                                       .sampleShadingEnable = VK_FALSE};

    constexpr vk::PipelineColorBlendAttachmentState
        color_blend_attachment_state {.blendEnable = VK_FALSE,
                                      .colorWriteMask =
                                          vk::ColorComponentFlagBits::eR |
                                          vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB |
                                          vk::ColorComponentFlagBits::eA};

    const vk::PipelineColorBlendStateCreateInfo color_blend_state_create_info {
        .logicOpEnable = VK_FALSE,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_state,
        .blendConstants = {{0.0f, 0.0f, 0.0f, 0.0f}}};

    const vk::GraphicsPipelineCreateInfo pipeline_create_info {
        .stageCount = 2,
        .pStages = shader_stage_create_infos,
        .pVertexInputState = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_state_create_info,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState = &multisample_state_create_info,
        .pColorBlendState = &color_blend_state_create_info,
        .layout = *pipeline_layout,
        .renderPass = *render_pass,
        .subpass = 0};

    // FIXME
    return device
        ->createGraphicsPipelineUnique(VK_NULL_HANDLE, pipeline_create_info)
        .value;
}

[[nodiscard]] vk::UniquePipeline
create_offscreen_pipeline(const vk::UniqueDevice &device,
                          const vk::UniquePipelineLayout &pipeline_layout,
                          const vk::UniqueRenderPass &render_pass,
                          const char *vertex_shader_path,
                          const char *fragment_shader_path,
                          vk::PrimitiveTopology primitive_topology,
                          vk::Extent2D extent)
{
    const auto vertex_shader_module =
        create_shader_module(device, vertex_shader_path);
    const auto fragment_shader_module =
        create_shader_module(device, fragment_shader_path);

    const vk::PipelineShaderStageCreateInfo shader_stage_create_infos[] {
        {.stage = vk::ShaderStageFlagBits::eVertex,
         .module = *vertex_shader_module,
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eFragment,
         .module = *fragment_shader_module,
         .pName = "main"}};

    const vk::PipelineVertexInputStateCreateInfo
        vertex_input_state_create_info {};

    const vk::PipelineInputAssemblyStateCreateInfo
        input_assembly_state_create_info {.topology = primitive_topology,
                                          .primitiveRestartEnable = VK_FALSE};

    const vk::Viewport viewport {.x = 0.0f,
                                 .y = 0.0f,
                                 .width = static_cast<float>(extent.width),
                                 .height = static_cast<float>(extent.height),
                                 .minDepth = 0.0f,
                                 .maxDepth = 1.0f};

    const vk::Rect2D scissor {.offset = {0, 0}, .extent = extent};

    const vk::PipelineViewportStateCreateInfo viewport_state_create_info {
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor};

    constexpr vk::PipelineRasterizationStateCreateInfo
        rasterization_state_create_info {
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f};

    constexpr vk::PipelineMultisampleStateCreateInfo
        multisample_state_create_info {.rasterizationSamples =
                                           vk::SampleCountFlagBits::e1,
                                       .sampleShadingEnable = VK_FALSE};

    constexpr vk::PipelineColorBlendAttachmentState
        color_blend_attachment_state {.blendEnable = VK_FALSE,
                                      .colorWriteMask =
                                          vk::ColorComponentFlagBits::eR |
                                          vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB |
                                          vk::ColorComponentFlagBits::eA};

    const vk::PipelineColorBlendStateCreateInfo color_blend_state_create_info {
        .logicOpEnable = VK_FALSE,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_state,
        .blendConstants = {{0.0f, 0.0f, 0.0f, 0.0f}}};

    const vk::GraphicsPipelineCreateInfo pipeline_create_info {
        .stageCount = 2,
        .pStages = shader_stage_create_infos,
        .pVertexInputState = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_state_create_info,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState = &multisample_state_create_info,
        .pColorBlendState = &color_blend_state_create_info,
        .layout = *pipeline_layout,
        .renderPass = *render_pass,
        .subpass = 0};

    // FIXME
    return device
        ->createGraphicsPipelineUnique(VK_NULL_HANDLE, pipeline_create_info)
        .value;
}

[[nodiscard]] vk::UniqueFramebuffer
create_framebuffer(const vk::UniqueDevice &device,
                   const vk::UniqueImageView &image_view,
                   const vk::UniqueRenderPass &render_pass,
                   std::uint32_t width,
                   std::uint32_t height)
{
    const vk::FramebufferCreateInfo create_info {.renderPass = *render_pass,
                                                 .attachmentCount = 1,
                                                 .pAttachments = &*image_view,
                                                 .width = width,
                                                 .height = height,
                                                 .layers = 1};
    return device->createFramebufferUnique(create_info);
}

[[nodiscard]] std::vector<vk::UniqueFramebuffer>
create_framebuffers(const vk::UniqueDevice &device,
                    const std::vector<vk::UniqueImageView> &image_views,
                    const vk::UniqueRenderPass &render_pass,
                    std::uint32_t width,
                    std::uint32_t height)
{
    std::vector<vk::UniqueFramebuffer> framebuffers;
    framebuffers.reserve(image_views.size());

    for (const auto &image_view : image_views)
    {
        framebuffers.push_back(
            create_framebuffer(device, image_view, render_pass, width, height));
    }

    return framebuffers;
}

[[nodiscard]] vk::UniqueSampler create_sampler(const vk::UniqueDevice &device)
{
    constexpr vk::SamplerCreateInfo create_info {
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

    return device->createSamplerUnique(create_info);
}

[[nodiscard]] Image
create_texture_image(const vk::UniqueDevice &device,
                     const vk::PhysicalDevice &physical_device,
                     const vk::UniqueCommandPool &command_pool,
                     const vk::Queue &graphics_queue,
                     const char *filename)
{
    int width {};
    int height {};
    int channels {};
    auto *const pixels = stbi_load(filename, &width, &height, &channels, 4);
    if (!pixels)
    {
        fatal_error("Failed to load texture image \"" + std::string(filename) +
                    "\"");
    }

    const auto image_size {static_cast<vk::DeviceSize>(width * height * 4)};

    const auto staging_buffer =
        create_buffer(device,
                      physical_device,
                      image_size,
                      vk::BufferUsageFlagBits::eTransferSrc,
                      vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent);

    const auto mapped = static_cast<std::uint8_t *>(
        device->mapMemory(*staging_buffer.memory, 0, image_size));

    std::memcpy(mapped, pixels, static_cast<std::size_t>(image_size));

    stbi_image_free(pixels);

    auto texture_image = create_image(device,
                                      physical_device,
                                      static_cast<std::uint32_t>(width),
                                      static_cast<std::uint32_t>(height),
                                      vk::Format::eR8G8B8A8Srgb,
                                      vk::ImageUsageFlagBits::eTransferDst |
                                          vk::ImageUsageFlagBits::eSampled,
                                      vk::MemoryPropertyFlagBits::eDeviceLocal);

    const auto command_buffer =
        begin_one_time_submit_command_buffer(device, command_pool);

    transition_image_layout(command_buffer,
                            texture_image.image,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eTransferDstOptimal,
                            vk::PipelineStageFlagBits::eTopOfPipe,
                            vk::PipelineStageFlagBits::eTransfer,
                            {},
                            vk::AccessFlagBits::eTransferWrite);

    copy_buffer_to_image(command_buffer,
                         staging_buffer.buffer,
                         texture_image.image,
                         static_cast<std::uint32_t>(width),
                         static_cast<std::uint32_t>(height));

    transition_image_layout(command_buffer,
                            texture_image.image,
                            vk::ImageLayout::eTransferDstOptimal,
                            vk::ImageLayout::eShaderReadOnlyOptimal,
                            vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eFragmentShader,
                            vk::AccessFlagBits::eTransferWrite,
                            vk::AccessFlagBits::eShaderRead);

    end_one_time_submit_command_buffer(command_buffer, graphics_queue);

    return texture_image;
}

[[nodiscard]] vk::UniqueDescriptorPool
create_descriptor_pool(const vk::UniqueDevice &device)
{
    constexpr vk::DescriptorPoolSize pool_sizes[] {
        {vk::DescriptorType::eCombinedImageSampler, 1 + frames_in_flight}};
    const vk::DescriptorPoolCreateInfo create_info {
        .maxSets = 1 + frames_in_flight,
        .poolSizeCount = static_cast<std::uint32_t>(std::size(pool_sizes)),
        .pPoolSizes = pool_sizes};

    return device->createDescriptorPoolUnique(create_info);
}

[[nodiscard]] vk::UniqueDescriptorPool
create_imgui_descriptor_pool(const vk::UniqueDevice &device)
{
    // Copied from imgui/examples/example_glfw_vulkan
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
        {vk::DescriptorType::eInputAttachment, 1000}};
    const vk::DescriptorPoolCreateInfo create_info {
        .maxSets = 1000 * std::size(pool_sizes),
        .poolSizeCount = static_cast<std::uint32_t>(std::size(pool_sizes)),
        .pPoolSizes = pool_sizes};

    return device->createDescriptorPoolUnique(create_info);
}

[[nodiscard]] std::array<vk::DescriptorSet, frames_in_flight>
create_descriptor_sets(
    const vk::UniqueDevice &device,
    const vk::UniqueDescriptorSetLayout &descriptor_set_layout,
    const vk::UniqueDescriptorPool &descriptor_pool,
    const vk::UniqueSampler &sampler,
    const vk::UniqueImageView &texture_image_view)
{
    std::array<vk::DescriptorSetLayout, frames_in_flight> layouts;
    std::fill(layouts.begin(), layouts.end(), *descriptor_set_layout);

    const vk::DescriptorSetAllocateInfo allocate_info {
        .descriptorPool = *descriptor_pool,
        .descriptorSetCount = frames_in_flight,
        .pSetLayouts = layouts.data()};

    std::array<vk::DescriptorSet, frames_in_flight> descriptor_sets;
    if (device->allocateDescriptorSets(
            &allocate_info, descriptor_sets.data()) != vk::Result::eSuccess)
    {
        fatal_error("Failed to allocate descriptor sets");
    }

    const vk::DescriptorImageInfo image_info {
        .sampler = *sampler,
        .imageView = *texture_image_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

    for (std::size_t i {}; i < frames_in_flight; ++i)
    {
        const vk::WriteDescriptorSet descriptor_write {
            .dstSet = descriptor_sets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &image_info};

        device->updateDescriptorSets(descriptor_write, {});
    }

    return descriptor_sets;
}

[[nodiscard]] vk::DescriptorSet create_offscreen_descriptor_set(
    const vk::UniqueDevice &device,
    const vk::UniqueDescriptorSetLayout &descriptor_set_layout,
    const vk::UniqueDescriptorPool &descriptor_pool,
    const vk::UniqueSampler &sampler,
    const vk::UniqueImageView &texture_image_view)
{
    const vk::DescriptorSetAllocateInfo allocate_info {
        .descriptorPool = *descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &*descriptor_set_layout};

    vk::DescriptorSet descriptor_set;
    if (device->allocateDescriptorSets(&allocate_info, &descriptor_set) !=
        vk::Result::eSuccess)
    {
        fatal_error("Failed to allocate descriptor set");
    }

    const vk::DescriptorImageInfo image_info {
        .sampler = *sampler,
        .imageView = *texture_image_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

    const vk::WriteDescriptorSet descriptor_write {
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &image_info};

    device->updateDescriptorSets(descriptor_write, {});

    return descriptor_set;
}

[[nodiscard]] std::array<vk::UniqueCommandBuffer, frames_in_flight>
create_draw_command_buffers(const vk::UniqueDevice &device,
                            const vk::UniqueCommandPool &command_pool)
{
    const vk::CommandBufferAllocateInfo allocate_info {
        .commandPool = *command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = frames_in_flight};

    std::array<vk::CommandBuffer, frames_in_flight> command_buffers;
    if (device->allocateCommandBuffers(
            &allocate_info, command_buffers.data()) != vk::Result::eSuccess)
    {
        fatal_error("Failed to allocate command buffers");
    }

    std::array<vk::UniqueCommandBuffer, frames_in_flight>
        unique_command_buffers;
    for (std::uint32_t i {}; i < frames_in_flight; ++i)
    {
        unique_command_buffers[i] = vk::UniqueCommandBuffer(command_buffers[i]);
    }
    return unique_command_buffers;
}

[[nodiscard]] Image
create_offscreen_color_attachment(const vk::UniqueDevice &device,
                                  const vk::PhysicalDevice &physical_device,
                                  const vk::UniqueCommandPool &command_pool,
                                  const vk::Queue &graphics_queue,
                                  std::uint32_t width,
                                  std::uint32_t height,
                                  vk::Format format)
{
    auto image = create_image(device,
                              physical_device,
                              width,
                              height,
                              format,
                              vk::ImageUsageFlagBits::eColorAttachment |
                                  vk::ImageUsageFlagBits::eSampled,
                              vk::MemoryPropertyFlagBits::eDeviceLocal);

    const auto command_buffer =
        begin_one_time_submit_command_buffer(device, command_pool);

    transition_image_layout(command_buffer,
                            image.image,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eColorAttachmentOptimal,
                            vk::PipelineStageFlagBits::eTopOfPipe,
                            vk::PipelineStageFlagBits::eColorAttachmentOutput,
                            {},
                            vk::AccessFlagBits::eColorAttachmentWrite);

    end_one_time_submit_command_buffer(command_buffer, graphics_queue);

    return image;
}

void check_imgui_vk_result(VkResult result)
{
    if (result != VK_SUCCESS)
    {
        fatal_error(std::string {"ImGui Vulkan error: "} +
                    vk::to_string(static_cast<vk::Result>(result)));
    }
}

[[nodiscard]] constexpr std::uint32_t
scaling_factor(std::uint32_t offscreen_width,
               std::uint32_t offscreen_height,
               std::uint32_t swapchain_width,
               std::uint32_t swapchain_height)
{
    const auto horizontal_scaling_factor = swapchain_width / offscreen_width;
    const auto vertical_scaling_factor = swapchain_height / offscreen_height;
    return std::max(
        std::min(horizontal_scaling_factor, vertical_scaling_factor), 1u);
}

[[nodiscard]] constexpr vk::Extent2D
viewport_extent(std::uint32_t offscreen_width,
                std::uint32_t offscreen_height,
                std::uint32_t swapchain_width,
                std::uint32_t swapchain_height)
{
    const auto scale = scaling_factor(
        offscreen_width, offscreen_height, swapchain_width, swapchain_height);
    return {scale * offscreen_width, scale * offscreen_height};
}

[[nodiscard]] constexpr vk::Offset2D
viewport_offset(std::uint32_t offscreen_width,
                std::uint32_t offscreen_height,
                std::uint32_t swapchain_width,
                std::uint32_t swapchain_height)
{
    const auto extent = viewport_extent(
        offscreen_width, offscreen_height, swapchain_width, swapchain_height);
    const auto offset_x = (static_cast<std::int32_t>(swapchain_width) -
                           static_cast<std::int32_t>(extent.width)) /
                          2;
    const auto offset_y = (static_cast<std::int32_t>(swapchain_height) -
                           static_cast<std::int32_t>(extent.height)) /
                          2;
    return {offset_x, offset_y};
}

[[noreturn]] void glfw_error_callback(int error, const char *description)
{
    std::ostringstream oss;
    oss << "GLFW error " << error << ": " << description << '\n';
    fatal_error(oss.str());
}

[[nodiscard]] GLFWwindow *create_window()
{
    glfwSetErrorCallback(&glfw_error_callback);
    glfwInit();

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    return glfwCreateWindow(1280, 720, "Vulkan engine", nullptr, nullptr);
}

} // namespace

void Renderer::create_renderer(GLFWwindow *window,
                               std::uint32_t render_width,
                               std::uint32_t render_height)
{
    int framebuffer_width;
    int framebuffer_height;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    m_framebuffer_width = static_cast<std::uint32_t>(framebuffer_width);
    m_framebuffer_height = static_cast<std::uint32_t>(framebuffer_height);

    m_instance = create_instance();
#ifdef ENABLE_VALIDATION_LAYERS
    m_debug_messenger = create_debug_utils_messenger(m_instance);
#endif
    m_surface = create_surface(m_instance, window);
    m_physical_device = select_physical_device(m_instance, m_surface);
    m_queue_family_indices =
        get_queue_family_indices(m_physical_device, m_surface).value();
    m_device = create_device(m_physical_device, m_queue_family_indices);
    m_graphics_queue = m_device->getQueue(m_queue_family_indices.graphics, 0);
    m_present_queue = m_device->getQueue(m_queue_family_indices.present, 0);
    m_swapchain = create_swapchain(m_device,
                                   m_physical_device,
                                   m_surface,
                                   m_queue_family_indices,
                                   m_framebuffer_width,
                                   m_framebuffer_width);
    m_swapchain_images =
        m_device->getSwapchainImagesKHR(*m_swapchain.swapchain);
    m_swapchain_image_views = create_swapchain_image_views(
        m_device, m_swapchain_images, m_swapchain.format);
    m_sampler = create_sampler(m_device);
    m_descriptor_pool = create_descriptor_pool(m_device);
    m_imgui_descriptor_pool = create_imgui_descriptor_pool(m_device);
    m_command_pool =
        create_command_pool(m_device, m_queue_family_indices.graphics);
    m_offscreen_width = render_width;
    m_offscreen_height = render_height;
    m_storage_image =
        create_offscreen_color_attachment(m_device,
                                          m_physical_device,
                                          m_command_pool,
                                          m_graphics_queue,
                                          m_offscreen_width,
                                          m_offscreen_height,
                                          vk::Format::eR8G8B8A8Unorm); // FIXME
    m_render_pass = create_render_pass(m_device, m_swapchain.format);
    m_descriptor_set_layout = create_descriptor_set_layout(m_device);
    m_pipeline_layout =
        create_pipeline_layout(m_device, m_descriptor_set_layout);
    m_pipeline = create_pipeline(m_device,
                                 m_pipeline_layout,
                                 m_render_pass,
                                 viewport_offset(m_offscreen_width,
                                                 m_offscreen_height,
                                                 m_swapchain.extent.width,
                                                 m_swapchain.extent.height),
                                 viewport_extent(m_offscreen_width,
                                                 m_offscreen_height,
                                                 m_swapchain.extent.width,
                                                 m_swapchain.extent.height),
                                 {m_framebuffer_width, m_framebuffer_height});
    m_framebuffers = create_framebuffers(m_device,
                                         m_swapchain_image_views,
                                         m_render_pass,
                                         m_swapchain.extent.width,
                                         m_swapchain.extent.height);
    m_descriptor_sets = create_descriptor_sets(m_device,
                                               m_descriptor_set_layout,
                                               m_descriptor_pool,
                                               m_sampler,
                                               m_storage_image.view);
    m_draw_command_buffers =
        create_draw_command_buffers(m_device, m_command_pool);

    for (auto &semaphore : m_image_available_semaphores)
    {
        semaphore = m_device->createSemaphoreUnique(vk::SemaphoreCreateInfo {});
    }
    for (auto &semaphore : m_render_finished_semaphores)
    {
        semaphore = m_device->createSemaphoreUnique(vk::SemaphoreCreateInfo {});
    }
    constexpr vk::FenceCreateInfo fence_create_info {
        .flags = vk::FenceCreateFlagBits::eSignaled};
    for (auto &fence : m_in_flight_fences)
    {
        fence = m_device->createFenceUnique(fence_create_info);
    }
}

void Renderer::init_imgui()
{
    ImGui_ImplVulkan_InitInfo init_info {};
    init_info.Instance = *m_instance;
    init_info.PhysicalDevice = m_physical_device;
    init_info.Device = *m_device;
    init_info.QueueFamily = m_queue_family_indices.graphics;
    init_info.Queue = m_graphics_queue;
    init_info.DescriptorPool = *m_imgui_descriptor_pool;
    init_info.Subpass = 0;
    init_info.MinImageCount = m_swapchain.min_image_count;
    init_info.ImageCount =
        static_cast<std::uint32_t>(m_swapchain_images.size());
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = &check_imgui_vk_result;
    ImGui_ImplVulkan_Init(&init_info, *m_render_pass);

    const auto command_buffer =
        begin_one_time_submit_command_buffer(m_device, m_command_pool);
    ImGui_ImplVulkan_CreateFontsTexture(*command_buffer);
    end_one_time_submit_command_buffer(command_buffer, m_graphics_queue);
    m_device->waitIdle();
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Renderer::shutdown_imgui()
{
    ImGui_ImplVulkan_Shutdown();
}

void Renderer::wait()
{
    m_device->waitIdle();
}

void Renderer::recreate_swapchain()
{
    m_device->waitIdle();

    m_swapchain = create_swapchain(m_device,
                                   m_physical_device,
                                   m_surface,
                                   m_queue_family_indices,
                                   m_framebuffer_width,
                                   m_framebuffer_height);
    m_swapchain_images =
        m_device->getSwapchainImagesKHR(*m_swapchain.swapchain);
    m_swapchain_image_views = create_swapchain_image_views(
        m_device, m_swapchain_images, m_swapchain.format);
    m_render_pass = create_render_pass(m_device, m_swapchain.format);
    m_pipeline = create_pipeline(m_device,
                                 m_pipeline_layout,
                                 m_render_pass,
                                 viewport_offset(m_offscreen_width,
                                                 m_offscreen_height,
                                                 m_swapchain.extent.width,
                                                 m_swapchain.extent.height),
                                 viewport_extent(m_offscreen_width,
                                                 m_offscreen_height,
                                                 m_swapchain.extent.width,
                                                 m_swapchain.extent.height),
                                 {m_framebuffer_width, m_framebuffer_height});
    m_framebuffers = create_framebuffers(m_device,
                                         m_swapchain_image_views,
                                         m_render_pass,
                                         m_swapchain.extent.width,
                                         m_swapchain.extent.height);
}

void Renderer::resize_framebuffer(std::uint32_t framebuffer_width,
                                  std::uint32_t framebuffer_height)
{
    m_framebuffer_width = framebuffer_width;
    m_framebuffer_height = framebuffer_height;
    m_framebuffer_resized = true;
}

void Renderer::begin_frame()
{
    ImGui_ImplVulkan_NewFrame();
}

void Renderer::draw_frame()
{
    const auto wait_result =
        m_device->waitForFences(*m_in_flight_fences[m_current_frame],
                                VK_TRUE,
                                std::numeric_limits<std::uint64_t>::max());
    if (wait_result != vk::Result::eSuccess)
    {
        fatal_error("Error while waiting for fences");
    }

    const auto &[result, image_index] = m_device->acquireNextImageKHR(
        *m_swapchain.swapchain,
        std::numeric_limits<std::uint64_t>::max(),
        *m_image_available_semaphores[m_current_frame]);

    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        recreate_swapchain();
        return;
    }
    else if (result != vk::Result::eSuccess &&
             result != vk::Result::eSuboptimalKHR)
    {
        fatal_error("Failed to acquire swapchain image");
    }

    m_device->resetFences(*m_in_flight_fences[m_current_frame]);

    m_draw_command_buffers[m_current_frame]->reset();

    static std::uint32_t frame {};
    const Push_constants push_constants {.resolution_width = m_offscreen_width,
                                         .resolution_height =
                                             m_offscreen_height,
                                         .frame = frame++};

    const auto &command_buffer = m_draw_command_buffers[m_current_frame];

    command_buffer->begin(vk::CommandBufferBeginInfo {});

    constexpr vk::ClearValue clear_color_value {
        .color = {{{0.0f, 0.0f, 0.0f, 1.0f}}}};

    const vk::RenderPassBeginInfo render_pass_begin_info {
        .renderPass = *m_render_pass,
        .framebuffer = *m_framebuffers[image_index],
        .renderArea = {.offset = {0, 0}, .extent = m_swapchain.extent},
        .clearValueCount = 1,
        .pClearValues = &clear_color_value};

    command_buffer->beginRenderPass(render_pass_begin_info,
                                    vk::SubpassContents::eInline);

    command_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       *m_pipeline_layout,
                                       0,
                                       m_descriptor_sets[m_current_frame],
                                       {});

    command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);

    command_buffer->draw(4, 1, 0, 0);

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *command_buffer);

    command_buffer->endRenderPass();

    command_buffer->end();

    const vk::PipelineStageFlags wait_stages[] {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};

    const vk::SubmitInfo submit_info {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*m_image_available_semaphores[m_current_frame],
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &*m_draw_command_buffers[m_current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*m_render_finished_semaphores[m_current_frame]};

    m_graphics_queue.submit(submit_info, *m_in_flight_fences[m_current_frame]);

    const vk::PresentInfoKHR present_info {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*m_render_finished_semaphores[m_current_frame],
        .swapchainCount = 1,
        .pSwapchains = &*m_swapchain.swapchain,
        .pImageIndices = &image_index};

    const auto present_result = m_present_queue.presentKHR(present_info);

    if (present_result == vk::Result::eErrorOutOfDateKHR ||
        present_result == vk::Result::eSuboptimalKHR || m_framebuffer_resized)
    {
        recreate_swapchain();
        m_framebuffer_resized = false;
    }
    else if (present_result != vk::Result::eSuccess)
    {
        fatal_error("Failed to present swapchain image");
    }

    m_current_frame = (m_current_frame + 1) % frames_in_flight;
}

Renderer::Renderer() : m_window {create_window()}
{
    create_renderer(m_window, 160, 90);

    glfwSetWindowUserPointer(m_window, this);

    glfwSetKeyCallback(m_window, key_callback);
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    init_imgui();
}

Renderer::~Renderer()
{
    Renderer::shutdown_imgui();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Renderer::run()
{
    auto last_frame_time = glfwGetTime();
    auto last_second = last_frame_time;
    int frames = 0;

    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();

        ImGui_ImplGlfw_NewFrame();
        begin_frame();
        ImGui::NewFrame();

        const auto now = glfwGetTime();
        const auto frame_duration = now - last_frame_time;
        last_frame_time = now;

        if (ImGui::Begin("Debug"))
        {
            ImGui::Text("%.3f ms/frame, %.1f fps",
                        1000.0 / static_cast<double>(ImGui::GetIO().Framerate),
                        static_cast<double>(ImGui::GetIO().Framerate));
            ImGui::Text("Press [F] to toggle fullscreen");
        }
        ImGui::End();

        draw_frame();

        ++frames;

        if (glfwGetTime() - last_second > 1.0)
        {
            last_second += 1.0;
            std::cout << frames << " fps\n";
            frames = 0;
        }
    }

    wait();
}

void Renderer::key_callback(GLFWwindow *window,
                            int key,
                            [[maybe_unused]] int scancode,
                            int action,
                            [[maybe_unused]] int mods)
{
    if (action == GLFW_PRESS && key == GLFW_KEY_F)
    {
        const auto app =
            static_cast<Renderer *>(glfwGetWindowUserPointer(window));
        if (app->is_fullscreen())
        {
            app->set_windowed();
        }
        else
        {
            app->set_fullscreen();
        }
    }
}

void Renderer::framebuffer_size_callback(GLFWwindow *window,
                                         int width,
                                         int height)
{
    const auto app = static_cast<Renderer *>(glfwGetWindowUserPointer(window));
    app->resize_framebuffer(static_cast<std::uint32_t>(width),
                            static_cast<std::uint32_t>(height));
}

bool Renderer::is_fullscreen()
{
    return glfwGetWindowMonitor(m_window) != nullptr;
}

void Renderer::set_fullscreen()
{
    // TODO: ideally use the current monitor, not the primary one
    const auto monitor = glfwGetPrimaryMonitor();
    const auto video_mode = glfwGetVideoMode(monitor);
    glfwSetWindowMonitor(m_window,
                         monitor,
                         0,
                         0,
                         video_mode->width,
                         video_mode->height,
                         GLFW_DONT_CARE);
}

void Renderer::set_windowed()
{
    // TODO: use previous windowed size, scale must be taken into account
    const auto monitor = glfwGetPrimaryMonitor();
    const auto video_mode = glfwGetVideoMode(monitor);
    constexpr int width {1280};
    constexpr int height {720};
    glfwSetWindowMonitor(m_window,
                         nullptr,
                         (video_mode->width - width) / 2,
                         (video_mode->height - height) / 2,
                         width,
                         height,
                         GLFW_DONT_CARE);
}
