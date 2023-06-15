#include "vulkan_renderer.hpp"

#include "tiny_obj_loader.h"

#include "stb_image_write.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include "imgui_impl_vulkan.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "imgui.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cassert>
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

Unique_allocation::Unique_allocation(VmaAllocation allocation,
                                     VmaAllocator allocator) noexcept
    : m_allocation {allocation}, m_allocator {allocator}
{
    assert(m_allocation);
    assert(m_allocator);
}

Unique_allocation::~Unique_allocation() noexcept
{
    if (m_allocation)
    {
        vmaFreeMemory(m_allocator, m_allocation);
    }
}

Unique_allocation::Unique_allocation(Unique_allocation &&rhs) noexcept
    : m_allocation {rhs.m_allocation}, m_allocator {rhs.m_allocator}
{
    rhs.m_allocation = {};
    rhs.m_allocator = {};
}

Unique_allocation &
Unique_allocation::operator=(Unique_allocation &&rhs) noexcept
{
    const auto old_allocation = m_allocation;
    const auto old_allocator = m_allocator;
    m_allocation = rhs.m_allocation;
    m_allocator = rhs.m_allocator;
    rhs.m_allocation = {};
    rhs.m_allocator = {};
    if (old_allocation)
    {
        vmaFreeMemory(old_allocator, old_allocation);
    }
    return *this;
}

Vulkan_renderer::Vulkan_renderer(GLFWwindow *window,
                                 std::uint32_t framebuffer_width,
                                 std::uint32_t framebuffer_height,
                                 std::uint32_t render_width,
                                 std::uint32_t render_height)
    : m_window {window}
{
    constexpr std::uint32_t api_version {VK_API_VERSION_1_3};

    create_instance(api_version);

    create_surface();

    constexpr const char *device_extension_names[] {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME};
    constexpr auto device_extension_count =
        static_cast<std::uint32_t>(std::size(device_extension_names));

    select_physical_device(device_extension_count, device_extension_names);

    create_device(device_extension_count, device_extension_names);

    create_allocator(api_version);

    m_graphics_compute_queue =
        m_device->getQueue(m_queue_family_indices.graphics_compute, 0);

    m_present_queue = m_device->getQueue(m_queue_family_indices.present, 0);

    create_swapchain(framebuffer_width, framebuffer_height);

    create_command_pool();

    create_storage_image(render_width, render_height);

    create_render_target(render_width, render_height);

    create_geometry_buffers();

    create_descriptor_set_layout();

    create_final_render_descriptor_set_layout();

    create_descriptor_pool();

    create_descriptor_set();

    create_final_render_descriptor_set();

    create_pipeline();

    create_render_pass();

    create_framebuffers();

    create_command_buffers();

    create_synchronization_objects();

    init_imgui();

    create_blas();

    create_tlas();
}

Vulkan_renderer::~Vulkan_renderer()
{
    // This could throw, but at this point there is nothing we can do about it,
    // so let the runtime call std::terminate()
    m_device->waitIdle();

    ImGui_ImplVulkan_Shutdown();
}

vk::DescriptorSet Vulkan_renderer::get_final_render_descriptor_set()
{
    return m_final_render_descriptor_set;
}

std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> Vulkan_renderer::get_heap_budgets()
{
    std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets {};
    vmaGetHeapBudgets(m_allocator.get(), budgets.data());
    return budgets;
}

