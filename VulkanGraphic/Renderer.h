#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "vulkan/vulkan_raii.hpp"

class Renderer
{
public:
    void CreateInstance();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    
private:
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::Device device = nullptr;
    vk::raii::Queue graphicsQueue = nullptr;
    uint32_t graphicsQueueFamilyIndex = 0;
    
    std::vector<char const*> GetRequiredExtensions();
    std::vector<char const*> GetRequiredLayers();
    
    void SetupDebugMessenger();
    
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT, 
        vk::DebugUtilsMessageTypeFlagsEXT, 
        const vk::DebugUtilsMessengerCallbackDataEXT*,
        void*);
};
