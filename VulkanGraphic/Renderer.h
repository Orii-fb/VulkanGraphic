#pragma once

#include "Dependencies.h"

#include <cstdint>
#include <vector>

struct SwapChainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

class Renderer
{
public:
    void CreateInstance();
    void CreateSurface(GLFWwindow* window);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateVertexBuffer();
    void CreateSwapChainResources(GLFWwindow* window);
    void CreateSwapChain(GLFWwindow* window);
    void CreateImageViews();
    void CreateGraphicsPipeline();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void DrawFrame(GLFWwindow* window);
    void NotifyFramebufferResized() noexcept;
    void DestroySwapChain();
    void DestroySurface();

private:
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::SurfaceKHR surface = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::Device device = nullptr;
    vk::raii::Queue graphicsQueue = nullptr;
    vk::raii::Queue presentQueue = nullptr;
    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    std::vector<vk::raii::ImageView> swapChainImageViews;
    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;
    vk::raii::CommandPool commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;
    std::vector<vk::Fence> imagesInFlight;
    vk::Format swapChainImageFormat = vk::Format::eUndefined;
    vk::Extent2D swapChainExtent;
    uint32_t graphicsQueueFamilyIndex = 0;
    uint32_t currentFrame = 0;
    bool framebufferResized = false;

    static constexpr uint32_t MaxFramesInFlight = 2;

    vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(
        const std::vector<vk::SurfaceFormatKHR>& availableFormats) const;
    vk::PresentModeKHR ChooseSwapPresentMode(
        const std::vector<vk::PresentModeKHR>& availablePresentModes) const;
    vk::Extent2D ChooseSwapExtent(
        const vk::SurfaceCapabilitiesKHR& capabilities,
        GLFWwindow* window) const;
    bool RecreateSwapChain(GLFWwindow* window);
    void RecordCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex);

    std::vector<char const*> GetRequiredExtensions();
    std::vector<char const*> GetRequiredLayers();

    void SetupDebugMessenger();

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT,
        vk::DebugUtilsMessageTypeFlagsEXT,
        const vk::DebugUtilsMessengerCallbackDataEXT*,
        void*);
};
