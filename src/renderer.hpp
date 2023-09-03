#ifndef RENDERER_HPP
#define RENDERER_HPP

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

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

class Renderer
{
public:
    Renderer();

    void load_scene(std::uint32_t render_width,
                    std::uint32_t render_height,
                    const struct Geometry &geometry);
    void render() const;
    void write_to_png(const char *file_name) const;

private:
    void create_instance();
    void select_physical_device(std::uint32_t device_extension_count,
                                const char *const *device_extension_names);
    void create_device(std::uint32_t device_extension_count,
                       const char *const *device_extension_names);
    void create_command_pool();
    [[nodiscard]] vk::CommandBuffer
    begin_one_time_submit_command_buffer() const;
    void
    end_one_time_submit_command_buffer(vk::CommandBuffer command_buffer) const;
    void create_storage_image(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory>
    create_buffer(vk::DeviceSize size,
                  vk::BufferUsageFlags usage,
                  vk::MemoryPropertyFlags properties) const;
    void create_geometry_buffer(const std::vector<float> &vertices,
                                const std::vector<std::uint32_t> &indices,
                                const std::vector<float> &normals);
    void create_blas();
    void create_tlas();
    void create_descriptor_set_layout();
    void create_descriptor_pool();
    void create_descriptor_set();
    [[nodiscard]] vk::UniqueShaderModule
    create_shader_module(const char *file_name) const;
    void create_ray_tracing_pipeline();
    void create_shader_binding_table();

    vk::DynamicLoader m_dl {};

    vk::UniqueInstance m_instance {};

#ifndef NDEBUG
    vk::UniqueDebugUtilsMessengerEXT m_debug_messenger {};
#endif

    std::uint32_t m_queue_family_index {
        std::numeric_limits<std::uint32_t>::max()};
    vk::PhysicalDevice m_physical_device {};
    vk::PhysicalDeviceProperties m_physical_device_properties {};
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
        m_ray_tracing_pipeline_properties {};

    vk::UniqueDevice m_device {};

    vk::Queue m_queue {};

    vk::UniqueCommandPool m_command_pool {};

    std::uint32_t m_width {};
    std::uint32_t m_height {};
    vk::UniqueImage m_storage_image {};
    vk::UniqueDeviceMemory m_storage_image_memory {};
    vk::UniqueImageView m_storage_image_view {};

    std::uint32_t m_vertex_count {};
    std::uint32_t m_index_count {};
    vk::DeviceSize m_vertex_range_size {};
    vk::DeviceSize m_index_range_offset {};
    vk::DeviceSize m_index_range_size {};
    vk::DeviceSize m_normal_range_offset {};
    vk::DeviceSize m_normal_range_size {};
    vk::UniqueBuffer m_geometry_buffer {};
    vk::UniqueDeviceMemory m_geometry_memory {};

    vk::UniqueBuffer m_blas_buffer {};
    vk::UniqueDeviceMemory m_blas_buffer_memory {};
    vk::UniqueAccelerationStructureKHR m_blas {};
    vk::UniqueBuffer m_tlas_buffer {};
    vk::UniqueDeviceMemory m_tlas_buffer_memory {};
    vk::UniqueAccelerationStructureKHR m_tlas {};

    vk::UniqueDescriptorSetLayout m_descriptor_set_layout {};

    vk::UniqueDescriptorPool m_descriptor_pool {};

    vk::DescriptorSet m_descriptor_set {};

    std::vector<vk::RayTracingShaderGroupCreateInfoKHR>
        m_ray_tracing_shader_groups {};
    vk::UniquePipelineLayout m_ray_tracing_pipeline_layout {};
    vk::UniquePipeline m_ray_tracing_pipeline {};
    vk::UniqueBuffer m_sbt_buffer {};
    vk::UniqueDeviceMemory m_sbt_buffer_memory {};
    vk::StridedDeviceAddressRegionKHR m_rgen_region {};
    vk::StridedDeviceAddressRegionKHR m_miss_region {};
    vk::StridedDeviceAddressRegionKHR m_hit_region {};
    vk::StridedDeviceAddressRegionKHR m_call_region {};
};

#endif // RENDERER_HPP
