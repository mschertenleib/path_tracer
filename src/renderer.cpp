#include "renderer.hpp"

#include "geometry.hpp"

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
static_assert(sizeof(Push_constants) <= 128);

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

// alignment MUST be a power of 2
template <std::unsigned_integral U>
[[nodiscard]] constexpr U align_up(U value, U alignment) noexcept
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}

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
        align_up(file_size, sizeof(std::uint32_t)) / sizeof(std::uint32_t);

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

[[nodiscard]] std::uint32_t
get_queue_family_index(vk::PhysicalDevice physical_device)
{
    const auto properties = physical_device.getQueueFamilyProperties();

    for (std::uint32_t i {}; i < static_cast<std::uint32_t>(properties.size());
         ++i)
    {
        if (properties[i].queueFlags & vk::QueueFlagBits::eCompute)
        {
            return i;
        }
    }

    return std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] vk::DeviceAddress get_device_address(vk::Device device,
                                                   vk::Buffer buffer)
{
    const vk::BufferDeviceAddressInfo address_info {.buffer = buffer};
    return device.getBufferAddress(address_info);
}

[[nodiscard]] vk::DeviceAddress
get_device_address(vk::Device device,
                   vk::AccelerationStructureKHR acceleration_structure)
{
    const vk::AccelerationStructureDeviceAddressInfoKHR address_info {
        .accelerationStructure = acceleration_structure};
    return device.getAccelerationStructureAddressKHR(address_info);
}

void select_physical_device(Renderer &r,
                            std::uint32_t device_extension_count,
                            const char *const *device_extension_names)
{
    const auto physical_devices = r.instance->enumeratePhysicalDevices();

    for (const auto physical_device : physical_devices)
    {
        r.queue_family_index = get_queue_family_index(physical_device);
        if (r.queue_family_index == std::numeric_limits<std::uint32_t>::max())
        {
            continue;
        }

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

        const auto features = physical_device.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan12Features,
            vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
            vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();
        if (!features.get<vk::PhysicalDeviceVulkan12Features>()
                 .bufferDeviceAddress ||
            !features.get<vk::PhysicalDeviceVulkan12Features>()
                 .scalarBlockLayout ||
            !features.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>()
                 .accelerationStructure ||
            !features.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>()
                 .rayTracingPipeline)
        {
            continue;
        }

        const auto format_properties = physical_device.getFormatProperties(
            vk::Format::eR32G32B32A32Sfloat);
        if (!(format_properties.optimalTilingFeatures &
              (vk::FormatFeatureFlagBits::eStorageImage |
               vk::FormatFeatureFlagBits::eTransferSrc)))
        {
            continue;
        }

        r.physical_device = physical_device;
        const auto properties_chain = r.physical_device.getProperties2<
            vk::PhysicalDeviceProperties2,
            vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
        r.physical_device_properties =
            properties_chain.get<vk::PhysicalDeviceProperties2>().properties;
        r.ray_tracing_pipeline_properties =
            properties_chain
                .get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
        return;
    }

    throw std::runtime_error("Failed to find a suitable physical device");
}

