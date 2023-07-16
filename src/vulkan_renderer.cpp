#include "vulkan_renderer.hpp"

#include "tiny_obj_loader.h"

#include "stb_image_write.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace
{

struct Push_constants
{
    std::uint32_t rng_seed;
};

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

[[nodiscard]] std::vector<std::uint32_t> read_binary_file(const char *file_name)
{
    const std::filesystem::path path(file_name);
    if (!std::filesystem::exists(path))
    {
        std::ostringstream oss;
        oss << "File " << path << " does not exist";
        throw std::runtime_error(oss.str());
    }

    const auto file_size = std::filesystem::file_size(path);
    const auto buffer_length =
        (file_size + sizeof(std::uint32_t) - 1u) / sizeof(std::uint32_t);

    std::vector<std::uint32_t> buffer(buffer_length);

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::ostringstream oss;
        oss << "Failed to open file " << path;
        throw std::runtime_error(oss.str());
    }

    file.read(
        reinterpret_cast<char *>(buffer.data()),
        static_cast<std::streamsize>(buffer_length * sizeof(std::uint32_t)));
    if (file.eof())
    {
        std::ostringstream oss;
        oss << "End-of-file reached while reading file " << path;
        throw std::runtime_error(oss.str());
    }

    return buffer;
}

} // namespace

Unique_allocator::Unique_allocator(const VmaAllocatorCreateInfo &create_info)
{
    const auto result = vmaCreateAllocator(&create_info, &m_allocator);
    vk::resultCheck(static_cast<vk::Result>(result), "vmaCreateAllocator");
}

Unique_allocator::~Unique_allocator() noexcept
{
    if (m_allocator)
    {
        vmaDestroyAllocator(m_allocator);
    }
}

Unique_allocator::Unique_allocator(Unique_allocator &&rhs) noexcept
    : m_allocator {rhs.m_allocator}
{
    rhs.m_allocator = {};
}

Unique_allocator &Unique_allocator::operator=(Unique_allocator &&rhs) noexcept
{
    const auto old_allocator = m_allocator;
    m_allocator = rhs.m_allocator;
    rhs.m_allocator = {};
    if (old_allocator)
    {
        vmaDestroyAllocator(old_allocator);
    }
    return *this;
}

Unique_buffer::Unique_buffer(
    VmaAllocator allocator,
    const vk::BufferCreateInfo &buffer_create_info,
    const VmaAllocationCreateInfo &allocation_create_info,
    VmaAllocationInfo *allocation_info)
    : m_allocator {allocator}
{
    const auto result = vmaCreateBuffer(
        m_allocator,
        reinterpret_cast<const VkBufferCreateInfo *>(&buffer_create_info),
        &allocation_create_info,
        reinterpret_cast<VkBuffer *>(&m_buffer),
        &m_allocation,
        allocation_info);
    vk::resultCheck(static_cast<vk::Result>(result), "vmaCreateBuffer");
}

Unique_buffer::~Unique_buffer() noexcept
{
    if (m_buffer)
    {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
}

Unique_buffer::Unique_buffer(Unique_buffer &&rhs) noexcept
    : m_buffer {rhs.m_buffer},
      m_allocation {rhs.m_allocation},
      m_allocator {rhs.m_allocator}
{
    rhs.m_buffer = vk::Buffer {};
    rhs.m_allocation = {};
    rhs.m_allocator = {};
}

Unique_buffer &Unique_buffer::operator=(Unique_buffer &&rhs) noexcept
{
    const auto old_buffer = m_buffer;
    const auto old_allocation = m_allocation;
    const auto old_allocator = m_allocator;
    m_buffer = rhs.m_buffer;
    m_allocation = rhs.m_allocation;
    m_allocator = rhs.m_allocator;
    rhs.m_buffer = vk::Buffer {};
    rhs.m_allocation = {};
    rhs.m_allocator = {};
    if (old_buffer)
    {
        vmaDestroyBuffer(old_allocator, old_buffer, old_allocation);
    }
    return *this;
}

Unique_image::Unique_image(
    VmaAllocator allocator,
    const vk::ImageCreateInfo &image_create_info,
    const VmaAllocationCreateInfo &allocation_create_info)
    : m_allocator {allocator}
{
    const auto result = vmaCreateImage(
        m_allocator,
        reinterpret_cast<const VkImageCreateInfo *>(&image_create_info),
        &allocation_create_info,
        reinterpret_cast<VkImage *>(&m_image),
        &m_allocation,
        nullptr);
    vk::resultCheck(static_cast<vk::Result>(result), "vmaCreateImage");
}

Unique_image::~Unique_image() noexcept
{
    if (m_image)
    {
        vmaDestroyImage(m_allocator, m_image, m_allocation);
    }
}

Unique_image::Unique_image(Unique_image &&rhs) noexcept
    : m_image {rhs.m_image},
      m_allocation {rhs.m_allocation},
      m_allocator {rhs.m_allocator}
{
    rhs.m_image = vk::Image {};
    rhs.m_allocation = {};
    rhs.m_allocator = {};
}

Unique_image &Unique_image::operator=(Unique_image &&rhs) noexcept
{
    const auto old_image = m_image;
    const auto old_allocation = m_allocation;
    const auto old_allocator = m_allocator;
    m_image = rhs.m_image;
    m_allocation = rhs.m_allocation;
    m_allocator = rhs.m_allocator;
    rhs.m_image = vk::Image {};
    rhs.m_allocation = {};
    rhs.m_allocator = {};
    if (old_image)
    {
        vmaDestroyImage(old_allocator, old_image, old_allocation);
    }
    return *this;
}

Vulkan_renderer::Vulkan_renderer(std::uint32_t render_width,
                                 std::uint32_t render_height)
{
    constexpr std::uint32_t api_version {VK_API_VERSION_1_3};

    constexpr const char *device_extension_names[] {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};
    constexpr auto device_extension_count =
        static_cast<std::uint32_t>(std::size(device_extension_names));

    create_instance(api_version);
    select_physical_device(device_extension_count, device_extension_names);
    create_device(device_extension_count, device_extension_names);
    create_allocator(api_version);
    m_queue = m_device->getQueue(m_queue_family_index, 0);
    create_command_pool();
    create_storage_image(render_width, render_height);
    create_geometry_buffers();
    create_blas();
    create_tlas();
    create_descriptor_set_layout();
    create_descriptor_pool();
    create_descriptor_set();
    create_ray_tracing_pipeline();
    create_shader_binding_table();
}

void Vulkan_renderer::render()
{
    const auto command_buffer = begin_one_time_submit_command_buffer();

    command_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR,
                                m_ray_tracing_pipeline.get());

    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR,
                                      m_ray_tracing_pipeline_layout.get(),
                                      0,
                                      m_descriptor_set,
                                      {});

    const Push_constants push_constants {.rng_seed = 1};

    command_buffer.pushConstants(m_ray_tracing_pipeline_layout.get(),
                                 vk::ShaderStageFlagBits::eRaygenKHR |
                                     vk::ShaderStageFlagBits::eClosestHitKHR,
                                 0,
                                 sizeof(Push_constants),
                                 &push_constants);

    command_buffer.traceRaysKHR(m_rgen_region,
                                m_miss_region,
                                m_hit_region,
                                m_call_region,
                                m_width,
                                m_height,
                                1);

    end_one_time_submit_command_buffer(command_buffer);
}

