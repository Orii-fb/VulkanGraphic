#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "vulkan/vulkan_raii.hpp"

class Renderer
{
public:
    void CreateInstance();
    void PickPhysicalDevice();
    
private:
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    
    std::vector<char const*> GetRequiredExtensions();
    std::vector<char const*> GetRequiredLayers();
    
    void SetupDebugMessenger();
    
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT, 
        vk::DebugUtilsMessageTypeFlagsEXT, 
        const vk::DebugUtilsMessengerCallbackDataEXT*,
        void*);
};
