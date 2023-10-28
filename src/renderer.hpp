#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "vk_mem_alloc.h"

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

template <typename T>
struct Destructor;

template <>
struct Destructor<VmaAllocator>
{
    void operator()(VmaAllocator allocator) const noexcept;
};

template <>
struct Destructor<vk::Buffer>
{
    void operator()(vk::Buffer buffer) const noexcept;

    VmaAllocator allocator;
    VmaAllocation allocation;
};

template <>
struct Destructor<vk::Image>
{
    void operator()(vk::Image image) const noexcept;

    VmaAllocator allocator;
    VmaAllocation allocation;
};

template <typename T>
class Handle : private Destructor<T>
{
public:
    constexpr Handle() noexcept : Destructor<T>(), m_value()
    {
    }

    constexpr explicit Handle(T value) noexcept
        : Destructor<T>(), m_value(value)
    {
    }

    constexpr Handle(T value, const Destructor<T> &destructor) noexcept
        : Destructor<T>(destructor), m_value(value)
    {
    }

    constexpr Handle(const Handle &) = delete;

    constexpr Handle(Handle &&other) noexcept
        : Destructor<T>(std::move(static_cast<Destructor<T> &>(other))),
          m_value(other.m_value)
    {
        other.m_value = T {};
    }

    constexpr ~Handle() noexcept
    {
        if (m_value)
        {
            this->operator()(m_value);
        }
    }

    constexpr Handle &operator=(const Handle &) = delete;

    constexpr Handle &operator=(Handle &&other) noexcept
    {
        if (m_value)
        {
            this->operator()(m_value);
        }

        static_cast<Destructor<T> &>(*this) =
            std::move(static_cast<Destructor<T> &>(other));
        m_value = other.m_value;
        other.m_value = T {};
        return *this;
    }

    [[nodiscard]] constexpr T get() const noexcept
    {
        return m_value;
    }

private:
    T m_value;
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

    Handle<VmaAllocator> allocator {};

    vk::Queue queue {};

    vk::UniqueCommandPool command_pool {};

    std::uint32_t render_width {};
    std::uint32_t render_height {};
    Handle<vk::Image> storage_image {};
    vk::UniqueImageView storage_image_view {};

    std::uint32_t vertex_count {};
    std::uint32_t index_count {};
    vk::DeviceSize vertex_range_size {};
    vk::DeviceSize index_range_offset {};
    vk::DeviceSize index_range_size {};
    vk::DeviceSize normal_range_offset {};
    vk::DeviceSize normal_range_size {};
    Handle<vk::Buffer> geometry_buffer {};

    Handle<vk::Buffer> blas_buffer {};
    vk::UniqueAccelerationStructureKHR blas {};
    Handle<vk::Buffer> tlas_buffer {};
    vk::UniqueAccelerationStructureKHR tlas {};

    vk::UniqueDescriptorSetLayout descriptor_set_layout {};
    vk::UniqueDescriptorPool descriptor_pool {};
    vk::DescriptorSet descriptor_set {};

    std::vector<vk::RayTracingShaderGroupCreateInfoKHR>
        ray_tracing_shader_groups {};
    vk::UniquePipelineLayout ray_tracing_pipeline_layout {};
    vk::UniquePipeline ray_tracing_pipeline {};
    Handle<vk::Buffer> sbt_buffer {};
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

// TODO: it might be just better to dump this bloated vulkan.hpp nonsense and
// just have create/destroy pairs that make sense, having try/catch blocks in
// higher level functions if necessary

#endif // RENDERER_HPP