void create_device(Renderer &r,
                   std::uint32_t device_extension_count,
                   const char *const *device_extension_names)
{
    constexpr float queue_priority {1.0f};

    const vk::DeviceQueueCreateInfo queue_create_info {
        .queueFamilyIndex = r.queue_family_index,
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

    r.device = r.physical_device.createDeviceUnique(device_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(r.device.get());
}

void create_command_pool(Renderer &r)
{
    const vk::CommandPoolCreateInfo create_info {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = r.queue_family_index};

    r.command_pool = r.device->createCommandPoolUnique(create_info);
}

[[nodiscard]] vk::CommandBuffer
begin_one_time_submit_command_buffer(const Renderer &r)
{
    const vk::CommandBufferAllocateInfo allocate_info {
        .commandPool = r.command_pool.get(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1};

    auto command_buffer =
        r.device->allocateCommandBuffers(allocate_info).front();

    constexpr vk::CommandBufferBeginInfo begin_info {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};

    command_buffer.begin(begin_info);

    return command_buffer;
}

void end_one_time_submit_command_buffer(const Renderer &r,
                                        vk::CommandBuffer command_buffer)
{
    command_buffer.end();

    const vk::SubmitInfo submit_info {.commandBufferCount = 1,
                                      .pCommandBuffers = &command_buffer};

    r.queue.submit(submit_info);
    r.queue.waitIdle();
}

void create_storage_image(Renderer &r,
                          std::uint32_t width,
                          std::uint32_t height)
{
    r.render_width = width;
    r.render_height = height;

    constexpr auto format = vk::Format::eR32G32B32A32Sfloat;

    const vk::ImageCreateInfo image_create_info {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {r.render_width, r.render_height, 1},
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

    r.storage_image = create_image(
        r.allocator.get(), image_create_info, allocation_create_info);

    constexpr vk::ImageSubresourceRange subresource_range {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    const vk::ImageViewCreateInfo image_view_create_info {
        .image = r.storage_image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = subresource_range};

    r.storage_image_view =
        r.device->createImageViewUnique(image_view_create_info);

    const auto command_buffer = begin_one_time_submit_command_buffer(r);

    const vk::ImageMemoryBarrier image_memory_barrier {
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask =
            vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eGeneral,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = r.storage_image.get(),
        .subresourceRange = subresource_range};

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eRayTracingShaderKHR,
        {},
        {},
        {},
        image_memory_barrier);

    end_one_time_submit_command_buffer(r, command_buffer);
}

void create_geometry_buffer(Renderer &r,
                            const std::vector<float> &vertices,
                            const std::vector<std::uint32_t> &indices,
                            const std::vector<float> &normals)
{
    const auto offset_alignemnt =
        r.physical_device_properties.limits.minStorageBufferOffsetAlignment;

    r.vertex_count = static_cast<std::uint32_t>(vertices.size() / 3);
    r.index_count = static_cast<std::uint32_t>(indices.size());
    r.vertex_range_size = vertices.size() * sizeof(float);
    r.index_range_offset = align_up(r.vertex_range_size, offset_alignemnt);
    r.index_range_size = indices.size() * sizeof(float);
    r.normal_range_offset =
        align_up(r.index_range_offset + r.index_range_size, offset_alignemnt);
    r.normal_range_size = normals.size() * sizeof(float);

    const auto geometry_buffer_size =
        r.normal_range_offset + r.normal_range_size;

    const vk::BufferCreateInfo staging_buffer_create_info {
        .size = geometry_buffer_size,
        .usage = vk::BufferUsageFlagBits::eTransferSrc};

    VmaAllocationCreateInfo staging_allocation_create_info {};
    staging_allocation_create_info.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
    staging_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    VmaAllocationInfo staging_allocation_info {};

    const auto staging_buffer = create_buffer(r.allocator.get(),
                                              staging_buffer_create_info,
                                              staging_allocation_create_info,
                                              &staging_allocation_info);
    auto *const mapped_data =
        static_cast<std::uint8_t *>(staging_allocation_info.pMappedData);

    std::memcpy(mapped_data, vertices.data(), r.vertex_range_size);
    std::memcpy(
        mapped_data + r.index_range_offset, indices.data(), r.index_range_size);
    std::memcpy(mapped_data + r.normal_range_offset,
                normals.data(),
                r.normal_range_size);

    const vk::BufferCreateInfo buffer_create_info {
        .size = geometry_buffer_size,
        .usage = vk::BufferUsageFlagBits::eStorageBuffer |
                 vk::BufferUsageFlagBits::eTransferDst |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::
                     eAccelerationStructureBuildInputReadOnlyKHR};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    r.geometry_buffer = create_buffer(
        r.allocator.get(), buffer_create_info, allocation_create_info);

    const auto command_buffer = begin_one_time_submit_command_buffer(r);

    const vk::BufferCopy region {
        .srcOffset = 0, .dstOffset = 0, .size = geometry_buffer_size};

    command_buffer.copyBuffer(
        staging_buffer.get(), r.geometry_buffer.get(), region);

    end_one_time_submit_command_buffer(r, command_buffer);
}

void create_blas(Renderer &r)
{
    const auto geometry_buffer_address =
        get_device_address(r.device.get(), r.geometry_buffer.get());

    const vk::AccelerationStructureGeometryTrianglesDataKHR triangles {
        .vertexFormat = vk::Format::eR32G32B32Sfloat,
        .vertexData = {.deviceAddress = geometry_buffer_address},
        .vertexStride = 3 * sizeof(float),
        .maxVertex = r.vertex_count - 1,
        .indexType = vk::IndexType::eUint32,
        .indexData = {.deviceAddress =
                          geometry_buffer_address + r.index_range_offset}};

    const vk::AccelerationStructureGeometryKHR geometry {
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry = {.triangles = triangles},
        .flags = vk::GeometryFlagBitsKHR::eOpaque};

    const vk::AccelerationStructureBuildRangeInfoKHR build_range_info {
        .primitiveCount = r.index_count / 3,
        .primitiveOffset = 0,
        .firstVertex = 0};

    vk::AccelerationStructureBuildGeometryInfoKHR build_geometry_info {
        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        .flags = vk::BuildAccelerationStructureFlagBitsKHR ::ePreferFastTrace,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &geometry};

    const auto build_sizes_info =
        r.device->getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice,
            build_geometry_info,
            build_range_info.primitiveCount);

    const vk::BufferCreateInfo scratch_buffer_create_info {
        .size = build_sizes_info.buildScratchSize,
        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::eStorageBuffer};

    VmaAllocationCreateInfo scratch_allocation_create_info {};
    scratch_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    const auto scratch_buffer = create_buffer(r.allocator.get(),
                                              scratch_buffer_create_info,
                                              scratch_allocation_create_info);

    const auto scratch_buffer_address =
        get_device_address(r.device.get(), scratch_buffer.get());

    const auto command_buffer = begin_one_time_submit_command_buffer(r);

    const vk::BufferCreateInfo blas_buffer_create_info {
        .size = build_sizes_info.accelerationStructureSize,
        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR};

    VmaAllocationCreateInfo blas_allocation_create_info {};
    blas_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    r.blas_buffer = create_buffer(r.allocator.get(),
                                  blas_buffer_create_info,
                                  blas_allocation_create_info);

    const vk::AccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .buffer = r.blas_buffer.get(),
            .size = build_sizes_info.accelerationStructureSize,
            .type = vk::AccelerationStructureTypeKHR::eBottomLevel};

    r.blas = r.device->createAccelerationStructureKHRUnique(
        acceleration_structure_create_info);

    build_geometry_info.dstAccelerationStructure = r.blas.get();
    build_geometry_info.scratchData.deviceAddress = scratch_buffer_address;

    command_buffer.buildAccelerationStructuresKHR(build_geometry_info,
                                                  &build_range_info);

    end_one_time_submit_command_buffer(r, command_buffer);
}

void create_tlas(Renderer &r)
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
        .accelerationStructureReference =
            get_device_address(r.device.get(), r.blas.get())};

    const vk::BufferCreateInfo staging_buffer_create_info {
        .size = sizeof(vk::AccelerationStructureInstanceKHR),
        .usage = vk::BufferUsageFlagBits::eTransferSrc};

    VmaAllocationCreateInfo staging_allocation_create_info {};
    staging_allocation_create_info.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
    staging_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    VmaAllocationInfo staging_allocation_info {};

    const auto staging_buffer = create_buffer(r.allocator.get(),
                                              staging_buffer_create_info,
                                              staging_allocation_create_info,
                                              &staging_allocation_info);
    auto *const mapped_data =
        static_cast<std::uint8_t *>(staging_allocation_info.pMappedData);

    std::memcpy(
        mapped_data, &tlas, sizeof(vk::AccelerationStructureInstanceKHR));

    const vk::BufferCreateInfo instance_buffer_create_info {
        .size = sizeof(vk::AccelerationStructureInstanceKHR),
        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::
                     eAccelerationStructureBuildInputReadOnlyKHR |
                 vk::BufferUsageFlagBits::eTransferDst};

    VmaAllocationCreateInfo instance_allocation_create_info {};
    instance_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    const auto instance_buffer = create_buffer(r.allocator.get(),
                                               instance_buffer_create_info,
                                               instance_allocation_create_info);

    const auto command_buffer = begin_one_time_submit_command_buffer(r);

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
        geometry_instances_data {
            .data = {.deviceAddress = get_device_address(
                         r.device.get(), instance_buffer.get())}};

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
        r.device->getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice,
            build_geometry_info,
            1);

    const vk::BufferCreateInfo tlas_buffer_create_info {
        .size = build_sizes_info.accelerationStructureSize,
        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR};

    VmaAllocationCreateInfo tlas_allocation_create_info {};
    tlas_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    r.tlas_buffer = create_buffer(r.allocator.get(),
                                  tlas_buffer_create_info,
                                  tlas_allocation_create_info);

    const vk::AccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .buffer = r.tlas_buffer.get(),
            .size = build_sizes_info.accelerationStructureSize,
            .type = vk::AccelerationStructureTypeKHR::eTopLevel};

    r.tlas = r.device->createAccelerationStructureKHRUnique(
        acceleration_structure_create_info);

    const vk::BufferCreateInfo scratch_buffer_create_info {
        .size = build_sizes_info.buildScratchSize,
        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::eStorageBuffer};

    VmaAllocationCreateInfo scratch_allocation_create_info {};
    scratch_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    const auto scratch_buffer = create_buffer(r.allocator.get(),
                                              scratch_buffer_create_info,
                                              scratch_allocation_create_info);

    const auto scratch_buffer_address =
        get_device_address(r.device.get(), scratch_buffer.get());

    build_geometry_info.dstAccelerationStructure = r.tlas.get();
    build_geometry_info.scratchData.deviceAddress = scratch_buffer_address;

    const vk::AccelerationStructureBuildRangeInfoKHR build_range_info {
        .primitiveCount = 1,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0};

    command_buffer.buildAccelerationStructuresKHR(build_geometry_info,
                                                  &build_range_info);

    end_one_time_submit_command_buffer(r, command_buffer);
}

