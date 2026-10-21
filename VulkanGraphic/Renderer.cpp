#include "Renderer.h"
#include "Debug.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    const std::vector<char const*> requiredDeviceExtensions = { vk::KHRSwapchainExtensionName };

    std::filesystem::path GetExecutableDirectory()
    {
        std::vector<wchar_t> pathBuffer(32768);
        const DWORD pathLength = GetModuleFileNameW(
            nullptr,
            pathBuffer.data(),
            static_cast<DWORD>(pathBuffer.size()));
        if (pathLength == 0 || pathLength >= pathBuffer.size())
        {
            throw std::runtime_error("Failed to determine the executable directory.");
        }

        return std::filesystem::path(std::wstring(pathBuffer.data(), pathLength)).parent_path();
    }

    std::vector<uint32_t> ReadSpirvFile(const std::filesystem::path& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open shader file: " + filename.string());
        }

        const std::streamoff fileSize = file.tellg();
        if (fileSize <= 0 || fileSize % sizeof(uint32_t) != 0)
        {
            throw std::runtime_error("Shader file has an invalid SPIR-V size: " + filename.string());
        }

        std::vector<uint32_t> code(static_cast<size_t>(fileSize) / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(fileSize));
        if (!file)
        {
            throw std::runtime_error("Failed to read shader file: " + filename.string());
        }

        return code;
    }

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

        bool supportsSwapChain = false;
        if (supportsAllRequiredExtensions)
        {
            const auto swapChainSupport = GetSwapChainSupportDetails(physicalDevice, surface);
            supportsSwapChain =
                !swapChainSupport.formats.empty() &&
                !swapChainSupport.presentModes.empty();
        }

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
            supportsSwapChain &&
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

vk::SurfaceFormatKHR Renderer::ChooseSwapSurfaceFormat(
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

vk::PresentModeKHR Renderer::ChooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR>& availablePresentModes) const
{
    const auto preferredMode = std::ranges::find(
        availablePresentModes,
        vk::PresentModeKHR::eMailbox);

    return preferredMode != availablePresentModes.end()
        ? *preferredMode
        : vk::PresentModeKHR::eFifo;
}