void Vulkan_renderer::store_to_png(const char *file_name)
{
    constexpr auto format = vk::Format::eR8G8B8A8Srgb;

    const vk::ImageCreateInfo image_create_info {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {m_width, m_height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eTransferDst |
                 vk::ImageUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined};

    VmaAllocationCreateInfo image_allocation_create_info {};
    image_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    const auto final_image = Unique_image(
        m_allocator.get(), image_create_info, image_allocation_create_info);

    const vk::BufferCreateInfo staging_buffer_create_info {
        .size = m_width * m_height * 4,
        .usage = vk::BufferUsageFlagBits::eTransferDst};

    VmaAllocationCreateInfo staging_buffer_allocation_create_info {};
    staging_buffer_allocation_create_info.flags =
        VMA_ALLOCATION_CREATE_MAPPED_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    staging_buffer_allocation_create_info.usage =
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    VmaAllocationInfo staging_buffer_allocation_info {};
    const Unique_buffer staging_buffer(m_allocator.get(),
                                       staging_buffer_create_info,
                                       staging_buffer_allocation_create_info,
                                       &staging_buffer_allocation_info);

    {
        const auto command_buffer = begin_one_time_submit_command_buffer();

        constexpr vk::ImageSubresourceRange subresource_range {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1};

        vk::ImageMemoryBarrier image_memory_barrier {
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = final_image.get(),
            .subresourceRange = subresource_range};

        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                       vk::PipelineStageFlagBits::eTransfer,
                                       {},
                                       {},
                                       {},
                                       image_memory_barrier);

        constexpr vk::ImageSubresourceLayers subresource_layers {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1};

        const std::array offsets {
            vk::Offset3D {0, 0, 0},
            vk::Offset3D {static_cast<std::int32_t>(m_width),
                          static_cast<std::int32_t>(m_height),
                          1}};

        const vk::ImageBlit image_blit {.srcSubresource = subresource_layers,
                                        .srcOffsets = offsets,
                                        .dstSubresource = subresource_layers,
                                        .dstOffsets = offsets};

        command_buffer.blitImage(m_storage_image.get(),
                                 vk::ImageLayout::eGeneral,
                                 final_image.get(),
                                 vk::ImageLayout::eTransferDstOptimal,
                                 image_blit,
                                 vk::Filter::eNearest);

        image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        image_memory_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        image_memory_barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;

        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                       vk::PipelineStageFlagBits::eTransfer,
                                       {},
                                       {},
                                       {},
                                       image_memory_barrier);

        const vk::BufferImageCopy copy_region {
            .bufferOffset = 0,
            .bufferImageHeight = m_height,
            .imageSubresource = subresource_layers,
            .imageOffset = {0, 0, 0},
            .imageExtent = {m_width, m_height, 1}};

        command_buffer.copyImageToBuffer(final_image.get(),
                                         vk::ImageLayout::eTransferSrcOptimal,
                                         staging_buffer.get(),
                                         copy_region);

        end_one_time_submit_command_buffer(command_buffer);
    }

    std::cout << "Writing image to \"" << file_name << "\"..." << std::flush;
    if (!stbi_write_png(file_name,
                        static_cast<int>(m_width),
                        static_cast<int>(m_height),
                        4,
                        staging_buffer_allocation_info.pMappedData,
                        static_cast<int>(m_width * 4)))
    {
        throw std::runtime_error("Failed to write PNG image");
    }
    std::cout << " Done" << std::endl;
}