void create_descriptor_set_layout(Renderer &r)
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

    r.descriptor_set_layout = r.device->createDescriptorSetLayoutUnique(
        descriptor_set_layout_create_info);
}

void create_descriptor_pool(Renderer &r)
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

    r.descriptor_pool = r.device->createDescriptorPoolUnique(create_info);
}

void create_descriptor_set(Renderer &r)
{
    const vk::DescriptorSetAllocateInfo descriptor_set_allocate_info {
        .descriptorPool = r.descriptor_pool.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = &r.descriptor_set_layout.get()};

    const auto sets =
        r.device->allocateDescriptorSets(descriptor_set_allocate_info);
    r.descriptor_set = sets.front();

    const vk::DescriptorImageInfo descriptor_storage_image {
        .sampler = VK_NULL_HANDLE,
        .imageView = r.storage_image_view.get(),
        .imageLayout = vk::ImageLayout::eGeneral};

    const vk::WriteDescriptorSetAccelerationStructureKHR
        descriptor_acceleration_structure {.accelerationStructureCount = 1,
                                           .pAccelerationStructures =
                                               &r.tlas.get()};

    const vk::DescriptorBufferInfo descriptor_vertices {
        .buffer = r.geometry_buffer.get(),
        .offset = 0,
        .range = r.vertex_range_size};

    const vk::DescriptorBufferInfo descriptor_indices {
        .buffer = r.geometry_buffer.get(),
        .offset = r.index_range_offset,
        .range = r.index_range_size};

    const vk::DescriptorBufferInfo descriptor_normals {
        .buffer = r.geometry_buffer.get(),
        .offset = r.normal_range_offset,
        .range = r.normal_range_size};

    const vk::WriteDescriptorSet descriptor_writes[5] {
        {.dstSet = r.descriptor_set,
         .dstBinding = 0,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &descriptor_storage_image},
        {.pNext = &descriptor_acceleration_structure,
         .dstSet = r.descriptor_set,
         .dstBinding = 1,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eAccelerationStructureKHR},
        {.dstSet = r.descriptor_set,
         .dstBinding = 2,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &descriptor_vertices},
        {.dstSet = r.descriptor_set,
         .dstBinding = 3,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &descriptor_indices},
        {.dstSet = r.descriptor_set,
         .dstBinding = 4,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &descriptor_normals}};

    r.device->updateDescriptorSets(descriptor_writes, {});
}

