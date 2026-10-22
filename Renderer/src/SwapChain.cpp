#include "RendererInternal.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

SwapChainSupportDetails GetSwapChainSupportDetails(
    const vk::raii::PhysicalDevice& device,
    const vk::raii::SurfaceKHR& surface)
{
    return
    {
        .capabilities = device.getSurfaceCapabilitiesKHR(surface),
        .formats = device.getSurfaceFormatsKHR(surface),
        .presentModes = device.getSurfacePresentModesKHR(surface)
    };
}

void Renderer::Impl::CreateSwapChainResources(GLFWwindow* window)
{
    CreateSwapChain(window);
    CreateImageViews();
    CreateGraphicsPipeline();
    CreateCommandPool();
    CreateCommandBuffers();
    CreateSyncObjects();
}

vk::SurfaceFormatKHR Renderer::Impl::ChooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& availableFormats) const
{
    const auto preferredFormat = std::ranges::find_if(
        availableFormats,
        [](const vk::SurfaceFormatKHR& format)
        {
            return format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });

    return preferredFormat != availableFormats.end() ? *preferredFormat : availableFormats.front();
}

vk::PresentModeKHR Renderer::Impl::ChooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR>& availablePresentModes) const
{
    const auto preferredMode = std::ranges::find(
        availablePresentModes,
        vk::PresentModeKHR::eMailbox);

    return preferredMode != availablePresentModes.end()
        ? *preferredMode
        : vk::PresentModeKHR::eFifo;
}

vk::Extent2D Renderer::Impl::ChooseSwapExtent(
    const vk::SurfaceCapabilitiesKHR& capabilities,
    GLFWwindow* window) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    return
    {
        .width = std::clamp(
            static_cast<uint32_t>(width),
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width),
        .height = std::clamp(
            static_cast<uint32_t>(height),
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height)
    };
}

void Renderer::Impl::CreateSwapChain(GLFWwindow* window)
{
    const auto swapChainSupport = GetSwapChainSupportDetails(physicalDevice, surface);
    if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
    {
        throw std::runtime_error("The surface has no supported swap chain formats or presentation modes.");
    }

    const auto surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    const auto presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    const auto extent = ChooseSwapExtent(swapChainSupport.capabilities, window);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    const vk::SwapchainCreateInfoKHR createInfo
    {
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = swapChainSupport.capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode,
        .clipped = vk::True,
        .oldSwapchain = nullptr
    };

    swapChain = device.createSwapchainKHR(createInfo);
    swapChainImages = swapChain.getImages();
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void Renderer::Impl::CreateImageViews()
{
    swapChainImageViews.reserve(swapChainImages.size());

    for (const auto& image : swapChainImages)
    {
        const vk::ImageViewCreateInfo createInfo
        {
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = swapChainImageFormat,
            .components =
            {
                .r = vk::ComponentSwizzle::eIdentity,
                .g = vk::ComponentSwizzle::eIdentity,
                .b = vk::ComponentSwizzle::eIdentity,
                .a = vk::ComponentSwizzle::eIdentity
            },
            .subresourceRange =
            {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        swapChainImageViews.emplace_back(device, createInfo);
    }
}

bool Renderer::Impl::RecreateSwapChain(GLFWwindow* window)
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    while (width == 0 || height == 0)
    {
        if (glfwWindowShouldClose(window))
        {
            return false;
        }

        glfwWaitEvents();
        glfwGetFramebufferSize(window, &width, &height);
    }

    DestroySwapChain();
    CreateSwapChainResources(window);
    framebufferResized = false;
    return true;
}

void Renderer::Impl::DestroySwapChain()
{
    if (!logicalDeviceCreated)
    {
        return;
    }

    device.waitIdle();

    imagesInFlight.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();
    imageAvailableSemaphores.clear();
    commandBuffers.clear();
    commandPool = nullptr;
    graphicsPipeline = nullptr;
    pipelineLayout = nullptr;
    swapChainImageViews.clear();
    swapChain = nullptr;
    swapChainImages.clear();
}

void Renderer::Impl::DestroySurface()
{
    surface = nullptr;
}

void Renderer::Impl::NotifyFramebufferResized() noexcept
{
    framebufferResized = true;
}