vk::Extent2D Renderer::ChooseSwapExtent(
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

void Renderer::CreateSwapChain(GLFWwindow* window)
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

void Renderer::CreateImageViews()
{
    swapChainImageViews.reserve(swapChainImages.size());

    for (const auto& image : swapChainImages)
    {
        vk::ImageViewCreateInfo createInfo
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

void Renderer::CreateGraphicsPipeline()
{
    const auto shaderDirectory = GetExecutableDirectory() / L"shaders";
    const auto vertexShaderCode = ReadSpirvFile(shaderDirectory / L"vert.spv");
    const auto fragmentShaderCode = ReadSpirvFile(shaderDirectory / L"frag.spv");

    const vk::ShaderModuleCreateInfo vertexShaderCreateInfo
    {
        .codeSize = vertexShaderCode.size() * sizeof(uint32_t),
        .pCode = vertexShaderCode.data()
    };
    const vk::ShaderModuleCreateInfo fragmentShaderCreateInfo
    {
        .codeSize = fragmentShaderCode.size() * sizeof(uint32_t),
        .pCode = fragmentShaderCode.data()
    };

    const vk::raii::ShaderModule vertexShader(device, vertexShaderCreateInfo);
    const vk::raii::ShaderModule fragmentShader(device, fragmentShaderCreateInfo);

    const std::array shaderStages
    {
        vk::PipelineShaderStageCreateInfo
        {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = *vertexShader,
            .pName = "main"
        },
        vk::PipelineShaderStageCreateInfo
        {
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = *fragmentShader,
            .pName = "main"
        }
    };

    const vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    const vk::PipelineInputAssemblyStateCreateInfo inputAssembly
    {
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = false
    };
    const vk::PipelineViewportStateCreateInfo viewportState
    {
        .viewportCount = 1,
        .scissorCount = 1
    };
    const vk::PipelineRasterizationStateCreateInfo rasterizer
    {
        .depthClampEnable = false,
        .rasterizerDiscardEnable = false,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = false,
        .lineWidth = 1.0f
    };
    const vk::PipelineMultisampleStateCreateInfo multisampling
    {
        .rasterizationSamples = vk::SampleCountFlagBits::e1
    };
    const vk::PipelineColorBlendAttachmentState colorBlendAttachment
    {
        .blendEnable = false,
        .colorWriteMask = vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA
    };
    const vk::PipelineColorBlendStateCreateInfo colorBlending
    {
        .logicOpEnable = false,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };
    constexpr std::array dynamicStates
    {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    const vk::PipelineDynamicStateCreateInfo dynamicState
    {
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    const vk::PipelineLayoutCreateInfo layoutCreateInfo{};
    pipelineLayout = vk::raii::PipelineLayout(device, layoutCreateInfo);

    const vk::PipelineRenderingCreateInfo renderingInfo
    {
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapChainImageFormat
    };
    const vk::GraphicsPipelineCreateInfo pipelineCreateInfo
    {
        .pNext = &renderingInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = *pipelineLayout,
        .renderPass = nullptr,
        .subpass = 0
    };

    graphicsPipeline = device.createGraphicsPipeline(nullptr, pipelineCreateInfo);
}

void Renderer::CreateCommandPool()
{
    const vk::CommandPoolCreateInfo createInfo
    {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = graphicsQueueFamilyIndex
    };

    commandPool = vk::raii::CommandPool(device, createInfo);
}

void Renderer::CreateCommandBuffers()
{
    const vk::CommandBufferAllocateInfo allocateInfo
    {
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MaxFramesInFlight
    };

    commandBuffers = device.allocateCommandBuffers(allocateInfo);
}

void Renderer::CreateSyncObjects()
{
    const vk::SemaphoreCreateInfo semaphoreCreateInfo{};
    const vk::FenceCreateInfo fenceCreateInfo
    {
        .flags = vk::FenceCreateFlagBits::eSignaled
    };

    imageAvailableSemaphores.reserve(MaxFramesInFlight);
    inFlightFences.reserve(MaxFramesInFlight);
    for (uint32_t i = 0; i < MaxFramesInFlight; ++i)
    {
        imageAvailableSemaphores.emplace_back(device, semaphoreCreateInfo);
        inFlightFences.emplace_back(device, fenceCreateInfo);
    }

    renderFinishedSemaphores.reserve(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); ++i)
    {
        renderFinishedSemaphores.emplace_back(device, semaphoreCreateInfo);
    }

    imagesInFlight.resize(swapChainImages.size(), nullptr);
}

void Renderer::RecordCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex)
{
    commandBuffer.reset();
    commandBuffer.begin({});

    const vk::ImageMemoryBarrier toColorAttachment
    {
        .srcAccessMask = {},
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapChainImages[imageIndex],
        .subresourceRange =
        {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {},
        {},
        {},
        toColorAttachment);

    vk::ClearValue clearColor{};
    clearColor.color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    const vk::RenderingAttachmentInfo colorAttachment
    {
        .imageView = *swapChainImageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor
    };
    const vk::RenderingInfo renderingInfo
    {
        .renderArea =
        {
            .offset = { 0, 0 },
            .extent = swapChainExtent
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment
    };

    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

    const vk::Viewport viewport
    {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(swapChainExtent.width),
        .height = static_cast<float>(swapChainExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    const vk::Rect2D scissor
    {
        .offset = { 0, 0 },
        .extent = swapChainExtent
    };
    commandBuffer.setViewport(0, viewport);
    commandBuffer.setScissor(0, scissor);
    commandBuffer.draw(3, 1, 0, 0);
    commandBuffer.endRendering();

    const vk::ImageMemoryBarrier toPresent
    {
        .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .dstAccessMask = {},
        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout = vk::ImageLayout::ePresentSrcKHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapChainImages[imageIndex],
        .subresourceRange =
        {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        {},
        {},
        {},
        toPresent);

    commandBuffer.end();
}

void Renderer::DrawFrame()
{
    if (device.waitForFences(*inFlightFences[currentFrame], true, UINT64_MAX) != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to wait for the frame fence.");
    }

    const auto [result, imageIndex] = swapChain.acquireNextImage(
        UINT64_MAX,
        *imageAvailableSemaphores[currentFrame],
        nullptr);
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("Failed to acquire a swap chain image.");
    }

    if (imagesInFlight[imageIndex])
    {
        if (device.waitForFences(imagesInFlight[imageIndex], true, UINT64_MAX) != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to wait for the swap chain image fence.");
        }
    }
    imagesInFlight[imageIndex] = *inFlightFences[currentFrame];

    RecordCommandBuffer(commandBuffers[currentFrame], imageIndex);
    device.resetFences(*inFlightFences[currentFrame]);

    const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    const vk::CommandBuffer commandBuffer = *commandBuffers[currentFrame];
    const vk::Semaphore imageAvailableSemaphore = *imageAvailableSemaphores[currentFrame];
    const vk::Semaphore renderFinishedSemaphore = *renderFinishedSemaphores[imageIndex];
    const vk::SubmitInfo submitInfo
    {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphore
    };
    graphicsQueue.submit(submitInfo, *inFlightFences[currentFrame]);

    const vk::PresentInfoKHR presentInfo
    {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &*swapChain,
        .pImageIndices = &imageIndex
    };
    const vk::Result presentResult = presentQueue.presentKHR(presentInfo);
    if (presentResult != vk::Result::eSuccess && presentResult != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("Failed to present a swap chain image.");
    }

    currentFrame = (currentFrame + 1) % MaxFramesInFlight;
}

void Renderer::DestroySwapChain()
{
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
