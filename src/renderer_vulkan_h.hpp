#ifndef RENDERER_VULKAN_H_HPP
#define RENDERER_VULKAN_H_HPP

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

template <typename T>
struct Handle_deleter;

template <typename T>
    requires std::same_as<T, VkInstance> || std::same_as<T, VkDevice>
struct Handle_deleter<T>
{
    void operator()(VkInstance instance) const noexcept;
    void(VKAPI_PTR *destroy)(T, const VkAllocationCallbacks *);
};

template <>
struct Handle_deleter<VmaAllocator>
{
    void operator()(VmaAllocator allocator) const noexcept;
};

template <>
struct Handle_deleter<VkBuffer>
{
    void operator()(VkBuffer buffer) const noexcept;

    VmaAllocator allocator;
    VmaAllocation allocation;
};

template <>
struct Handle_deleter<VkImage>
{
    void operator()(VkImage image) const noexcept;

    VmaAllocator allocator;
    VmaAllocation allocation;
};

template <typename T>
class Handle : private Handle_deleter<T>
{
public:
    constexpr Handle() noexcept : Handle_deleter<T>(), m_value()
    {
    }

    constexpr explicit Handle(T value) noexcept
        : Handle_deleter<T>(), m_value(value)
    {
    }

    constexpr Handle(T value, const Handle_deleter<T> &deleter) noexcept
        : Handle_deleter<T>(deleter), m_value(value)
    {
    }

    constexpr Handle(const Handle &) = delete;

    constexpr Handle(Handle &&other) noexcept
        : Handle_deleter<T>(std::move(static_cast<Handle_deleter<T> &>(other))),
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

        static_cast<Handle_deleter<T> &>(*this) =
            std::move(static_cast<Handle_deleter<T> &>(other));
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

struct Vulkan_global_functions
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    PFN_vkEnumerateInstanceExtensionProperties
        vkEnumerateInstanceExtensionProperties;
    PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
    PFN_vkCreateInstance vkCreateInstance;
};

struct Vulkan_instance_functions
{
    PFN_vkDestroyInstance vkDestroyInstance;
    PFN_vkCreateDevice vkCreateDevice;
};

struct Vulkan_instance
{
    VkInstance instance;
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
};

#endif // RENDERER_VULKAN_H_HPP
