#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <vk_mem_alloc.h>

#include <vulkan/vulkan.h>

#include <array>
#include <vector>

#include <cstdint>

struct Vulkan_queue_family_indices
{
    std::uint32_t graphics_compute;
    std::uint32_t present;
};

struct Vulkan_device
{
    VkPhysicalDevice physical_device;
    Vulkan_queue_family_indices queue_family_indices;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR
        ray_tracing_pipeline_properties;
    VkDevice device;
};

struct Vulkan_swapchain
{
    VkFormat format;
    VkExtent2D extent;
    std::uint32_t min_image_count;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
};

struct Vulkan_image
{
    std::uint32_t width;
    std::uint32_t height;
    VkImage image;
    VmaAllocation allocation;
};

struct Vulkan_buffer
{
    VkDeviceSize size;
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct Vulkan_acceleration_structure
{
    Vulkan_buffer buffer;
    VkAccelerationStructureKHR acceleration_structure;
};

struct Vulkan_shader_binding_table
{
    Vulkan_buffer buffer;
    VkStridedDeviceAddressRegionKHR raygen_region;
    VkStridedDeviceAddressRegionKHR miss_region;
    VkStridedDeviceAddressRegionKHR hit_region;
    VkStridedDeviceAddressRegionKHR callable_region;
};

struct Vulkan_context
{
    VkInstance instance;
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    Vulkan_device device;
    VkQueue graphics_compute_queue;
    VkQueue present_queue;
    VkSurfaceKHR surface;
    VmaAllocator allocator;
    VkCommandPool command_pool;
    Vulkan_swapchain swapchain;
    Vulkan_image storage_image;
    VkImageView storage_image_view;
    Vulkan_image render_target;
    VkImageView render_target_view;
    VkSampler render_target_sampler;
    Vulkan_buffer vertex_buffer;
    Vulkan_buffer index_buffer;
    Vulkan_acceleration_structure blas;
    Vulkan_acceleration_structure tlas;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSetLayout final_render_descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkDescriptorSet final_render_descriptor_set;
    VkPipelineLayout ray_tracing_pipeline_layout;
    VkPipeline ray_tracing_pipeline;
    Vulkan_shader_binding_table shader_binding_table;
    VkRenderPass render_pass;
    std::vector<VkFramebuffer> framebuffers;
    static constexpr std::uint32_t frames_in_flight {2};
    std::array<VkCommandBuffer, frames_in_flight> command_buffers;
    std::array<VkSemaphore, frames_in_flight> image_available_semaphores;
    std::array<VkSemaphore, frames_in_flight> render_finished_semaphores;
    std::array<VkFence, frames_in_flight> in_flight_fences;
    std::uint32_t current_in_flight_frame;
    bool framebuffer_resized;
    std::uint32_t framebuffer_width;
    std::uint32_t framebuffer_height;
    std::uint32_t global_frame_count;
    std::uint32_t samples_to_render;
    std::uint32_t sample_count;
    std::uint32_t samples_per_frame;
    bool imgui_initialized;
};

[[nodiscard]] Vulkan_context create_vulkan_context(struct GLFWwindow *window);

void destroy_vulkan_context(Vulkan_context &context);

void load_scene(Vulkan_context &context,
                std::uint32_t render_width,
                std::uint32_t render_height,
                const struct aiScene *scene);

void destroy_scene_resources(const Vulkan_context &context);

void draw_frame(Vulkan_context &context, const struct Camera &camera);

void resize_framebuffer(Vulkan_context &context,
                        std::uint32_t width,
                        std::uint32_t height);

void reset_render(Vulkan_context &context);

void write_to_png(const Vulkan_context &context, const char *file_name);

void wait_idle(const Vulkan_context &context);

#endif // RENDERER_HPP
