#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

#include <array>
#include <vector>

#include <cstdint>

struct Vulkan_instance
{
    VkInstance instance;
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
    PFN_vkEnumerateInstanceExtensionProperties
        vkEnumerateInstanceExtensionProperties;
    PFN_vkCreateInstance vkCreateInstance;
    PFN_vkDestroyInstance vkDestroyInstance;
#ifndef NDEBUG
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
#endif
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties
        vkGetPhysicalDeviceQueueFamilyProperties;
    PFN_vkEnumerateDeviceExtensionProperties
        vkEnumerateDeviceExtensionProperties;
    PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
    PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
    PFN_vkCreateDevice vkCreateDevice;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
        vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
};

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
    PFN_vkDestroyDevice vkDestroyDevice;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkCreateCommandPool vkCreateCommandPool;
    PFN_vkDestroyCommandPool vkDestroyCommandPool;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkCreateSampler vkCreateSampler;
    PFN_vkDestroySampler vkDestroySampler;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkQueueWaitIdle vkQueueWaitIdle;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkCmdBindPipeline vkCmdBindPipeline;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    PFN_vkCmdPushConstants vkCmdPushConstants;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    PFN_vkCmdBlitImage vkCmdBlitImage;
    PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
    PFN_vkGetAccelerationStructureDeviceAddressKHR
        vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR
        vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    PFN_vkCreateShaderModule vkCreateShaderModule;
    PFN_vkDestroyShaderModule vkDestroyShaderModule;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    PFN_vkDestroyPipeline vkDestroyPipeline;
    PFN_vkGetRayTracingShaderGroupHandlesKHR
        vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCreateRenderPass vkCreateRenderPass;
    PFN_vkDestroyRenderPass vkDestroyRenderPass;
    PFN_vkCreateFramebuffer vkCreateFramebuffer;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
    PFN_vkCreateSemaphore vkCreateSemaphore;
    PFN_vkDestroySemaphore vkDestroySemaphore;
    PFN_vkCreateFence vkCreateFence;
    PFN_vkDestroyFence vkDestroyFence;
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
    Vulkan_instance instance;
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
    std::uint32_t current_frame;
};

[[nodiscard]] Vulkan_context create_vulkan_context(struct GLFWwindow *window);

void destroy_vulkan_context(Vulkan_context &context);

void load_scene(Vulkan_context &context,
                std::uint32_t render_width,
                std::uint32_t render_height,
                const struct Geometry &geometry);

void destroy_scene_resources(const Vulkan_context &context);

void render(const Vulkan_context &context);

void write_to_png(const Vulkan_context &context, const char *file_name);

#endif // RENDERER_HPP
