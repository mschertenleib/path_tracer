#ifndef RENDERER_HPP
#define RENDERER_HPP

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

class Unique_allocator
{
public:
    constexpr Unique_allocator() noexcept = default;

    explicit Unique_allocator(const VmaAllocatorCreateInfo &create_info);

    ~Unique_allocator() noexcept;

    constexpr friend void swap(Unique_allocator &a,
                               Unique_allocator &b) noexcept
    {
        std::swap(a.m_allocator, b.m_allocator);
    }

    constexpr Unique_allocator(Unique_allocator &&other) noexcept
        : Unique_allocator()
    {
        swap(*this, other);
    }

    Unique_allocator &operator=(Unique_allocator &&other) noexcept
    {
        auto tmp {std::move(other)};
        swap(*this, tmp);
        return *this;
    }

    [[nodiscard]] constexpr VmaAllocator get() const noexcept
    {
        return m_allocator;
    }

    Unique_allocator(const Unique_allocator &) = delete;
    Unique_allocator &operator=(const Unique_allocator &) = delete;

private:
    VmaAllocator m_allocator {};
};

class Unique_buffer
{
public:
    constexpr Unique_buffer() noexcept = default;

    Unique_buffer(VmaAllocator allocator,
                  const vk::BufferCreateInfo &buffer_create_info,
                  const VmaAllocationCreateInfo &allocation_create_info,
                  VmaAllocationInfo *allocation_info = nullptr);

    ~Unique_buffer() noexcept;

    constexpr friend void swap(Unique_buffer &a, Unique_buffer &b) noexcept
    {
        std::swap(a.m_allocator, b.m_allocator);
        std::swap(a.m_buffer, b.m_buffer);
        std::swap(a.m_allocation, b.m_allocation);
    }

    constexpr Unique_buffer(Unique_buffer &&other) noexcept : Unique_buffer()
    {
        swap(*this, other);
    }

    Unique_buffer &operator=(Unique_buffer &&other) noexcept
    {
        auto tmp {std::move(other)};
        swap(*this, tmp);
        return *this;
    }

    [[nodiscard]] constexpr vk::Buffer get() const noexcept
    {
        return m_buffer;
    }

    Unique_buffer(const Unique_buffer &) = delete;
    Unique_buffer &operator=(const Unique_buffer &) = delete;

private:
    VmaAllocator m_allocator {};
    vk::Buffer m_buffer {};
    VmaAllocation m_allocation {};
};

class Unique_image
{
public:
    constexpr Unique_image() noexcept = default;

    Unique_image(VmaAllocator allocator,
                 const vk::ImageCreateInfo &image_create_info,
                 const VmaAllocationCreateInfo &allocation_create_info,
                 VmaAllocationInfo *allocation_info = nullptr);

    ~Unique_image() noexcept;

    constexpr friend void swap(Unique_image &a, Unique_image &b) noexcept
    {
        std::swap(a.m_allocator, b.m_allocator);
        std::swap(a.m_image, b.m_image);
        std::swap(a.m_allocation, b.m_allocation);
    }

    constexpr Unique_image(Unique_image &&other) noexcept : Unique_image()
    {
        swap(*this, other);
    }

    Unique_image &operator=(Unique_image &&other) noexcept
    {
        auto tmp {std::move(other)};
        swap(*this, tmp);
        return *this;
    }

    [[nodiscard]] constexpr vk::Image get() const noexcept
    {
        return m_image;
    }

    Unique_image(const Unique_image &) = delete;
    Unique_image &operator=(const Unique_image &) = delete;

private:
    VmaAllocator m_allocator {};
    vk::Image m_image {};
    VmaAllocation m_allocation {};
};

struct Renderer
{
    vk::DynamicLoader dl {};

    vk::UniqueInstance instance {};

#ifndef NDEBUG
    vk::UniqueDebugUtilsMessengerEXT debug_messenger {};
#endif

    std::uint32_t queue_family_index {
        std::numeric_limits<std::uint32_t>::max()};
    vk::PhysicalDevice physical_device {};
    vk::PhysicalDeviceProperties physical_device_properties {};
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
        ray_tracing_pipeline_properties {};

    vk::UniqueDevice device {};

    Unique_allocator allocator {};

    vk::Queue queue {};

    vk::UniqueCommandPool command_pool {};

    std::uint32_t render_width {};
    std::uint32_t render_height {};
    Unique_image storage_image {};
    vk::UniqueImageView storage_image_view {};

    std::uint32_t vertex_count {};
    std::uint32_t index_count {};
    vk::DeviceSize vertex_range_size {};
    vk::DeviceSize index_range_offset {};
    vk::DeviceSize index_range_size {};
    vk::DeviceSize normal_range_offset {};
    vk::DeviceSize normal_range_size {};
    Unique_buffer geometry_buffer {};

    Unique_buffer blas_buffer {};
    vk::UniqueAccelerationStructureKHR blas {};
    Unique_buffer tlas_buffer {};
    vk::UniqueAccelerationStructureKHR tlas {};

    vk::UniqueDescriptorSetLayout descriptor_set_layout {};
    vk::UniqueDescriptorPool descriptor_pool {};
    vk::DescriptorSet descriptor_set {};

    std::vector<vk::RayTracingShaderGroupCreateInfoKHR>
        ray_tracing_shader_groups {};
    vk::UniquePipelineLayout ray_tracing_pipeline_layout {};
    vk::UniquePipeline ray_tracing_pipeline {};
    Unique_buffer sbt_buffer {};
    vk::StridedDeviceAddressRegionKHR rgen_region {};
    vk::StridedDeviceAddressRegionKHR miss_region {};
    vk::StridedDeviceAddressRegionKHR hit_region {};
    vk::StridedDeviceAddressRegionKHR call_region {};
};

[[nodiscard]] Renderer create_renderer();

void load_scene(Renderer &r,
                std::uint32_t render_width,
                std::uint32_t render_height,
                const struct Geometry &geometry);

void render(const Renderer &r);

void write_to_png(const Renderer &r, const char *file_name);

#endif // RENDERER_HPP
