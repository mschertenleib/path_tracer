#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <vk_mem_alloc.h>

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_NO_SPACESHIP_OPERATOR
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#ifndef NDEBUG
#define ENABLE_VALIDATION
#endif

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

struct Vulkan_context
{
    vk::DynamicLoader dl;
    vk::UniqueInstance instance;
#ifdef ENABLE_VALIDATION
    vk::UniqueDebugUtilsMessengerEXT debug_messenger;
#endif
    VkPhysicalDevice physical_device;
    std::uint32_t graphics_compute_queue_family_index;
    std::uint32_t present_queue_family_index;
    VkPhysicalDeviceProperties physical_device_properties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR
        ray_tracing_pipeline_properties;
    VkDevice device;
    VkQueue graphics_compute_queue;
    VkQueue present_queue;
    VkSurfaceKHR surface;
    VmaAllocator allocator;
    VkCommandPool command_pool;
    bool framebuffer_resized;
    std::uint32_t framebuffer_width;
    std::uint32_t framebuffer_height;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    std::uint32_t swapchain_min_image_count;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkDescriptorPool descriptor_pool;
    VkRenderPass render_pass;
    std::vector<VkFramebuffer> framebuffers;
    static constexpr std::uint32_t frames_in_flight {2};
    std::array<VkCommandBuffer, frames_in_flight> command_buffers;
    std::array<VkSemaphore, frames_in_flight> image_available_semaphores;
    std::array<VkSemaphore, frames_in_flight> render_finished_semaphores;
    std::array<VkFence, frames_in_flight> in_flight_fences;
    std::uint32_t current_frame_in_flight;
    std::uint32_t global_frame_count;
    bool imgui_initialized;
};

struct Vulkan_render_resources
{
    Vulkan_image storage_image;
    VkImageView storage_image_view;
    Vulkan_image render_target;
    VkImageView render_target_view;
    VkSampler render_target_sampler;
    Vulkan_buffer vertex_buffer;
    Vulkan_buffer index_buffer;
    Vulkan_buffer blas_buffer;
    VkAccelerationStructureKHR blas;
    Vulkan_buffer tlas_buffer;
    VkAccelerationStructureKHR tlas;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSetLayout final_render_descriptor_set_layout;
    VkDescriptorSet descriptor_set;
    VkDescriptorSet final_render_descriptor_set;
    VkPipelineLayout ray_tracing_pipeline_layout;
    VkPipeline ray_tracing_pipeline;
    Vulkan_buffer sbt_buffer;
    VkStridedDeviceAddressRegionKHR sbt_raygen_region;
    VkStridedDeviceAddressRegionKHR sbt_miss_region;
    VkStridedDeviceAddressRegionKHR sbt_hit_region;
    VkStridedDeviceAddressRegionKHR sbt_callable_region;
    std::uint32_t samples_to_render;
    std::uint32_t sample_count;
    std::uint32_t samples_per_frame;
};

[[nodiscard]] Vulkan_context create_context(struct GLFWwindow *window);

void destroy_context(Vulkan_context &context);

[[nodiscard]] Vulkan_render_resources
create_render_resources(const Vulkan_context &context,
                        std::uint32_t render_width,
                        std::uint32_t render_height,
                        const struct aiScene *scene);

void destroy_render_resources(const Vulkan_context &context,
                              Vulkan_render_resources &render_resources);

void draw_frame(Vulkan_context &context,
                Vulkan_render_resources &render_resources,
                const struct Camera &camera);

void resize_framebuffer(Vulkan_context &context,
                        std::uint32_t width,
                        std::uint32_t height);

void wait_idle(const Vulkan_context &context);

void reset_render(Vulkan_render_resources &render_resources);

// On failure, returns an error message. On success, returns an empty string.
[[nodiscard]] std::string
write_to_png(const Vulkan_context &context,
             const Vulkan_render_resources &render_resources,
             const char *file_name);

#endif // RENDERER_HPP
