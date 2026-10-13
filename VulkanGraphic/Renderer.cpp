#include "Renderer.h"
#include "Debug.h"
#include <algorithm>
#include <iostream>
#include <optional>


#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace
{
    const std::vector<char const*> requiredDeviceExtensions = { vk::KHRSwapchainExtensionName };

    std::optional<uint32_t> FindGraphicsPresentQueueFamily(
        const vk::raii::PhysicalDevice& physicalDevice,
        const vk::raii::SurfaceKHR& surface)
    {
        const auto queueFamilies = physicalDevice.getQueueFamilyProperties();

        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        {
            const bool supportsGraphics = static_cast<bool>(
                queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics);
            const bool supportsPresentation = physicalDevice.getSurfaceSupportKHR(i, surface);

            if (queueFamilies[i].queueCount > 0 && supportsGraphics && supportsPresentation)
            {
                return i;
            }
        }

        return std::nullopt;
    }

    bool IsDeviceSuitable(
        const vk::raii::PhysicalDevice& physicalDevice,
        const vk::raii::SurfaceKHR& surface)
    {
        const bool supportsVulkan13 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;
        const bool supportsGraphicsAndPresentation =
            FindGraphicsPresentQueueFamily(physicalDevice, surface).has_value();

        const auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        const bool supportsAllRequiredExtensions = std::ranges::all_of(
            requiredDeviceExtensions,
            [&availableDeviceExtensions](const char* requiredExtension)
            {
                return std::ranges::any_of(availableDeviceExtensions, [requiredExtension](const auto& availableExtension)
                {
                    return std::strcmp(availableExtension.extensionName, requiredExtension) == 0;
                });
            });

        const auto features = physicalDevice.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        const bool supportsRequiredFeatures =
            features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
            features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
            features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        return supportsVulkan13 &&
            supportsGraphicsAndPresentation &&
            supportsAllRequiredExtensions &&
            supportsRequiredFeatures;
    }
}

void Renderer::CreateInstance()
{
    constexpr vk::ApplicationInfo app_info
    {
        .pApplicationName = "VulkanGraphic",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "VulkanEngine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = vk::ApiVersion14
    };

    std::vector requiredExtensions = GetRequiredExtensions();
    auto extensionProperties = context.enumerateInstanceExtensionProperties();

    for (uint32_t i = 0; i < requiredExtensions.size(); ++i)
    {
        auto predicate = [extension = requiredExtensions[i]] (auto const& extensionProperty)
        {
            return strcmp(extensionProperty.extensionName, extension) == 0;
        };

        if (std::ranges::none_of(extensionProperties, predicate))
        {
            throw std::runtime_error("Required extension not supported: " + std::string(requiredExtensions[i]));
        }
    }

    std::vector requiredLayers = GetRequiredLayers();
    auto layerProperties = context.enumerateInstanceLayerProperties();

    auto predicate_layer = [&layerProperties] (auto const& requiredLayer)
    {
        return std::ranges::none_of(layerProperties, [requiredLayer] (auto const& layerProperty)
        {
            return strcmp(layerProperty.layerName, requiredLayer) == 0;
        });
    };

    auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, predicate_layer);

    if (unsupportedLayerIt != requiredLayers.end())
    {
        throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
    }

    vk::InstanceCreateInfo instance_create_info
    {
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()
    };

    instance = vk::raii::Instance(context, instance_create_info);
    
    SetupDebugMessenger();
}

void Renderer::CreateSurface(GLFWwindow* window)
{
    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;

    if (glfwCreateWindowSurface(*instance, window, nullptr, &rawSurface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create window surface.");
    }

    surface = vk::raii::SurfaceKHR(instance, rawSurface);
}

void Renderer::PickPhysicalDevice()
{
    auto physicalDevices = instance.enumeratePhysicalDevices();
    const auto device = std::ranges::find_if(physicalDevices, [this](const auto& candidate)
    {
        return IsDeviceSuitable(candidate, surface);
    });

    if (device == physicalDevices.end())
    {
        throw std::runtime_error("Failed to find a suitable GPU.");
    }

    physicalDevice = std::move(*device);

    const auto queueFamilyIndex = FindGraphicsPresentQueueFamily(physicalDevice, surface);
    if (!queueFamilyIndex.has_value())
    {
        throw std::runtime_error("Failed to find a queue family supporting graphics and presentation.");
    }

    graphicsQueueFamilyIndex = *queueFamilyIndex;
}

void Renderer::CreateLogicalDevice()
{
    constexpr float queuePriority = 0.5f;
    const vk::DeviceQueueCreateInfo queueCreateInfo
    {
        .queueFamilyIndex = graphicsQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    vk::StructureChain<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> 
    
    featureChain
    {
        {},
        {.shaderDrawParameters = true},
        {.dynamicRendering = true},
        {.extendedDynamicState = true}
    };

    const vk::DeviceCreateInfo deviceCreateInfo
    {
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data()
    };

    device = vk::raii::Device(physicalDevice, deviceCreateInfo);
    graphicsQueue = vk::raii::Queue(device, graphicsQueueFamilyIndex, 0);
    presentQueue = vk::raii::Queue(device, graphicsQueueFamilyIndex, 0);
}

void Renderer::DestroySurface()
{
    surface = nullptr;
}

std::vector<char const*> Renderer::GetRequiredExtensions()
{
    uint32_t glfwExtensionsCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
    
    std::vector<char const*> applicationExtension = { vk::EXTDebugUtilsExtensionName };
    
    std::vector requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionsCount);
    
    requiredExtensions.append_range(applicationExtension);
    
    return requiredExtensions;
}

std::vector<char const*> Renderer::GetRequiredLayers()
{
    const std::vector validationLayers = { "VK_LAYER_KHRONOS_validation" };
    
    std::vector<char const*> requiredLayers = {  };

    if (Debug::EnableDebug)
        requiredLayers.append_range(validationLayers);
    
    return requiredLayers;
}

void Renderer::SetupDebugMessenger()
{
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | 
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    
    vk::DebugUtilsMessageTypeFlagsEXT typeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    
    vk::DebugUtilsMessengerCreateInfoEXT createInfo
    {
        .messageSeverity = severityFlags,
        .messageType = typeFlags,
        .pfnUserCallback = &DebugCallback,
    };
    
    debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo);
}

vk::Bool32 Renderer::DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
                                   const vk::DebugUtilsMessengerCallbackDataEXT* data, void*)
{
    std::println(std::cout,"Validation layer: Type[{}]", vk::to_string(type));
    std::println(std::cout,"Message :{}", data->pMessage);
    
    return vk::False;
}