vk::UniqueShaderModule create_shader_module(const Renderer &r,
                                            const char *file_name)
{
    const auto shader_code = read_binary_file(file_name);

    const vk::ShaderModuleCreateInfo shader_module_create_info {
        .codeSize = shader_code.size() * sizeof(std::uint32_t),
        .pCode = shader_code.data()};

    return r.device->createShaderModuleUnique(shader_module_create_info);
}

void create_ray_tracing_pipeline(Renderer &r)
{
    const auto rgen_shader_module = create_shader_module(r, "shader.rgen.spv");
    const auto rmiss_shader_module =
        create_shader_module(r, "shader.rmiss.spv");
    const auto rchit_shader_module =
        create_shader_module(r, "shader.rchit.spv");

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
    r.ray_tracing_shader_groups.push_back(group);

    group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group.generalShader = 1;
    r.ray_tracing_shader_groups.push_back(group);

    group.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = 2;
    r.ray_tracing_shader_groups.push_back(group);

    constexpr vk::PushConstantRange push_constant_range {
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR |
                      vk::ShaderStageFlagBits::eClosestHitKHR,
        .offset = 0,
        .size = sizeof(Push_constants)};

    const vk::PipelineLayoutCreateInfo pipeline_layout_create_info {
        .setLayoutCount = 1,
        .pSetLayouts = &r.descriptor_set_layout.get(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range};

    r.ray_tracing_pipeline_layout =
        r.device->createPipelineLayoutUnique(pipeline_layout_create_info);

    const vk::RayTracingPipelineCreateInfoKHR ray_tracing_pipeline_create_info {
        .stageCount =
            static_cast<std::uint32_t>(std::size(shader_stage_create_infos)),
        .pStages = shader_stage_create_infos,
        .groupCount =
            static_cast<std::uint32_t>(r.ray_tracing_shader_groups.size()),
        .pGroups = r.ray_tracing_shader_groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = r.ray_tracing_pipeline_layout.get()};

    auto ray_tracing_pipeline = r.device->createRayTracingPipelineKHRUnique(
        VK_NULL_HANDLE, VK_NULL_HANDLE, ray_tracing_pipeline_create_info);
    vk::resultCheck(ray_tracing_pipeline.result,
                    "vk::Device::createRayTracingPipelineKHRUnique");
    r.ray_tracing_pipeline = std::move(ray_tracing_pipeline.value);
}

void create_shader_binding_table(Renderer &r)
{
    constexpr std::uint32_t miss_count {1};
    constexpr std::uint32_t hit_count {1};
    constexpr std::uint32_t handle_count {1 + miss_count + hit_count};
    const std::uint32_t handle_size {
        r.ray_tracing_pipeline_properties.shaderGroupHandleSize};

    const std::uint32_t handle_size_aligned =
        align_up(handle_size,
                 r.ray_tracing_pipeline_properties.shaderGroupHandleAlignment);

    r.rgen_region.stride =
        align_up(handle_size_aligned,
                 r.ray_tracing_pipeline_properties.shaderGroupBaseAlignment);
    r.rgen_region.size = r.rgen_region.stride;

    r.miss_region.stride = handle_size_aligned;
    r.miss_region.size =
        align_up(miss_count * handle_size_aligned,
                 r.ray_tracing_pipeline_properties.shaderGroupBaseAlignment);

    r.hit_region.stride = handle_size_aligned;
    r.hit_region.size =
        align_up(hit_count * handle_size_aligned,
                 r.ray_tracing_pipeline_properties.shaderGroupBaseAlignment);

    const auto data_size = handle_count * handle_size;
    const auto handles =
        r.device->getRayTracingShaderGroupHandlesKHR<std::uint8_t>(
            r.ray_tracing_pipeline.get(), 0, handle_count, data_size);

    const auto sbt_size = r.rgen_region.size + r.miss_region.size +
                          r.hit_region.size + r.call_region.size;

    const vk::BufferCreateInfo sbt_buffer_create_info {
        .size = sbt_size,
        .usage = vk::BufferUsageFlagBits::eTransferSrc |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::eShaderBindingTableKHR};

    VmaAllocationCreateInfo sbt_allocation_create_info {};
    sbt_allocation_create_info.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
    sbt_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    VmaAllocationInfo sbt_allocation_info {};

    r.sbt_buffer = create_buffer(r.allocator.get(),
                                 sbt_buffer_create_info,
                                 sbt_allocation_create_info,
                                 &sbt_allocation_info);
    auto *const sbt_buffer_mapped =
        static_cast<std::uint8_t *>(sbt_allocation_info.pMappedData);

    const auto sbt_address =
        get_device_address(r.device.get(), r.sbt_buffer.get());
    r.rgen_region.deviceAddress = sbt_address;
    r.miss_region.deviceAddress = sbt_address + r.rgen_region.size;
    r.hit_region.deviceAddress =
        sbt_address + r.rgen_region.size + r.miss_region.size;

    const auto get_handle_pointer = [&](std::uint32_t i)
    { return handles.data() + i * handle_size; };

    std::uint32_t handle_index {};
    std::memcpy(
        sbt_buffer_mapped, get_handle_pointer(handle_index), handle_size);
    ++handle_index;

    auto p_data = sbt_buffer_mapped + r.rgen_region.size;
    for (std::uint32_t i {}; i < miss_count; ++i)
    {
        std::memcpy(p_data, get_handle_pointer(handle_index), handle_size);
        ++handle_index;
        p_data += r.miss_region.stride;
    }

    p_data = sbt_buffer_mapped + r.rgen_region.size + r.miss_region.size;
    for (std::uint32_t i {}; i < hit_count; ++i)
    {
        std::memcpy(p_data, get_handle_pointer(handle_index), handle_size);
        ++handle_index;
        p_data += r.hit_region.stride;
    }
}

} // namespace