void Vulkan_renderer::draw_frame(std::uint32_t rng_seed)
{
    const auto wait_result =
        m_device->waitForFences(m_in_flight_fences[m_current_frame].get(),
                                VK_TRUE,
                                std::numeric_limits<std::uint64_t>::max());
    vk::resultCheck(wait_result, "vk::Device::waitForFences");

    const auto [image_index_result, image_index] =
        m_device->acquireNextImageKHR(
            m_swapchain.get(),
            std::numeric_limits<std::uint64_t>::max(),
            m_image_available_semaphores[m_current_frame].get(),
            VK_NULL_HANDLE);

    if (image_index_result == vk::Result::eErrorOutOfDateKHR)
    {
        recreate_swapchain();
        return;
    }
    else if (image_index_result != vk::Result::eSuboptimalKHR)
    {
        vk::resultCheck(image_index_result, "vk::Device::acquireNextImageKHR");
    }

    m_device->resetFences(m_in_flight_fences[m_current_frame].get());

    const auto command_buffer = m_command_buffers[m_current_frame];

    command_buffer.reset();

    constexpr vk::CommandBufferBeginInfo command_buffer_begin_info {};

    command_buffer.begin(command_buffer_begin_info);

    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                      m_compute_pipeline_layout.get(),
                                      0,
                                      m_descriptor_set,
                                      {});

    command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute,
                                m_compute_pipeline.get());

    const Push_constants push_constants {.rng_seed = rng_seed};

    command_buffer.pushConstants(m_compute_pipeline_layout.get(),
                                 vk::ShaderStageFlagBits::eCompute,
                                 0,
                                 sizeof(Push_constants),
                                 &push_constants);

    constexpr std::uint32_t group_size_x {32};
    constexpr std::uint32_t group_size_y {32};
    constexpr std::uint32_t group_size_z {1};
    command_buffer.dispatch(group_size_x, group_size_y, group_size_z);

    constexpr vk::ImageSubresourceRange subresource_range {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    vk::ImageMemoryBarrier image_memory_barriers[] {
        {.srcAccessMask =
             vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
         .dstAccessMask = vk::AccessFlagBits::eTransferRead,
         .oldLayout = vk::ImageLayout::eGeneral,
         .newLayout = vk::ImageLayout::eTransferSrcOptimal,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .image = m_storage_image.get(),
         .subresourceRange = subresource_range},
        {.srcAccessMask = vk::AccessFlagBits::eShaderRead,
         .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
         .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
         .newLayout = vk::ImageLayout::eTransferDstOptimal,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .image = m_render_target_image.get(),
         .subresourceRange = subresource_range}};

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader |
            vk::PipelineStageFlagBits::eFragmentShader,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        {},
        {},
        image_memory_barriers);

    constexpr vk::ImageSubresourceLayers subresource_layers {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1};

    const std::array offsets {
        vk::Offset3D {0, 0, 0},
        vk::Offset3D {static_cast<std::int32_t>(m_render_width),
                      static_cast<std::int32_t>(m_render_height),
                      1}};

    const vk::ImageBlit image_blit {.srcSubresource = subresource_layers,
                                    .srcOffsets = offsets,
                                    .dstSubresource = subresource_layers,
                                    .dstOffsets = offsets};

    command_buffer.blitImage(m_storage_image.get(),
                             vk::ImageLayout::eTransferSrcOptimal,
                             m_render_target_image.get(),
                             vk::ImageLayout::eTransferDstOptimal,
                             image_blit,
                             vk::Filter::eNearest);

    image_memory_barriers[0].srcAccessMask = vk::AccessFlagBits::eTransferRead;
    image_memory_barriers[0].dstAccessMask =
        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
    image_memory_barriers[0].oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    image_memory_barriers[0].newLayout = vk::ImageLayout::eGeneral;
    image_memory_barriers[1].srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    image_memory_barriers[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;
    image_memory_barriers[1].oldLayout = vk::ImageLayout::eTransferDstOptimal;
    image_memory_barriers[1].newLayout =
        vk::ImageLayout::eShaderReadOnlyOptimal;

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eComputeShader |
            vk::PipelineStageFlagBits::eFragmentShader,
        {},
        {},
        {},
        image_memory_barriers);

    constexpr vk::ClearValue clear_value {
        .color = {.float32 = std::array {0.0f, 0.0f, 0.0f, 1.0f}}};

    const vk::RenderPassBeginInfo render_pass_begin_info {
        .renderPass = m_render_pass.get(),
        .framebuffer = m_framebuffers[image_index].get(),
        .renderArea = {.offset = {0, 0}, .extent = m_swapchain_extent},
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
        .pWaitSemaphores = &m_image_available_semaphores[m_current_frame].get(),
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores =
            &m_render_finished_semaphores[m_current_frame].get()};

    m_graphics_compute_queue.submit(submit_info,
                                    m_in_flight_fences[m_current_frame].get());

    const vk::PresentInfoKHR present_info {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_render_finished_semaphores[m_current_frame].get(),
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain.get(),
        .pImageIndices = &image_index};

    // Use the version that does not call vk::resultCheck
    // (because vk::Result::eErrorOutOfDateKHR is an error)
    const auto present_result = m_present_queue.presentKHR(&present_info);

    if (present_result == vk::Result::eErrorOutOfDateKHR ||
        present_result == vk::Result::eSuboptimalKHR || m_framebuffer_resized)
    {
        m_framebuffer_resized = false;
        recreate_swapchain();
    }
    else
    {
        vk::resultCheck(present_result, "vk::Queue::presentKHR");
    }

    m_current_frame = (m_current_frame + 1) % s_frames_in_flight;
}