void Vulkan_renderer::create_instance(std::uint32_t api_version)
{
    VULKAN_HPP_DEFAULT_DISPATCHER.init(
        m_dl.getProcAddress<PFN_vkGetInstanceProcAddr>(
            "vkGetInstanceProcAddr"));

    const vk::ApplicationInfo application_info {.apiVersion = api_version};

#ifndef NDEBUG

    const auto layer_properties = vk::enumerateInstanceLayerProperties();

    constexpr auto khronos_validation_layer = "VK_LAYER_KHRONOS_validation";
    if (std::none_of(layer_properties.begin(),
                     layer_properties.end(),
                     [](const vk::LayerProperties &properties) {
                         return std::strcmp(properties.layerName,
                                            khronos_validation_layer) == 0;
                     }))
    {
        throw std::runtime_error("Validation layers are not supported");
    }

    std::vector<const char *> required_extensions {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

    std::vector<const char *> unsupported_extensions;

    const auto extension_properties =
        vk::enumerateInstanceExtensionProperties();

    for (const auto extension_name : required_extensions)
    {
        if (std::none_of(
                extension_properties.begin(),
                extension_properties.end(),
                [extension_name](const VkExtensionProperties &properties) {
                    return std::strcmp(properties.extensionName,
                                       extension_name) == 0;
                }))
        {
            unsupported_extensions.push_back(extension_name);
        }
    }

    if (!unsupported_extensions.empty())
    {
        std::ostringstream oss;
        oss << "The following instance extensions are not supported:";
        for (const auto extension_name : unsupported_extensions)
        {
            oss << "\n    " << extension_name;
        }
        throw std::runtime_error(oss.str());
    }

    constexpr vk::DebugUtilsMessengerCreateInfoEXT
        debug_utils_messenger_create_info {
            .messageSeverity =
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = debug_callback};

    const vk::InstanceCreateInfo instance_create_info {
        .pNext = &debug_utils_messenger_create_info,
        .pApplicationInfo = &application_info,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &khronos_validation_layer,
        .enabledExtensionCount =
            static_cast<std::uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data()};

    m_instance = vk::createInstanceUnique(instance_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get());

    m_debug_messenger = m_instance->createDebugUtilsMessengerEXTUnique(
        debug_utils_messenger_create_info);

#else

    const vk::InstanceCreateInfo instance_create_info {.pApplicationInfo =
                                                           &application_info};

    m_instance = vk::createInstanceUnique(instance_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get());

#endif
}

void Vulkan_renderer::select_physical_device(
    std::uint32_t device_extension_count,
    const char *const *device_extension_names)
{
    const auto physical_devices = m_instance->enumeratePhysicalDevices();

    for (const auto physical_device : physical_devices)
    {
        // Graphics queue
        m_queue_family_index = get_queue_family_index(physical_device);
        if (m_queue_family_index == std::numeric_limits<std::uint32_t>::max())
        {
            continue;
        }

        // Extensions
        const auto extension_properties =
            physical_device.enumerateDeviceExtensionProperties();
        bool all_extensions_supported {true};
        for (std::uint32_t i {}; i < device_extension_count; ++i)
        {
            const auto extension_name = device_extension_names[i];
            if (std::none_of(extension_properties.begin(),
                             extension_properties.end(),
                             [extension_name](
                                 const vk::ExtensionProperties &properties) {
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

        // Features
        // FIXME: put these and the features in create_device in the same place
        const auto features = physical_device.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan12Features,
            vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
            vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();
        if (!features.get<vk::PhysicalDeviceVulkan12Features>()
                 .bufferDeviceAddress ||
            !features.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>()
                 .accelerationStructure ||
            !features.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>()
                 .rayTracingPipeline)
        {
            continue;
        }

        // Storage image format
        const auto format_properties = physical_device.getFormatProperties(
            vk::Format::eR32G32B32A32Sfloat);
        if (!(format_properties.optimalTilingFeatures &
              (vk::FormatFeatureFlagBits::eStorageImage |
               vk::FormatFeatureFlagBits::eTransferSrc)))
        {
            continue;
        }

        m_physical_device = physical_device;
        m_ray_tracing_properties =
            m_physical_device
                .getProperties2<
                    vk::PhysicalDeviceProperties2,
                    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>()
                .get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
        return;
    }

    throw std::runtime_error("Failed to find a suitable physical device");
}

std::uint32_t
Vulkan_renderer::get_queue_family_index(vk::PhysicalDevice physical_device)
{
    const auto properties = physical_device.getQueueFamilyProperties();

    for (std::uint32_t i {}; i < static_cast<std::uint32_t>(properties.size());
         ++i)
    {
        // Note: vkCmdTraceRaysKHR are executed on compute queues
        if (properties[i].queueFlags & vk::QueueFlagBits::eCompute)
        {
            return i;
        }
    }

    return std::numeric_limits<std::uint32_t>::max();
}

void Vulkan_renderer::create_device(std::uint32_t device_extension_count,
                                    const char *const *device_extension_names)
{
    constexpr float queue_priority {1.0f};

    const vk::DeviceQueueCreateInfo queue_create_info {
        .queueFamilyIndex = m_queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority};

    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR
        ray_tracing_pipeline_features {.rayTracingPipeline = VK_TRUE};
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR
        acceleration_structure_features_khr {.pNext =
                                                 &ray_tracing_pipeline_features,
                                             .accelerationStructure = VK_TRUE};
    vk::PhysicalDeviceVulkan12Features vulkan_1_2_features {
        .pNext = &acceleration_structure_features_khr,
        .scalarBlockLayout = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE};
    const vk::PhysicalDeviceFeatures2 features_2 {.pNext =
                                                      &vulkan_1_2_features};

    const vk::DeviceCreateInfo device_create_info {
        .pNext = &features_2,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = device_extension_count,
        .ppEnabledExtensionNames = device_extension_names};

    m_device = m_physical_device.createDeviceUnique(device_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device.get());
}

void Vulkan_renderer::create_allocator(std::uint32_t api_version)
{
    VmaVulkanFunctions vulkan_functions {};
    vulkan_functions.vkGetInstanceProcAddr =
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr =
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_create_info {};
    allocator_create_info.flags =
        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_create_info.physicalDevice = m_physical_device;
    allocator_create_info.device = m_device.get();
    allocator_create_info.pVulkanFunctions = &vulkan_functions;
    allocator_create_info.instance = m_instance.get();
    allocator_create_info.vulkanApiVersion = api_version;

    m_allocator = Unique_allocator(allocator_create_info);
}

void Vulkan_renderer::create_command_pool()
{
    const vk::CommandPoolCreateInfo create_info {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = m_queue_family_index};

    m_command_pool = m_device->createCommandPoolUnique(create_info);
}

[[nodiscard]] vk::CommandBuffer
Vulkan_renderer::begin_one_time_submit_command_buffer()
{
    const vk::CommandBufferAllocateInfo allocate_info {
        .commandPool = m_command_pool.get(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1};

    auto command_buffer =
        m_device->allocateCommandBuffers(allocate_info).front();

    constexpr vk::CommandBufferBeginInfo begin_info {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};

    command_buffer.begin(begin_info);

    return command_buffer;
}

void Vulkan_renderer::end_one_time_submit_command_buffer(
    vk::CommandBuffer command_buffer)
{
    command_buffer.end();

    const vk::SubmitInfo submit_info {.commandBufferCount = 1,
                                      .pCommandBuffers = &command_buffer};

    m_queue.submit(submit_info);
    m_queue.waitIdle();
}

void Vulkan_renderer::create_storage_image(std::uint32_t width,
                                           std::uint32_t height)
{
    m_width = width;
    m_height = height;

    constexpr auto format = vk::Format::eR32G32B32A32Sfloat;

    const vk::ImageCreateInfo image_create_info {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {m_width, m_height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eStorage |
                 vk::ImageUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    m_storage_image = Unique_image(
        m_allocator.get(), image_create_info, allocation_create_info);

    constexpr vk::ImageSubresourceRange subresource_range {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    const vk::ImageViewCreateInfo image_view_create_info {
        .image = m_storage_image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = subresource_range};

    m_storage_image_view =
        m_device->createImageViewUnique(image_view_create_info);

    const auto command_buffer = begin_one_time_submit_command_buffer();

    const vk::ImageMemoryBarrier image_memory_barrier {
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead |
                         vk::AccessFlagBits::eShaderWrite |
                         vk::AccessFlagBits::eTransferRead,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eGeneral,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_storage_image.get(),
        .subresourceRange = subresource_range};

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eRayTracingShaderKHR |
            vk::PipelineStageFlagBits::eTransfer,
        {},
        {},
        {},
        image_memory_barrier);

    end_one_time_submit_command_buffer(command_buffer);
}

Unique_buffer Vulkan_renderer::create_buffer(vk::DeviceSize size,
                                             vk::BufferUsageFlags usage)
{
    const vk::BufferCreateInfo buffer_create_info {
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    return {
        m_allocator.get(), buffer_create_info, allocation_create_info, nullptr};
}

Unique_buffer
Vulkan_renderer::create_buffer_mapped(vk::DeviceSize size,
                                      vk::BufferUsageFlags usage,
                                      VmaAllocationInfo *allocation_info)
{
    const vk::BufferCreateInfo buffer_create_info {
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.flags =
        VMA_ALLOCATION_CREATE_MAPPED_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    return {m_allocator.get(),
            buffer_create_info,
            allocation_create_info,
            allocation_info};
}

void Vulkan_renderer::create_vertex_buffer(const std::vector<float> &vertices)
{
    m_vertex_count = static_cast<std::uint32_t>(vertices.size() / 3);
    m_vertex_buffer_size = vertices.size() * sizeof(float);

    VmaAllocationInfo staging_buffer_allocation_info {};
    const auto staging_buffer =
        create_buffer_mapped(m_vertex_buffer_size,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             &staging_buffer_allocation_info);

    m_vertex_buffer =
        create_buffer(m_vertex_buffer_size,
                      vk::BufferUsageFlagBits::eStorageBuffer |
                          vk::BufferUsageFlagBits::eTransferDst |
                          vk::BufferUsageFlagBits::eShaderDeviceAddress |
                          vk::BufferUsageFlagBits::
                              eAccelerationStructureBuildInputReadOnlyKHR);

    std::memcpy(staging_buffer_allocation_info.pMappedData,
                vertices.data(),
                m_vertex_buffer_size);

    const auto command_buffer = begin_one_time_submit_command_buffer();

    const vk::BufferCopy region {
        .srcOffset = 0, .dstOffset = 0, .size = m_vertex_buffer_size};

    command_buffer.copyBuffer(
        staging_buffer.get(), m_vertex_buffer.get(), region);

    end_one_time_submit_command_buffer(command_buffer);
}

void Vulkan_renderer::create_normals_buffer(const std::vector<float> &normals)
{
    m_normals_buffer_size = normals.size() * sizeof(float);

    VmaAllocationInfo staging_buffer_allocation_info {};
    const auto staging_buffer =
        create_buffer_mapped(m_normals_buffer_size,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             &staging_buffer_allocation_info);

    m_normals_buffer =
        create_buffer(m_normals_buffer_size,
                      vk::BufferUsageFlagBits::eStorageBuffer |
                          vk::BufferUsageFlagBits::eTransferDst |
                          vk::BufferUsageFlagBits::eShaderDeviceAddress |
                          vk::BufferUsageFlagBits::
                              eAccelerationStructureBuildInputReadOnlyKHR);

    std::memcpy(staging_buffer_allocation_info.pMappedData,
                normals.data(),
                m_normals_buffer_size);

    const auto command_buffer = begin_one_time_submit_command_buffer();

    const vk::BufferCopy region {
        .srcOffset = 0, .dstOffset = 0, .size = m_normals_buffer_size};

    command_buffer.copyBuffer(
        staging_buffer.get(), m_normals_buffer.get(), region);

    end_one_time_submit_command_buffer(command_buffer);
}

void Vulkan_renderer::create_index_buffer(
    const std::vector<std::uint32_t> &indices)
{
    m_index_count = static_cast<std::uint32_t>(indices.size());
    m_index_buffer_size = indices.size() * sizeof(float);

    VmaAllocationInfo staging_buffer_allocation_info {};
    const auto staging_buffer =
        create_buffer_mapped(m_index_buffer_size,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             &staging_buffer_allocation_info);

    m_index_buffer =
        create_buffer(m_index_buffer_size,
                      vk::BufferUsageFlagBits::eStorageBuffer |
                          vk::BufferUsageFlagBits::eTransferDst |
                          vk::BufferUsageFlagBits::eShaderDeviceAddress |
                          vk::BufferUsageFlagBits::
                              eAccelerationStructureBuildInputReadOnlyKHR);

    std::memcpy(staging_buffer_allocation_info.pMappedData,
                indices.data(),
                m_index_buffer_size);

    const auto command_buffer = begin_one_time_submit_command_buffer();

    const vk::BufferCopy region {
        .srcOffset = 0, .dstOffset = 0, .size = m_index_buffer_size};

    command_buffer.copyBuffer(
        staging_buffer.get(), m_index_buffer.get(), region);

    end_one_time_submit_command_buffer(command_buffer);
}

void Vulkan_renderer::create_geometry_buffers()
{
    tinyobj::ObjReader reader;
    reader.ParseFromFile("../resources/bunny.obj");
    if (!reader.Valid())
    {
        throw std::runtime_error(reader.Error());
    }

    const auto &shapes = reader.GetShapes();
    if (shapes.size() != 1)
    {
        throw std::runtime_error("OBJ file contains more than one shape");
    }

    const auto &indices = shapes.front().mesh.indices;

    std::vector<std::uint32_t> vertex_indices(indices.size());
    for (std::size_t i {}; i < indices.size(); ++i)
    {
        vertex_indices[i] = static_cast<std::uint32_t>(indices[i].vertex_index);
    }

    const auto &obj_vertices = reader.GetAttrib().vertices;
    const auto &obj_normals = reader.GetAttrib().normals;
    std::vector<float> normals(reader.GetAttrib().vertices.size());
    for (const auto index : indices)
    {
        const auto vertex_index = static_cast<std::size_t>(index.vertex_index);
        const auto normal_index = static_cast<std::size_t>(index.normal_index);
        normals[vertex_index * 3 + 0] = obj_normals[normal_index * 3 + 0];
        normals[vertex_index * 3 + 1] = obj_normals[normal_index * 3 + 1];
        normals[vertex_index * 3 + 2] = obj_normals[normal_index * 3 + 2];
    }

    create_vertex_buffer(obj_vertices);
    create_normals_buffer(normals);
    create_index_buffer(vertex_indices);
}

vk::DeviceAddress Vulkan_renderer::get_device_address(vk::Buffer buffer)
{
    const vk::BufferDeviceAddressInfo address_info {.buffer = buffer};
    return m_device->getBufferAddress(address_info);
}

vk::DeviceAddress Vulkan_renderer::get_device_address(
    vk::AccelerationStructureKHR acceleration_structure)
{
    const vk::AccelerationStructureDeviceAddressInfoKHR address_info {
        .accelerationStructure = acceleration_structure};
    return m_device->getAccelerationStructureAddressKHR(address_info);
}

void Vulkan_renderer::create_blas()
{
    const auto vertex_buffer_address =
        get_device_address(m_vertex_buffer.get());
    const auto index_buffer_address = get_device_address(m_index_buffer.get());

    const vk::AccelerationStructureGeometryTrianglesDataKHR triangles {
        .vertexFormat = vk::Format::eR32G32B32Sfloat,
        .vertexData = {.deviceAddress = vertex_buffer_address},
        .vertexStride = 3 * sizeof(float),
        .maxVertex = m_vertex_count - 1,
        .indexType = vk::IndexType::eUint32,
        .indexData = {.deviceAddress = index_buffer_address}};

    const vk::AccelerationStructureGeometryKHR geometry {
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry = {.triangles = triangles},
        .flags = vk::GeometryFlagBitsKHR::eOpaque};

    const vk::AccelerationStructureBuildRangeInfoKHR build_range_info {
        .primitiveCount = m_index_count / 3,
        .primitiveOffset = 0,
        .firstVertex = 0};

    vk::AccelerationStructureBuildGeometryInfoKHR build_geometry_info {
        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        .flags = vk::BuildAccelerationStructureFlagBitsKHR ::ePreferFastTrace,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &geometry};

    const auto build_sizes_info =
        m_device->getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice,
            build_geometry_info,
            build_range_info.primitiveCount);

    const auto scratch_buffer =
        create_buffer(build_sizes_info.buildScratchSize,
                      vk::BufferUsageFlagBits::eShaderDeviceAddress |
                          vk::BufferUsageFlagBits::eStorageBuffer);

    const auto scratch_buffer_address =
        get_device_address(scratch_buffer.get());

    const auto command_buffer = begin_one_time_submit_command_buffer();

    const auto acceleration_structure_buffer = create_buffer(
        build_sizes_info.accelerationStructureSize,
        vk::BufferUsageFlagBits::eShaderDeviceAddress |
            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);

    const vk::AccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .buffer = acceleration_structure_buffer.get(),
            .size = build_sizes_info.accelerationStructureSize,
            .type = vk::AccelerationStructureTypeKHR::eBottomLevel};

    m_blas = m_device->createAccelerationStructureKHRUnique(
        acceleration_structure_create_info);

    build_geometry_info.dstAccelerationStructure = m_blas.get();
    build_geometry_info.scratchData.deviceAddress = scratch_buffer_address;

    command_buffer.buildAccelerationStructuresKHR(build_geometry_info,
                                                  &build_range_info);

    end_one_time_submit_command_buffer(command_buffer);
}

void Vulkan_renderer::create_tlas()
{
    vk::TransformMatrixKHR transform {};
    transform.matrix[0][0] = 1.0f;
    transform.matrix[1][1] = 1.0f;
    transform.matrix[2][2] = 1.0f;

    const vk::AccelerationStructureInstanceKHR tlas {
        .transform = transform,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = get_device_address(m_blas.get())};

    VmaAllocationInfo staging_buffer_allocation_info {};
    const auto staging_buffer =
        create_buffer_mapped(sizeof(vk::AccelerationStructureInstanceKHR),
                             vk::BufferUsageFlagBits::eTransferSrc,
                             &staging_buffer_allocation_info);

    std::memcpy(staging_buffer_allocation_info.pMappedData,
                &tlas,
                sizeof(vk::AccelerationStructureInstanceKHR));

    const auto instance_buffer =
        create_buffer(sizeof(vk::AccelerationStructureInstanceKHR),
                      vk::BufferUsageFlagBits::eShaderDeviceAddress |
                          vk::BufferUsageFlagBits::
                              eAccelerationStructureBuildInputReadOnlyKHR |
                          vk::BufferUsageFlagBits::eTransferDst);

    const auto command_buffer = begin_one_time_submit_command_buffer();

    constexpr vk::BufferCopy region {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = sizeof(vk::AccelerationStructureInstanceKHR)};

    command_buffer.copyBuffer(
        staging_buffer.get(), instance_buffer.get(), region);

    constexpr vk::MemoryBarrier barrier {
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR};

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
        {},
        barrier,
        {},
        {});

    const vk::AccelerationStructureGeometryInstancesDataKHR
        geometry_instances_data {.data = {.deviceAddress = get_device_address(
                                              instance_buffer.get())}};

    const vk::AccelerationStructureGeometryKHR geometry {
        .geometryType = vk::GeometryTypeKHR::eInstances,
        .geometry = {.instances = geometry_instances_data}};

    vk::AccelerationStructureBuildGeometryInfoKHR build_geometry_info {
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        .flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &geometry};

    const auto build_sizes_info =
        m_device->getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice,
            build_geometry_info,
            1);

    const auto acceleration_structure_buffer = create_buffer(
        build_sizes_info.accelerationStructureSize,
        vk::BufferUsageFlagBits::eShaderDeviceAddress |
            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);

    const vk::AccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .buffer = acceleration_structure_buffer.get(),
            .size = build_sizes_info.accelerationStructureSize,
            .type = vk::AccelerationStructureTypeKHR::eTopLevel};

    m_tlas = m_device->createAccelerationStructureKHRUnique(
        acceleration_structure_create_info);

    const auto scratch_buffer =
        create_buffer(build_sizes_info.buildScratchSize,
                      vk::BufferUsageFlagBits::eShaderDeviceAddress |
                          vk::BufferUsageFlagBits::eStorageBuffer);

    const auto scratch_buffer_address =
        get_device_address(scratch_buffer.get());

    build_geometry_info.dstAccelerationStructure = m_tlas.get();
    build_geometry_info.scratchData.deviceAddress = scratch_buffer_address;

    const vk::AccelerationStructureBuildRangeInfoKHR build_range_info {
        .primitiveCount = 1,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0};

    command_buffer.buildAccelerationStructuresKHR(build_geometry_info,
                                                  &build_range_info);

    end_one_time_submit_command_buffer(command_buffer);
}

void Vulkan_renderer::create_descriptor_set_layout()
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
         .stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR},
        {.binding = 3,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR},
        {.binding = 4,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR}};

    const vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info {
        .bindingCount = static_cast<std::uint32_t>(
            std::size(descriptor_set_layout_bindings)),
        .pBindings = descriptor_set_layout_bindings};

    m_descriptor_set_layout = m_device->createDescriptorSetLayoutUnique(
        descriptor_set_layout_create_info);
}

void Vulkan_renderer::create_descriptor_pool()
{
    constexpr vk::DescriptorPoolSize pool_sizes[] {
        {vk::DescriptorType::eStorageImage, 1},
        {vk::DescriptorType::eStorageBuffer, 3},
        {vk::DescriptorType::eAccelerationStructureKHR, 1}};

    const vk::DescriptorPoolCreateInfo create_info {
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 5, // FIXME
        .poolSizeCount = static_cast<std::uint32_t>(std::size(pool_sizes)),
        .pPoolSizes = pool_sizes};

    m_descriptor_pool = m_device->createDescriptorPoolUnique(create_info);
}

void Vulkan_renderer::create_descriptor_set()
{
    const vk::DescriptorSetAllocateInfo descriptor_set_allocate_info {
        .descriptorPool = m_descriptor_pool.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptor_set_layout.get()};

    const auto sets =
        m_device->allocateDescriptorSets(descriptor_set_allocate_info);
    m_descriptor_set = sets.front();

    const vk::DescriptorImageInfo descriptor_storage_image {
        .sampler = VK_NULL_HANDLE,
        .imageView = m_storage_image_view.get(),
        .imageLayout = vk::ImageLayout::eGeneral};

    const vk::WriteDescriptorSetAccelerationStructureKHR
        descriptor_acceleration_structure {.accelerationStructureCount = 1,
                                           .pAccelerationStructures =
                                               &m_tlas.get()};

    const vk::DescriptorBufferInfo descriptor_vertex_buffer {
        .buffer = m_vertex_buffer.get(),
        .offset = 0,
        .range = m_vertex_buffer_size};

    const vk::DescriptorBufferInfo descriptor_normals_buffer {
        .buffer = m_normals_buffer.get(),
        .offset = 0,
        .range = m_normals_buffer_size};

    const vk::DescriptorBufferInfo descriptor_index_buffer {
        .buffer = m_index_buffer.get(),
        .offset = 0,
        .range = m_index_buffer_size};

    const vk::WriteDescriptorSet descriptor_writes[] {
        {.dstSet = m_descriptor_set,
         .dstBinding = 0,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &descriptor_storage_image},
        {.pNext = &descriptor_acceleration_structure,
         .dstSet = m_descriptor_set,
         .dstBinding = 1,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eAccelerationStructureKHR},
        {.dstSet = m_descriptor_set,
         .dstBinding = 2,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &descriptor_vertex_buffer},
        {.dstSet = m_descriptor_set,
         .dstBinding = 3,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &descriptor_index_buffer},
        {.dstSet = m_descriptor_set,
         .dstBinding = 4,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &descriptor_normals_buffer}};

    m_device->updateDescriptorSets(descriptor_writes, {});
}

vk::UniqueShaderModule
Vulkan_renderer::create_shader_module(const char *file_name)
{
    const auto shader_code = read_binary_file(file_name);

    const vk::ShaderModuleCreateInfo shader_module_create_info {
        .codeSize = shader_code.size() * sizeof(std::uint32_t),
        .pCode = shader_code.data()};

    return m_device->createShaderModuleUnique(shader_module_create_info);
}

void Vulkan_renderer::create_ray_tracing_pipeline()
{
    const auto rgen_shader_module = create_shader_module("ray_trace.rgen.spv");
    const auto rmiss_shader_module =
        create_shader_module("ray_trace.rmiss.spv");
    const auto rchit_shader_module =
        create_shader_module("ray_trace.rchit.spv");

    const vk::PipelineShaderStageCreateInfo shader_stage_create_infos[] {
        {.stage = vk::ShaderStageFlagBits::eRaygenKHR,
         .module = rgen_shader_module.get(),
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eMissKHR,
         .module = rmiss_shader_module.get(),
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eClosestHitKHR,
         .module = rchit_shader_module.get(),
         .pName = "main"}};

    vk::RayTracingShaderGroupCreateInfoKHR group {
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR};

    group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group.generalShader = 0;
    m_ray_tracing_shader_groups.push_back(group);

    group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group.generalShader = 1;
    m_ray_tracing_shader_groups.push_back(group);

    group.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = 2;
    m_ray_tracing_shader_groups.push_back(group);

    constexpr vk::PushConstantRange push_constant_range {
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR |
                      vk::ShaderStageFlagBits::eClosestHitKHR,
        .offset = 0,
        .size = sizeof(Push_constants)};

    const vk::PipelineLayoutCreateInfo pipeline_layout_create_info {
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptor_set_layout.get(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range};

    m_ray_tracing_pipeline_layout =
        m_device->createPipelineLayoutUnique(pipeline_layout_create_info);

    const vk::RayTracingPipelineCreateInfoKHR ray_tracing_pipeline_create_info {
        .stageCount =
            static_cast<std::uint32_t>(std::size(shader_stage_create_infos)),
        .pStages = shader_stage_create_infos,
        .groupCount =
            static_cast<std::uint32_t>(m_ray_tracing_shader_groups.size()),
        .pGroups = m_ray_tracing_shader_groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = m_ray_tracing_pipeline_layout.get()};

    auto ray_tracing_pipeline = m_device->createRayTracingPipelineKHRUnique(
        VK_NULL_HANDLE, VK_NULL_HANDLE, ray_tracing_pipeline_create_info);
    vk::resultCheck(ray_tracing_pipeline.result,
                    "vk::Device::createRayTracingPipelineKHRUnique");
    m_ray_tracing_pipeline = std::move(ray_tracing_pipeline.value);
}

void Vulkan_renderer::create_shader_binding_table()
{
    constexpr std::uint32_t miss_count {1};
    constexpr std::uint32_t hit_count {1};
    constexpr std::uint32_t handle_count {1 + miss_count + hit_count};
    const std::uint32_t handle_size {
        m_ray_tracing_properties.shaderGroupHandleSize};

    constexpr auto align_up = [](std::uint32_t size, std::uint32_t alignment)
    { return (size + (alignment - 1)) & ~(alignment - 1); };

    const std::uint32_t handle_size_aligned = align_up(
        handle_size, m_ray_tracing_properties.shaderGroupHandleAlignment);

    m_rgen_region.stride = align_up(
        handle_size_aligned, m_ray_tracing_properties.shaderGroupBaseAlignment);
    m_rgen_region.size = m_rgen_region.stride;

    m_miss_region.stride = handle_size_aligned;
    m_miss_region.size =
        align_up(miss_count * handle_size_aligned,
                 m_ray_tracing_properties.shaderGroupBaseAlignment);

    m_hit_region.stride = handle_size_aligned;
    m_hit_region.size =
        align_up(hit_count * handle_size_aligned,
                 m_ray_tracing_properties.shaderGroupBaseAlignment);

    const auto data_size = handle_count * handle_size;
    const auto handles =
        m_device->getRayTracingShaderGroupHandlesKHR<std::uint8_t>(
            m_ray_tracing_pipeline.get(), 0, handle_count, data_size);

    const auto sbt_size = m_rgen_region.size + m_miss_region.size +
                          m_hit_region.size + m_call_region.size;

    VmaAllocationInfo sbt_allocation_info {};
    m_sbt_buffer = create_buffer_mapped(
        sbt_size,
        vk::BufferUsageFlagBits::eTransferSrc |
            vk::BufferUsageFlagBits::eShaderDeviceAddress |
            vk::BufferUsageFlagBits::eShaderBindingTableKHR,
        &sbt_allocation_info);

    const auto sbt_address = get_device_address(m_sbt_buffer.get());
    m_rgen_region.deviceAddress = sbt_address;
    m_miss_region.deviceAddress = sbt_address + m_rgen_region.size;
    m_hit_region.deviceAddress =
        sbt_address + m_rgen_region.size + m_miss_region.size;

    const auto get_handle_pointer = [&](std::uint32_t i)
    { return handles.data() + i * handle_size; };

    const auto sbt_buffer_mapped =
        static_cast<std::uint8_t *>(sbt_allocation_info.pMappedData);
    std::uint32_t handle_index {};
    std::memcpy(
        sbt_buffer_mapped, get_handle_pointer(handle_index), handle_size);
    ++handle_index;

    auto p_data = sbt_buffer_mapped + m_rgen_region.size;
    for (std::uint32_t i {}; i < miss_count; ++i)
    {
        std::memcpy(p_data, get_handle_pointer(handle_index), handle_size);
        ++handle_index;
        p_data += m_miss_region.stride;
    }

    p_data = sbt_buffer_mapped + m_rgen_region.size + m_miss_region.size;
    for (std::uint32_t i {}; i < hit_count; ++i)
    {
        std::memcpy(p_data, get_handle_pointer(handle_index), handle_size);
        ++handle_index;
        p_data += m_hit_region.stride;
    }
}