void Handle_deleter<VmaAllocator>::operator()(
    VmaAllocator allocator) const noexcept
{
    vmaDestroyAllocator(allocator);
}

void Handle_deleter<vk::Buffer>::operator()(vk::Buffer buffer) const noexcept
{
    vmaDestroyBuffer(allocator, buffer, allocation);
}

void Handle_deleter<vk::Image>::operator()(vk::Image image) const noexcept
{
    vmaDestroyImage(allocator, image, allocation);
}

Handle<VmaAllocator> create_allocator(const VmaAllocatorCreateInfo &create_info)
{
    VmaAllocator allocator {};
    const auto result = vmaCreateAllocator(&create_info, &allocator);
    vk::resultCheck(static_cast<vk::Result>(result), "vmaCreateAllocator");
    return Handle<VmaAllocator>(allocator);
}

Handle<vk::Buffer>
create_buffer(VmaAllocator allocator,
              const vk::BufferCreateInfo &buffer_create_info,
              const VmaAllocationCreateInfo &allocation_create_info,
              VmaAllocationInfo *allocation_info)
{
    VkBuffer buffer {};
    VmaAllocation allocation {};
    const auto result = vmaCreateBuffer(
        allocator,
        reinterpret_cast<const VkBufferCreateInfo *>(&buffer_create_info),
        &allocation_create_info,
        &buffer,
        &allocation,
        allocation_info);
    vk::resultCheck(static_cast<vk::Result>(result), "vmaCreateBuffer");
    return {static_cast<vk::Buffer>(buffer),
            Handle_deleter<vk::Buffer> {allocator, allocation}};
}

