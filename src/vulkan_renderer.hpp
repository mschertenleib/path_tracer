#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

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
#include <utility>
#include <vector>

class Vulkan_renderer
{
public:
    constexpr Vulkan_renderer() noexcept = default;

    Vulkan_renderer(std::uint32_t render_width,
                    std::uint32_t render_height,
                    const std::vector<float> &vertices,
                    const std::vector<std::uint32_t> &indices,
                    const std::vector<float> &normals);

    Vulkan_renderer(const Vulkan_renderer &) = delete;
    Vulkan_renderer(Vulkan_renderer &&) noexcept = default;

    Vulkan_renderer &operator=(const Vulkan_renderer &) = delete;
    Vulkan_renderer &operator=(Vulkan_renderer &&) noexcept = default;

    void render();
    void store_to_png(const char *file_name);

private:
    void create_instance();
    void select_physical_device(std::uint32_t device_extension_count,
                                const char *const *device_extension_names);
    void create_device(std::uint32_t device_extension_count,
                       const char *const *device_extension_names);
    void create_command_pool();
    [[nodiscard]] vk::CommandBuffer begin_one_time_submit_command_buffer();
    void end_one_time_submit_command_buffer(vk::CommandBuffer command_buffer);
    void create_storage_image(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory>
    create_buffer(vk::DeviceSize size,
                  vk::BufferUsageFlags usage,
                  vk::MemoryPropertyFlags properties);
    void create_geometry_buffer(const std::vector<float> &vertices,
                                const std::vector<std::uint32_t> &indices,
                                const std::vector<float> &normals);
    void create_blas();
    void create_tlas();
    void create_descriptor_set_layout();
    void create_descriptor_pool();
    void create_descriptor_set();
    [[nodiscard]] vk::UniqueShaderModule
    create_shader_module(const char *file_name);
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
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
        m_ray_tracing_properties {};

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

#endif // VULKAN_RENDERER_HPP
