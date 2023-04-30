
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
#include <bit>
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
    VkImage render_image;
    VkImageView render_image_view;
    VmaAllocation render_image_allocation;
    std::uint32_t render_image_width;
    std::uint32_t render_image_height;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
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

void store_image_to_png(context &ctx, const char *filename)
{
    VkBufferCreateInfo buffer_create_info {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size =
        ctx.render_image_width * ctx.render_image_height * 4 * sizeof(float);
    buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 1;
    buffer_create_info.pQueueFamilyIndices = &ctx.compute_queue_family;

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.flags =
        VMA_ALLOCATION_CREATE_MAPPED_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    VkBuffer staging_buffer {};
    VmaAllocation staging_buffer_allocation {};
    VmaAllocationInfo staging_buffer_allocation_info {};
    vk_check(vmaCreateBuffer(ctx.allocator,
                             &buffer_create_info,
                             &allocation_create_info,
                             &staging_buffer,
                             &staging_buffer_allocation,
                             &staging_buffer_allocation_info));

    VkCommandBufferAllocateInfo command_buffer_allocate_info {};
    command_buffer_allocate_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandPool = ctx.command_pool;
    command_buffer_allocate_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vk_check(vkAllocateCommandBuffers(
        ctx.device, &command_buffer_allocate_info, &command_buffer));

    VkCommandBufferBeginInfo command_buffer_begin_info {};
    command_buffer_begin_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vk_check(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));

    VkImageSubresourceRange subresource_range {};
    subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_range.baseMipLevel = 0;
    subresource_range.levelCount = 1;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount = 1;

    VkImageMemoryBarrier image_memory_barrier {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = ctx.render_image;
    image_memory_barrier.subresourceRange = subresource_range;

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
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
    region.bufferImageHeight = ctx.render_image_height;
    region.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               .mipLevel = 0,
                               .baseArrayLayer = 0,
                               .layerCount = 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {ctx.render_image_width, ctx.render_image_height, 1};
    vkCmdCopyImageToBuffer(command_buffer,
                           ctx.render_image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           staging_buffer,
                           1,
                           &region);

    vk_check(vkEndCommandBuffer(command_buffer));

    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    vk_check(vkQueueSubmit(ctx.compute_queue, 1, &submit_info, VK_NULL_HANDLE));

    vk_check(vkQueueWaitIdle(ctx.compute_queue));

    vkFreeCommandBuffers(ctx.device, ctx.command_pool, 1, &command_buffer);

    const auto *const hdr_image_data =
        reinterpret_cast<float *>(staging_buffer_allocation_info.pMappedData);
    const auto image_size =
        ctx.render_image_width * ctx.render_image_height * 4;
    std::vector<std::uint8_t> rgba8_image_data(image_size);
    for (std::uint32_t i {}; i < image_size; ++i)
    {
        rgba8_image_data[i] =
            static_cast<std::uint8_t>(hdr_image_data[i] * 255.0f);
    }

    if (!stbi_write_png(filename,
                        static_cast<int>(ctx.render_image_width),
                        static_cast<int>(ctx.render_image_height),
                        4,
                        rgba8_image_data.data(),
                        static_cast<int>(ctx.render_image_width * 4)))
    {
        fatal_error("Failed to write PNG image");
    }

    vmaDestroyBuffer(ctx.allocator, staging_buffer, staging_buffer_allocation);
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

    {
        ctx.render_image_width = 160;
        ctx.render_image_height = 90;

        constexpr auto format = VK_FORMAT_R32G32B32A32_SFLOAT;

        VkImageCreateInfo image_create_info {};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.format = format;
        image_create_info.extent.width = ctx.render_image_width;
        image_create_info.extent.height = ctx.render_image_height;
        image_create_info.extent.depth = 1;
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = VK_IMAGE_USAGE_STORAGE_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.queueFamilyIndexCount = 1;
        image_create_info.pQueueFamilyIndices = &ctx.compute_queue_family;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocation_create_info {};
        allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        vk_check(vmaCreateImage(ctx.allocator,
                                &image_create_info,
                                &allocation_create_info,
                                &ctx.render_image,
                                &ctx.render_image_allocation,
                                nullptr));

        VkImageViewCreateInfo image_view_create_info {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = ctx.render_image;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;
        image_view_create_info.subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        vk_check(vkCreateImageView(ctx.device,
                                   &image_view_create_info,
                                   nullptr,
                                   &ctx.render_image_view));
    }

    {
        VkCommandPoolCreateInfo command_pool_create_info {};
        command_pool_create_info.sType =
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.queueFamilyIndex = ctx.compute_queue_family;
        vk_check(vkCreateCommandPool(
            ctx.device, &command_pool_create_info, nullptr, &ctx.command_pool));
    }

    {
        VkCommandBufferAllocateInfo command_buffer_allocate_info {};
        command_buffer_allocate_info.sType =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandPool = ctx.command_pool;
        command_buffer_allocate_info.commandBufferCount = 1;

        vk_check(vkAllocateCommandBuffers(
            ctx.device, &command_buffer_allocate_info, &ctx.command_buffer));
    }
}

void shutdown(context &ctx)
{
    if (ctx.device)
    {
        vk_check(vkDeviceWaitIdle(ctx.device));
    }

    if (ctx.device)
    {
        if (ctx.command_pool)
        {
            vkDestroyCommandPool(ctx.device, ctx.command_pool, nullptr);
        }

        if (ctx.render_image_allocation)
        {
            vmaFreeMemory(ctx.allocator, ctx.render_image_allocation);
        }

        if (ctx.render_image_view)
        {
            vkDestroyImageView(ctx.device, ctx.render_image_view, nullptr);
        }

        if (ctx.render_image)
        {
            vkDestroyImage(ctx.device, ctx.render_image, nullptr);
        }
    }

    if (ctx.allocator)
    {
        vmaDestroyAllocator(ctx.allocator);
    }

    if (ctx.device)
    {
        vkDestroyDevice(ctx.device, nullptr);
    }

    if (ctx.instance)
    {
#ifdef ENABLE_VALIDATION_LAYERS
        if (ctx.debug_messenger)
        {
            vkDestroyDebugUtilsMessengerEXT(
                ctx.instance, ctx.debug_messenger, nullptr);
        }
#endif

        vkDestroyInstance(ctx.instance, nullptr);
    }

    ctx = {};
}

void run(context &ctx)
{
    VkCommandBufferBeginInfo command_buffer_begin_info {};
    command_buffer_begin_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vk_check(
        vkBeginCommandBuffer(ctx.command_buffer, &command_buffer_begin_info));

    VkImageSubresourceRange subresource_range {};
    subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_range.baseMipLevel = 0;
    subresource_range.levelCount = 1;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount = 1;

    VkImageMemoryBarrier image_memory_barrier {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.srcAccessMask = VK_ACCESS_NONE;
    image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = ctx.render_image;
    image_memory_barrier.subresourceRange = subresource_range;
    vkCmdPipelineBarrier(ctx.command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         {},
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &image_memory_barrier);

    const VkClearColorValue clear_color {
        .float32 = {0.75f, 0.25f, 0.25f, 1.0f}};

    vkCmdClearColorImage(ctx.command_buffer,
                         ctx.render_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &clear_color,
                         1,
                         &subresource_range);

    VkMemoryBarrier memory_barrier {};
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(ctx.command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         {},
                         1,
                         &memory_barrier,
                         0,
                         nullptr,
                         0,
                         nullptr);

    vk_check(vkEndCommandBuffer(ctx.command_buffer));

    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ctx.command_buffer;
    vk_check(vkQueueSubmit(ctx.compute_queue, 1, &submit_info, VK_NULL_HANDLE));

    vk_check(vkQueueWaitIdle(ctx.compute_queue));

    store_image_to_png(ctx, "image.png");
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

template <typename EF>
class scope_exit
{
public:
    explicit scope_exit(EF &&fn) : m_fn {std::forward<EF>(fn)}
    {
    }

    ~scope_exit() noexcept
    {
        m_fn();
    }

private:
    EF m_fn;
};

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2)      CONCATENATE_IMPL(s1, s2)

#ifdef __COUNTER__
#define ANONYMOUS_VARIABLE(s) CONCATENATE(s, __COUNTER__)
#else
#define ANONYMOUS_VARIABLE(s) CONCATENATE(s, __LINE__)
#endif

#define SCOPE_EXIT(fn)                                                         \
    auto ANONYMOUS_VARIABLE(scope_exit_object_) = scope_exit(fn)

int main()
{
    try
    {
        context ctx {};
        SCOPE_EXIT([&] { shutdown(ctx); });
        init(ctx);
        run(ctx);
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