Handle<vk::Image>
create_image(VmaAllocator allocator,
             const vk::ImageCreateInfo &image_create_info,
             const VmaAllocationCreateInfo &allocation_create_info,
             VmaAllocationInfo *allocation_info)
{
    VkImage image {};
    VmaAllocation allocation {};
    const auto result = vmaCreateImage(
        allocator,
        reinterpret_cast<const VkImageCreateInfo *>(&image_create_info),
        &allocation_create_info,
        &image,
        &allocation,
        allocation_info);
    vk::resultCheck(static_cast<vk::Result>(result), "vmaCreateImage");
    return {static_cast<vk::Image>(image),
            Handle_deleter<vk::Image> {allocator, allocation}};
}

Renderer create_renderer()
{
    Renderer r {};

    VULKAN_HPP_DEFAULT_DISPATCHER.init(
        r.dl.getProcAddress<PFN_vkGetInstanceProcAddr>(
            "vkGetInstanceProcAddr"));

    constexpr vk::ApplicationInfo application_info {.apiVersion =
                                                        VK_API_VERSION_1_3};

#ifndef NDEBUG

    const auto layer_properties {vk::enumerateInstanceLayerProperties()};

    constexpr auto khronos_validation_layer {"VK_LAYER_KHRONOS_validation"};

    if (std::none_of(layer_properties.begin(),
                     layer_properties.end(),
                     [](const vk::LayerProperties &properties) {
                         return std::strcmp(properties.layerName,
                                            khronos_validation_layer) == 0;
                     }))
    {
        throw std::runtime_error(
            "VK_LAYER_KHRONOS_validation is not supported");
    }

    const auto extension_properties {
        vk::enumerateInstanceExtensionProperties()};

    constexpr auto debug_utils_extension {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

    if (std::none_of(extension_properties.begin(),
                     extension_properties.end(),
                     [](const vk::ExtensionProperties &properties) {
                         return std::strcmp(properties.extensionName,
                                            debug_utils_extension) == 0;
                     }))
    {
        throw std::runtime_error(VK_EXT_DEBUG_UTILS_EXTENSION_NAME
                                 " is not supported");
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
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &debug_utils_extension};

    r.instance = vk::createInstanceUnique(instance_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(r.instance.get());

    r.debug_messenger = r.instance->createDebugUtilsMessengerEXTUnique(
        debug_utils_messenger_create_info);

#else

    const vk::InstanceCreateInfo instance_create_info {.pApplicationInfo =
                                                           &application_info};

    r.instance = vk::createInstanceUnique(instance_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(r.instance.get());

#endif

    constexpr const char *device_extension_names[] {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};
    constexpr auto device_extension_count =
        static_cast<std::uint32_t>(std::size(device_extension_names));

    select_physical_device(r, device_extension_count, device_extension_names);
    create_device(r, device_extension_count, device_extension_names);

    VmaVulkanFunctions vulkan_functions {};
    vulkan_functions.vkGetInstanceProcAddr =
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
    vulkan_functions.vkGetDeviceProcAddr =
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_create_info {};
    allocator_create_info.flags =
        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_create_info.physicalDevice = r.physical_device;
    allocator_create_info.device = r.device.get();
    allocator_create_info.pVulkanFunctions = &vulkan_functions;
    allocator_create_info.instance = r.instance.get();
    allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;
    r.allocator = create_allocator(allocator_create_info);

    r.queue = r.device->getQueue(r.queue_family_index, 0);
    create_command_pool(r);

    return r;
}

void load_scene(Renderer &r,
                std::uint32_t render_width,
                std::uint32_t render_height,
                const Geometry &geometry)
{
    create_storage_image(r, render_width, render_height);
    create_geometry_buffer(
        r, geometry.vertices, geometry.indices, geometry.normals);
    create_blas(r);
    create_tlas(r);
    create_descriptor_set_layout(r);
    create_descriptor_pool(r);
    create_descriptor_set(r);
    create_ray_tracing_pipeline(r);
    create_shader_binding_table(r);
}

void render(const Renderer &r)
{
    const auto command_buffer = begin_one_time_submit_command_buffer(r);

    command_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR,
                                r.ray_tracing_pipeline.get());

    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR,
                                      r.ray_tracing_pipeline_layout.get(),
                                      0,
                                      r.descriptor_set,
                                      {});

    const Push_constants push_constants {.rng_seed = 1};

    command_buffer.pushConstants(r.ray_tracing_pipeline_layout.get(),
                                 vk::ShaderStageFlagBits::eRaygenKHR |
                                     vk::ShaderStageFlagBits::eClosestHitKHR,
                                 0,
                                 sizeof(Push_constants),
                                 &push_constants);

    command_buffer.traceRaysKHR(r.rgen_region,
                                r.miss_region,
                                r.hit_region,
                                r.call_region,
                                r.render_width,
                                r.render_height,
                                1);

    end_one_time_submit_command_buffer(r, command_buffer);
}

void write_to_png(const Renderer &r, const char *file_name)
{
    constexpr auto format = vk::Format::eR8G8B8A8Srgb;

    const vk::ImageCreateInfo image_create_info {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {r.render_width, r.render_height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eTransferDst |
                 vk::ImageUsageFlagBits::eTransferSrc,
        .initialLayout = vk::ImageLayout::eUndefined};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    const auto final_image = create_image(
        r.allocator.get(), image_create_info, allocation_create_info);

    const auto staging_buffer_size = r.render_width * r.render_height * 4;

    const vk::BufferCreateInfo staging_buffer_create_info {
        .size = staging_buffer_size,
        .usage = vk::BufferUsageFlagBits::eTransferDst};

    VmaAllocationCreateInfo staging_allocation_create_info {};
    staging_allocation_create_info.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
    staging_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    VmaAllocationInfo staging_allocation_info {};

    const auto staging_buffer = create_buffer(r.allocator.get(),
                                              staging_buffer_create_info,
                                              staging_allocation_create_info,
                                              &staging_allocation_info);
    const auto *const mapped_data =
        static_cast<std::uint8_t *>(staging_allocation_info.pMappedData);

    {
        const auto command_buffer = begin_one_time_submit_command_buffer(r);

        constexpr vk::ImageSubresourceRange subresource_range {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1};

        const vk::ImageMemoryBarrier storage_image_memory_barrier {
            .srcAccessMask = vk::AccessFlagBits::eShaderRead |
                             vk::AccessFlagBits::eShaderWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = r.storage_image.get(),
            .subresourceRange = subresource_range};

        vk::ImageMemoryBarrier final_image_memory_barrier {
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = final_image.get(),
            .subresourceRange = subresource_range};

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe |
                vk::PipelineStageFlagBits::eRayTracingShaderKHR,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            {},
            {},
            {storage_image_memory_barrier, final_image_memory_barrier});

        constexpr vk::ImageSubresourceLayers subresource_layers {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1};

        const std::array offsets {
            vk::Offset3D {0, 0, 0},
            vk::Offset3D {static_cast<std::int32_t>(r.render_width),
                          static_cast<std::int32_t>(r.render_height),
                          1}};

        const vk::ImageBlit image_blit {.srcSubresource = subresource_layers,
                                        .srcOffsets = offsets,
                                        .dstSubresource = subresource_layers,
                                        .dstOffsets = offsets};

        command_buffer.blitImage(r.storage_image.get(),
                                 vk::ImageLayout::eTransferSrcOptimal,
                                 final_image.get(),
                                 vk::ImageLayout::eTransferDstOptimal,
                                 image_blit,
                                 vk::Filter::eNearest);

        final_image_memory_barrier.srcAccessMask =
            vk::AccessFlagBits::eTransferWrite;
        final_image_memory_barrier.dstAccessMask =
            vk::AccessFlagBits::eTransferRead;
        final_image_memory_barrier.oldLayout =
            vk::ImageLayout::eTransferDstOptimal;
        final_image_memory_barrier.newLayout =
            vk::ImageLayout::eTransferSrcOptimal;

        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                       vk::PipelineStageFlagBits::eTransfer,
                                       {},
                                       {},
                                       {},
                                       final_image_memory_barrier);

        const vk::BufferImageCopy copy_region {
            .bufferOffset = 0,
            .bufferImageHeight = r.render_height,
            .imageSubresource = subresource_layers,
            .imageOffset = {0, 0, 0},
            .imageExtent = {r.render_width, r.render_height, 1}};

        command_buffer.copyImageToBuffer(final_image.get(),
                                         vk::ImageLayout::eTransferSrcOptimal,
                                         staging_buffer.get(),
                                         copy_region);

        end_one_time_submit_command_buffer(r, command_buffer);
    }

    const auto write_result =
        stbi_write_png(file_name,
                       static_cast<int>(r.render_width),
                       static_cast<int>(r.render_height),
                       4,
                       mapped_data,
                       static_cast<int>(r.render_width * 4));
    if (write_result == 0)
    {
        std::cout << std::endl;
        throw std::runtime_error("Failed to write PNG image");
    }
}
