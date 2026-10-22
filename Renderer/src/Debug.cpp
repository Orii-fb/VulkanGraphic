#include "Debug.h"

#include "RendererInternal.h"

#include <iostream>
#include <print>

std::vector<char const*> Renderer::Impl::GetRequiredLayers()
{
    const std::vector validationLayers = { "VK_LAYER_KHRONOS_validation" };

    std::vector<char const*> requiredLayers = {};
    if (Debug::EnableDebug)
    {
        requiredLayers.append_range(validationLayers);
    }

    return requiredLayers;
}

void Renderer::Impl::SetupDebugMessenger()
{
    const vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    const vk::DebugUtilsMessageTypeFlagsEXT typeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    const vk::DebugUtilsMessengerCreateInfoEXT createInfo
    {
        .messageSeverity = severityFlags,
        .messageType = typeFlags,
        .pfnUserCallback = &DebugCallback
    };

    debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo);
}

vk::Bool32 Renderer::Impl::DebugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    std::println(std::cout, "Validation layer: Type[{}]", vk::to_string(type));
    std::println(std::cout, "Message :{}", data->pMessage);

    return vk::False;
}
