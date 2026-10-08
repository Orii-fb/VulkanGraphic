#include "Renderer.h"

#include "GLFW/glfw3.h"

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

    uint32_t glfwExtensionsCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

    auto extensionProperties = context.enumerateInstanceExtensionProperties();

    for (uint32_t i = 0; i < glfwExtensionsCount; ++i)
    {
        auto predicate = [glfwExtension = glfwExtensions[i]] (auto const& extensionProperty)
        {
            return strcmp(extensionProperty.extensionName, glfwExtension) == 0;
        };

        if (std::ranges::none_of(extensionProperties, predicate))
        {
            throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtensions[i]));
        }
    }

    const std::vector validationLayers = { "VK_LAYER_KHRONOS_validation" };

#ifdef NDEBUG
    bool enableValidationLayers = false;
#else
    bool enableValidationLayers = true;
#endif

    std::vector<char const*> requiredLayers;

    if (enableValidationLayers)
        requiredLayers.assign_range(validationLayers);

    auto layerProperties = context.enumerateInstanceLayerProperties();

    auto predicate_layer = [&layerProperties] (auto const& requiredLayer)
    {
        return std::ranges::none_of(layerProperties, [requiredLayer] (auto const& layerProperty)
        {
            return strcmp(layerProperty.layerName, requiredLayer) == 0;
        });
    };

    auto unsupportedLayerIt =
        std::ranges::find_if(requiredLayers, predicate_layer);

    if (unsupportedLayerIt != requiredLayers.end())
    {
        throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
    }

    vk::InstanceCreateInfo instance_create_info
    {
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = glfwExtensionsCount,
        .ppEnabledExtensionNames = glfwExtensions
    };

    instance = vk::raii::Instance(context, instance_create_info);
}