void Vulkan_renderer::resize_framebuffer()
{
    m_framebuffer_resized = true;
}

void Vulkan_renderer::resize_render_target(std::uint32_t render_width,
                                           std::uint32_t render_height)
{
    m_render_target_sampler = {};
    m_render_target_image_view = {};
    m_render_target_image = {};

    create_render_target(render_width, render_height);
}

void Vulkan_renderer::store_to_png(const char *file_name)
{
    const vk::BufferCreateInfo staging_buffer_create_info {
        .size = m_render_width * m_render_height * 4,
        .usage = vk::BufferUsageFlagBits::eTransferDst,
        .sharingMode = vk::SharingMode::eExclusive};

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

    const auto command_buffer = begin_one_time_submit_command_buffer();

    vk::ImageMemoryBarrier image_memory_barrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderRead,
        .dstAccessMask = vk::AccessFlagBits::eTransferRead,
        .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .newLayout = vk::ImageLayout::eTransferSrcOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_render_target_image.get(),
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                                   vk::PipelineStageFlagBits::eTransfer,
                                   {},
                                   {},
                                   {},
                                   image_memory_barrier);

    const vk::BufferImageCopy region {
        .bufferOffset = 0,
        .bufferImageHeight = m_render_height,
        .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .mipLevel = 0,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {m_render_width, m_render_height, 1}};

    command_buffer.copyImageToBuffer(m_render_target_image.get(),
                                     vk::ImageLayout::eTransferSrcOptimal,
                                     staging_buffer.get(),
                                     region);

    image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    image_memory_barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    image_memory_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eFragmentShader,
                                   {},
                                   {},
                                   {},
                                   image_memory_barrier);

    end_one_time_submit_command_buffer(command_buffer);

    if (!stbi_write_png(file_name,
                        static_cast<int>(m_render_width),
                        static_cast<int>(m_render_height),
                        4,
                        staging_buffer_allocation_info.pMappedData,
                        static_cast<int>(m_render_width * 4)))
    {
        throw std::runtime_error("Failed to write PNG image");
    }
}

void Vulkan_renderer::create_instance(std::uint32_t api_version)
{
    const auto vkGetInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(glfwGetInstanceProcAddress(
            VK_NULL_HANDLE, "vkGetInstanceProcAddr"));
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    const vk::ApplicationInfo application_info {.apiVersion = api_version};

    std::uint32_t glfw_required_extension_count {};
    const auto glfw_required_extension_names =
        glfwGetRequiredInstanceExtensions(&glfw_required_extension_count);

#ifndef NDEBUG

    const auto layer_properties = vk::enumerateInstanceLayerProperties();

    constexpr auto khronos_validation_layer = "VK_LAYER_KHRONOS_validation";
    if (std::none_of(layer_properties.begin(),
                     layer_properties.end(),
                     [](const VkLayerProperties &properties) {
                         return std::strcmp(properties.layerName,
                                            khronos_validation_layer) == 0;
                     }))
    {
        throw std::runtime_error("Validation layers are not supported");
    }

    std::vector<const char *> required_extensions;
    required_extensions.reserve(glfw_required_extension_count + 1);
    required_extensions.assign(glfw_required_extension_names,
                               glfw_required_extension_names +
                                   glfw_required_extension_count);
    required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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

    const vk::InstanceCreateInfo instance_create_info {
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = glfw_required_extension_count,
        .ppEnabledExtensionNames = glfw_required_extension_names};

    m_instance = vk::createInstanceUnique(instance_create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get());

#endif
}

void Vulkan_renderer::create_surface()
{
    VkSurfaceKHR surface {};
    const auto result =
        glfwCreateWindowSurface(m_instance.get(), m_window, nullptr, &surface);
    vk::resultCheck(static_cast<vk::Result>(result), "glfwCreateWindowSurface");
    m_surface = vk::UniqueSurfaceKHR(surface, m_instance.get());
}

