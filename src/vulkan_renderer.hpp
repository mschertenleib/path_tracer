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
#include <vector>

struct Unique_buffer
{
    vk::UniqueBuffer buffer;
    vk::UniqueDeviceMemory memory;
};

struct Unique_image
{
    vk::UniqueImage image;
    vk::UniqueDeviceMemory memory;
};

class Vulkan_renderer
{
public:
    constexpr Vulkan_renderer() noexcept = default;

    Vulkan_renderer(std::uint32_t render_width, std::uint32_t render_height);

    Vulkan_renderer(const Vulkan_renderer &) = delete;
    Vulkan_renderer(Vulkan_renderer &&) noexcept = default;

    Vulkan_renderer &operator=(const Vulkan_renderer &) = delete;
    Vulkan_renderer &operator=(Vulkan_renderer &&) noexcept = default;

    void render();
    void store_to_png(const char *file_name);

private:
    void create_instance(std::uint32_t api_version);
    void select_physical_device(std::uint32_t device_extension_count,
                                const char *const *device_extension_names);
    [[nodiscard]] static std::uint32_t
    get_queue_family_index(vk::PhysicalDevice physical_device);
    void create_device(std::uint32_t device_extension_count,
                       const char *const *device_extension_names);
    void create_command_pool();
    [[nodiscard]] vk::CommandBuffer begin_one_time_submit_command_buffer();
    void end_one_time_submit_command_buffer(vk::CommandBuffer command_buffer);
    [[nodiscard]] std::uint32_t
    find_memory_type(std::uint32_t type_filter,
                     vk::MemoryPropertyFlags properties);
    void create_storage_image(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] Unique_buffer
    create_buffer(vk::DeviceSize size,
                  vk::BufferUsageFlags usage,
                  vk::MemoryPropertyFlags properties);
    [[nodiscard]] Unique_buffer create_device_local_buffer_from_data(
        vk::DeviceSize size, vk::BufferUsageFlags usage, const void *data);
    void create_vertex_buffer(const std::vector<float> &vertices);
    void create_normals_buffer(const std::vector<float> &normals);
    void create_index_buffer(const std::vector<std::uint32_t> &indices);
    void create_geometry_buffers();
    [[nodiscard]] vk::DeviceAddress get_device_address(vk::Buffer buffer);
    [[nodiscard]] vk::DeviceAddress
    get_device_address(vk::AccelerationStructureKHR acceleration_structure);
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
    Unique_image m_storage_image {};
    vk::UniqueImageView m_storage_image_view {};

    std::uint32_t m_vertex_count {};
    vk::DeviceSize m_vertex_buffer_size {};
    Unique_buffer m_vertex_buffer {};

    vk::DeviceSize m_normals_buffer_size {};
    Unique_buffer m_normals_buffer {};

    std::uint32_t m_index_count {};
    vk::DeviceSize m_index_buffer_size {};
    Unique_buffer m_index_buffer {};

    Unique_buffer m_blas_buffer {};
    vk::UniqueAccelerationStructureKHR m_blas {};
    Unique_buffer m_tlas_buffer {};
    vk::UniqueAccelerationStructureKHR m_tlas {};

    vk::UniqueDescriptorSetLayout m_descriptor_set_layout {};

    vk::UniqueDescriptorPool m_descriptor_pool {};

    vk::DescriptorSet m_descriptor_set {};

    std::vector<vk::RayTracingShaderGroupCreateInfoKHR>
        m_ray_tracing_shader_groups {};
    vk::UniquePipelineLayout m_ray_tracing_pipeline_layout {};
    vk::UniquePipeline m_ray_tracing_pipeline {};
    Unique_buffer m_sbt_buffer {};
    vk::StridedDeviceAddressRegionKHR m_rgen_region {};
    vk::StridedDeviceAddressRegionKHR m_miss_region {};
    vk::StridedDeviceAddressRegionKHR m_hit_region {};
    vk::StridedDeviceAddressRegionKHR m_call_region {};
};

#endif // VULKAN_RENDERER_HPP