void Vulkan_renderer::select_physical_device(
    std::uint32_t device_extension_count,
    const char *const *device_extension_names)
{
    const auto physical_devices = m_instance->enumeratePhysicalDevices();

    for (const auto physical_device : physical_devices)
    {
        m_queue_family_indices = get_queue_family_indices(physical_device);
        if ((m_queue_family_indices.graphics_compute ==
             std::numeric_limits<std::uint32_t>::max()) ||
            (m_queue_family_indices.present ==
             std::numeric_limits<std::uint32_t>::max()))
        {
            continue;
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
                    [extension_name](const VkExtensionProperties &properties) {
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
            !features.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>()
                 .accelerationStructure ||
            !features.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>()
                 .rayTracingPipeline)
        {
            continue;
        }

        m_physical_device = physical_device;
        return;
    }

    throw std::runtime_error("Failed to find a suitable physical device");
}

Queue_family_indices
Vulkan_renderer::get_queue_family_indices(vk::PhysicalDevice physical_device)
{
    Queue_family_indices indices {std::numeric_limits<std::uint32_t>::max(),
                                  std::numeric_limits<std::uint32_t>::max()};

    const auto properties = physical_device.getQueueFamilyProperties();

    for (std::uint32_t i {}; i < static_cast<std::uint32_t>(properties.size());
         ++i)
    {
        if ((properties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
            (properties[i].queueFlags & vk::QueueFlagBits::eCompute))
        {
            indices.graphics_compute = i;
        }

        if (glfwGetPhysicalDevicePresentationSupport(
                m_instance.get(), physical_device, i))
        {
            indices.present = i;
        }

        if ((indices.graphics_compute !=
             std::numeric_limits<std::uint32_t>::max()) &&
            (indices.present != std::numeric_limits<std::uint32_t>::max()))
        {
            break;
        }
    }

    return indices;
}

void Vulkan_renderer::create_device(std::uint32_t device_extension_count,
                                    const char *const *device_extension_names)
{
    constexpr float queue_priority {1.0f};

    const vk::DeviceQueueCreateInfo queue_create_infos[2] {
        {.queueFamilyIndex = m_queue_family_indices.graphics_compute,
         .queueCount = 1,
         .pQueuePriorities = &queue_priority},
        {.queueFamilyIndex = m_queue_family_indices.present,
         .queueCount = 1,
         .pQueuePriorities = &queue_priority}};

    const std::uint32_t queue_create_info_count {
        m_queue_family_indices.graphics_compute ==
                m_queue_family_indices.present
            ? 1u
            : 2u};

    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR
        ray_tracing_pipeline_features {.rayTracingPipeline = VK_TRUE};
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR
        acceleration_structure_features_khr {.pNext =
                                                 &ray_tracing_pipeline_features,
                                             .accelerationStructure = VK_TRUE};
    vk::PhysicalDeviceVulkan12Features vulkan_1_2_features {
        .pNext = &acceleration_structure_features_khr,
        .bufferDeviceAddress = VK_TRUE};
    const vk::PhysicalDeviceFeatures2 features_2 {.pNext =
                                                      &vulkan_1_2_features};

    const vk::DeviceCreateInfo device_create_info {
        .pNext = &features_2,
        .queueCreateInfoCount = queue_create_info_count,
        .pQueueCreateInfos = queue_create_infos,
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

void Vulkan_renderer::create_swapchain(std::uint32_t framebuffer_width,
                                       std::uint32_t framebuffer_height)
{
    const auto surface_formats =
        m_physical_device.getSurfaceFormatsKHR(m_surface.get());

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
    m_swapchain_format = surface_format.format;

    const auto surface_capabilities =
        m_physical_device.getSurfaceCapabilitiesKHR(m_surface.get());

    m_swapchain_extent.width = framebuffer_width;
    m_swapchain_extent.height = framebuffer_height;
    if (surface_capabilities.currentExtent.width !=
        std::numeric_limits<std::uint32_t>::max())
    {
        m_swapchain_extent = surface_capabilities.currentExtent;
    }
    m_swapchain_extent.width =
        std::clamp(m_swapchain_extent.width,
                   surface_capabilities.minImageExtent.width,
                   surface_capabilities.maxImageExtent.width);
    m_swapchain_extent.height =
        std::clamp(m_swapchain_extent.height,
                   surface_capabilities.minImageExtent.height,
                   surface_capabilities.maxImageExtent.height);

    m_swapchain_min_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 &&
        m_swapchain_min_image_count > surface_capabilities.maxImageCount)
    {
        m_swapchain_min_image_count = surface_capabilities.maxImageCount;
    }

    const std::uint32_t queue_family_indices[] {
        m_queue_family_indices.graphics_compute,
        m_queue_family_indices.present};
    auto sharing_mode = vk::SharingMode::eExclusive;
    std::uint32_t queue_family_index_count {1};
    if (m_queue_family_indices.graphics_compute !=
        m_queue_family_indices.present)
    {
        sharing_mode = vk::SharingMode::eConcurrent;
        queue_family_index_count = 2;
    }

    const vk::SwapchainCreateInfoKHR swapchain_create_info {
        .surface = m_surface.get(),
        .minImageCount = m_swapchain_min_image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = m_swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = sharing_mode,
        .queueFamilyIndexCount = queue_family_index_count,
        .pQueueFamilyIndices = queue_family_indices,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eFifo,
        .clipped = VK_TRUE};

    m_swapchain = m_device->createSwapchainKHRUnique(swapchain_create_info);

    m_swapchain_images = m_device->getSwapchainImagesKHR(m_swapchain.get());

    m_swapchain_image_views.resize(m_swapchain_images.size());
    vk::ImageViewCreateInfo image_view_create_info {
        .viewType = vk::ImageViewType::e2D,
        .format = m_swapchain_format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    for (std::size_t i {}; i < m_swapchain_image_views.size(); ++i)
    {
        image_view_create_info.image = m_swapchain_images[i];
        m_swapchain_image_views[i] =
            m_device->createImageViewUnique(image_view_create_info);
    }
}

void Vulkan_renderer::create_command_pool()
{
    const vk::CommandPoolCreateInfo create_info {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = m_queue_family_indices.graphics_compute};

    m_command_pool = m_device->createCommandPoolUnique(create_info);
}

[[nodiscard]] vk::CommandBuffer
Vulkan_renderer::begin_one_time_submit_command_buffer()
{
    const vk::CommandBufferAllocateInfo allocate_info {
        .commandPool = m_command_pool.get(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1};

    auto command_buffers = m_device->allocateCommandBuffers(allocate_info);
    auto command_buffer = command_buffers.front();

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

    m_graphics_compute_queue.submit(submit_info);
    m_graphics_compute_queue.waitIdle();
}

void Vulkan_renderer::create_storage_image(std::uint32_t width,
                                           std::uint32_t height)
{
    m_render_width = width;
    m_render_height = height;

    constexpr auto format = vk::Format::eR32G32B32A32Sfloat;

    const vk::ImageCreateInfo image_create_info {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
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

    const vk::ImageViewCreateInfo image_view_create_info {
        .image = m_storage_image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    m_storage_image_view =
        m_device->createImageViewUnique(image_view_create_info);

    const auto command_buffer = begin_one_time_submit_command_buffer();

    const vk::ImageMemoryBarrier image_memory_barrier {
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask =
            vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eGeneral,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_storage_image.get(),
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eComputeShader,
                                   {},
                                   {},
                                   {},
                                   image_memory_barrier);

    end_one_time_submit_command_buffer(command_buffer);
}

void Vulkan_renderer::create_render_target(std::uint32_t width,
                                           std::uint32_t height)
{
    constexpr auto format = vk::Format::eR8G8B8A8Srgb;

    const vk::ImageCreateInfo image_create_info {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eSampled |
                 vk::ImageUsageFlagBits::eTransferDst |
                 vk::ImageUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    m_render_target_image = Unique_image(
        m_allocator.get(), image_create_info, allocation_create_info);

    const vk::ImageViewCreateInfo image_view_create_info {
        .image = m_render_target_image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    m_render_target_image_view =
        m_device->createImageViewUnique(image_view_create_info);

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

    m_render_target_sampler =
        m_device->createSamplerUnique(sampler_create_info);

    const auto command_buffer = begin_one_time_submit_command_buffer();

    const vk::ImageMemoryBarrier memory_barrier {
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_render_target_image.get(),
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eFragmentShader,
                                   {},
                                   {},
                                   {},
                                   memory_barrier);

    end_one_time_submit_command_buffer(command_buffer);
}

void Vulkan_renderer::create_vertex_buffer(const std::vector<float> &vertices)
{
    m_num_vertices = static_cast<std::uint32_t>(vertices.size() / 3);
    m_vertex_buffer_size = vertices.size() * sizeof(float);

    const vk::BufferCreateInfo staging_buffer_create_info {
        .size = m_vertex_buffer_size,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive};

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

    const vk::BufferCreateInfo buffer_create_info {
        .size = m_vertex_buffer_size,
        .usage = vk::BufferUsageFlagBits::eStorageBuffer |
                 vk::BufferUsageFlagBits::eTransferDst |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::
                     eAccelerationStructureBuildInputReadOnlyKHR,
        .sharingMode = vk::SharingMode::eExclusive};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    m_vertex_buffer = Unique_buffer(
        m_allocator.get(), buffer_create_info, allocation_create_info, nullptr);

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

void Vulkan_renderer::create_index_buffer(
    const std::vector<std::uint32_t> &indices)
{
    m_num_indices = static_cast<std::uint32_t>(indices.size());
    m_index_buffer_size = indices.size() * sizeof(float);

    const vk::BufferCreateInfo staging_buffer_create_info {
        .size = m_index_buffer_size,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive};

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

    const vk::BufferCreateInfo buffer_create_info {
        .size = m_index_buffer_size,
        .usage = vk::BufferUsageFlagBits::eStorageBuffer |
                 vk::BufferUsageFlagBits::eTransferDst |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::
                     eAccelerationStructureBuildInputReadOnlyKHR,
        .sharingMode = vk::SharingMode::eExclusive};

    VmaAllocationCreateInfo allocation_create_info {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    m_index_buffer = Unique_buffer(
        m_allocator.get(), buffer_create_info, allocation_create_info, nullptr);

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
    const auto &obj_indices = shapes.front().mesh.indices;
    std::vector<std::uint32_t> indices(obj_indices.size());
    for (std::size_t i {}; i < obj_indices.size(); ++i)
    {
        indices[i] = static_cast<std::uint32_t>(obj_indices[i].vertex_index);
    }

    create_vertex_buffer(reader.GetAttrib().GetVertices());
    create_index_buffer(indices);
}

void Vulkan_renderer::create_descriptor_set_layout()
{
    constexpr vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[] {
        {.binding = 0,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 1,
         .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 2,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 3,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute}};

    const vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info {
        .bindingCount = static_cast<std::uint32_t>(
            std::size(descriptor_set_layout_bindings)),
        .pBindings = descriptor_set_layout_bindings};

    m_descriptor_set_layout = m_device->createDescriptorSetLayoutUnique(
        descriptor_set_layout_create_info);
}

void Vulkan_renderer::create_final_render_descriptor_set_layout()
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

    m_final_render_descriptor_set_layout =
        m_device->createDescriptorSetLayoutUnique(
            descriptor_set_layout_create_info);
}

void Vulkan_renderer::create_descriptor_pool()
{
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
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1000 * static_cast<std::uint32_t>(std::size(pool_sizes)),
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

    const vk::DescriptorImageInfo storage_image_descriptor_image_info {
        .sampler = VK_NULL_HANDLE,
        .imageView = m_storage_image_view.get(),
        .imageLayout = vk::ImageLayout::eGeneral};

    const vk::DescriptorBufferInfo vertex_buffer_descriptor_buffer_info {
        .buffer = m_vertex_buffer.get(),
        .offset = 0,
        .range = m_vertex_buffer_size};

    const vk::DescriptorBufferInfo index_buffer_descriptor_buffer_info {
        .buffer = m_index_buffer.get(),
        .offset = 0,
        .range = m_index_buffer_size};

    const vk::WriteDescriptorSet descriptor_writes[] {
        {.dstSet = m_descriptor_set,
         .dstBinding = 0,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &storage_image_descriptor_image_info},
        {.dstSet = m_descriptor_set,
         .dstBinding = 2,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &vertex_buffer_descriptor_buffer_info},
        {.dstSet = m_descriptor_set,
         .dstBinding = 3,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &index_buffer_descriptor_buffer_info}};

    m_device->updateDescriptorSets(descriptor_writes, {});
}

void Vulkan_renderer::create_final_render_descriptor_set()
{
    const vk::DescriptorSetAllocateInfo descriptor_set_allocate_info {
        .descriptorPool = m_descriptor_pool.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = &m_final_render_descriptor_set_layout.get()};

    const auto sets =
        m_device->allocateDescriptorSets(descriptor_set_allocate_info);
    m_final_render_descriptor_set = sets.front();

    const vk::DescriptorImageInfo render_target_descriptor_image_info {
        .sampler = m_render_target_sampler.get(),
        .imageView = m_render_target_image_view.get(),
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

    const vk::WriteDescriptorSet descriptor_write {
        .dstSet = m_final_render_descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &render_target_descriptor_image_info};

    m_device->updateDescriptorSets(descriptor_write, {});
}

void Vulkan_renderer::create_pipeline()
{
    const auto shader_code = read_binary_file("render.comp.spv");

    const vk::ShaderModuleCreateInfo shader_module_create_info {
        .codeSize = shader_code.size() * sizeof(std::uint32_t),
        .pCode = shader_code.data()};

    const auto shader_module =
        m_device->createShaderModuleUnique(shader_module_create_info);

    const vk::PipelineShaderStageCreateInfo shader_stage_create_info {
        .stage = vk::ShaderStageFlagBits::eCompute,
        .module = shader_module.get(),
        .pName = "main"};

    constexpr vk::PushConstantRange push_constant_range {
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(Push_constants)};

    const vk::PipelineLayoutCreateInfo pipeline_layout_create_info {
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptor_set_layout.get(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range};

    m_compute_pipeline_layout =
        m_device->createPipelineLayoutUnique(pipeline_layout_create_info);

    const vk::ComputePipelineCreateInfo compute_pipeline_create_info {
        .stage = shader_stage_create_info,
        .layout = m_compute_pipeline_layout.get()};

    auto compute_pipeline = m_device->createComputePipelineUnique(
        VK_NULL_HANDLE, compute_pipeline_create_info);
    vk::resultCheck(compute_pipeline.result,
                    "vk::Device::createComputePipelineUnique");
    m_compute_pipeline = std::move(compute_pipeline.value);
}

void Vulkan_renderer::create_render_pass()
{
    const vk::AttachmentDescription attachment_description {
        .format = m_swapchain_format,
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

    m_render_pass = m_device->createRenderPassUnique(render_pass_create_info);
}

void Vulkan_renderer::create_framebuffers()
{
    m_framebuffers.resize(m_swapchain_image_views.size());

    for (std::size_t i {}; i < m_framebuffers.size(); ++i)
    {
        const vk::FramebufferCreateInfo framebuffer_create_info {
            .renderPass = m_render_pass.get(),
            .attachmentCount = 1,
            .pAttachments = &m_swapchain_image_views[i].get(),
            .width = m_swapchain_extent.width,
            .height = m_swapchain_extent.height,
            .layers = 1};

        m_framebuffers[i] =
            m_device->createFramebufferUnique(framebuffer_create_info);
    }
}

void Vulkan_renderer::create_command_buffers()
{
    const vk::CommandBufferAllocateInfo command_buffer_allocate_info {
        .commandPool = m_command_pool.get(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = s_frames_in_flight};

    m_command_buffers =
        m_device->allocateCommandBuffers(command_buffer_allocate_info);
}

void Vulkan_renderer::create_synchronization_objects()
{
    constexpr vk::SemaphoreCreateInfo semaphore_create_info {};

    constexpr vk::FenceCreateInfo fence_create_info {
        .flags = vk::FenceCreateFlagBits::eSignaled};

    for (auto &semaphore : m_image_available_semaphores)
    {
        semaphore = m_device->createSemaphoreUnique(semaphore_create_info);
    }

    for (auto &semaphore : m_render_finished_semaphores)
    {
        semaphore = m_device->createSemaphoreUnique(semaphore_create_info);
    }

    for (auto &fence : m_in_flight_fences)
    {
        fence = m_device->createFenceUnique(fence_create_info);
    }
}

void Vulkan_renderer::init_imgui()
{
    const auto loader_func = [](const char *function_name, void *user_data)
    {
        const auto renderer = static_cast<const Vulkan_renderer *>(user_data);
        return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
            renderer->m_instance.get(), function_name);
    };
    ImGui_ImplVulkan_LoadFunctions(loader_func, this);

    const auto check_vk_result = [](VkResult result)
    { vk::resultCheck(static_cast<vk::Result>(result), "ImGui Vulkan call"); };

    ImGui_ImplVulkan_InitInfo init_info {
        .Instance = m_instance.get(),
        .PhysicalDevice = m_physical_device,
        .Device = m_device.get(),
        .QueueFamily = m_queue_family_indices.graphics_compute,
        .Queue = m_graphics_compute_queue,
        .PipelineCache = VK_NULL_HANDLE,
        .DescriptorPool = m_descriptor_pool.get(),
        .Subpass = 0,
        .MinImageCount = m_swapchain_min_image_count,
        .ImageCount = static_cast<std::uint32_t>(m_swapchain_images.size()),
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .Allocator = nullptr,
        .CheckVkResultFn = check_vk_result};

    ImGui_ImplVulkan_Init(&init_info, m_render_pass.get());

    const auto command_buffer = begin_one_time_submit_command_buffer();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    end_one_time_submit_command_buffer(command_buffer);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Vulkan_renderer::recreate_swapchain()
{
    // FIXME
    int width {};
    int height {};
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwWaitEvents();
        glfwGetFramebufferSize(m_window, &width, &height);
    }

    m_device->waitIdle();

    m_swapchain_image_views.clear();
    m_swapchain_images.clear();
    m_swapchain = {};
    m_swapchain_min_image_count = {};
    m_swapchain_extent = vk::Extent2D {};
    m_swapchain_format = vk::Format::eUndefined;

    create_swapchain(static_cast<std::uint32_t>(width),
                     static_cast<std::uint32_t>(height));
    create_framebuffers();
}

[[nodiscard]] vk::DeviceAddress
Vulkan_renderer::get_buffer_device_address(vk::Buffer buffer)
{
    const vk::BufferDeviceAddressInfo address_info {.buffer = buffer};
    return m_device->getBufferAddress(address_info);
}

void Vulkan_renderer::create_blas()
{
    const auto vertex_buffer_address =
        get_buffer_device_address(m_vertex_buffer.get());
    const auto index_buffer_address =
        get_buffer_device_address(m_index_buffer.get());

    const vk::AccelerationStructureGeometryTrianglesDataKHR triangles {
        .vertexFormat = vk::Format::eR32G32B32Sfloat,
        .vertexData = {.deviceAddress = vertex_buffer_address},
        .vertexStride = 3 * sizeof(float),
        .maxVertex = m_num_vertices - 1,
        .indexType = vk::IndexType::eUint32,
        .indexData = {.deviceAddress = index_buffer_address}};

    const vk::AccelerationStructureGeometryKHR geometry {
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry = {.triangles = triangles},
        .flags = vk::GeometryFlagBitsKHR::eOpaque};

    const vk::AccelerationStructureBuildRangeInfoKHR build_range_info {
        .primitiveCount = m_num_indices / 3,
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

    const vk::BufferCreateInfo scratch_buffer_create_info {
        .size = build_sizes_info.buildScratchSize,
        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::eStorageBuffer,
        .sharingMode = vk::SharingMode::eExclusive};

    VmaAllocationCreateInfo scratch_allocation_create_info {};
    scratch_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    const Unique_buffer scratch_buffer(m_allocator.get(),
                                       scratch_buffer_create_info,
                                       scratch_allocation_create_info,
                                       nullptr);

    const auto scratch_buffer_address =
        get_buffer_device_address(scratch_buffer.get());

    const auto command_buffer = begin_one_time_submit_command_buffer();

    const vk::BufferCreateInfo as_buffer_create_info {
        .size = build_sizes_info.accelerationStructureSize,
        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
        .sharingMode = vk::SharingMode::eExclusive};

    VmaAllocationCreateInfo as_allocation_create_info {};
    scratch_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    const Unique_buffer as_buffer(m_allocator.get(),
                                  as_buffer_create_info,
                                  as_allocation_create_info,
                                  nullptr);

    const vk::AccelerationStructureCreateInfoKHR
        acceleration_structure_create_info {
            .buffer = as_buffer.get(),
            .size = build_sizes_info.accelerationStructureSize,
            .type = vk::AccelerationStructureTypeKHR::eBottomLevel};

    const auto acceleration_structure =
        m_device->createAccelerationStructureKHRUnique(
            acceleration_structure_create_info);

    build_geometry_info.dstAccelerationStructure = acceleration_structure.get();
    build_geometry_info.scratchData.deviceAddress = scratch_buffer_address;

    command_buffer.buildAccelerationStructuresKHR(build_geometry_info,
                                                  &build_range_info);

    end_one_time_submit_command_buffer(command_buffer);
}

void Vulkan_renderer::create_tlas()
{
}
