#include "RendererInternal.h"

#include <array>
#include <stdexcept>

void Renderer::Impl::CreateCommandPool()
{
    const vk::CommandPoolCreateInfo createInfo
    {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = graphicsQueueFamilyIndex
    };

    commandPool = vk::raii::CommandPool(device, createInfo);
}

void Renderer::Impl::CreateCommandBuffers()
{
    const vk::CommandBufferAllocateInfo allocateInfo
    {
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MaxFramesInFlight
    };

    commandBuffers = device.allocateCommandBuffers(allocateInfo);
}

void Renderer::Impl::CreateSyncObjects()
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

void Renderer::Impl::RecordCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex)
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
    const std::array vertexBuffers{ *vertexBuffer };
    constexpr std::array<vk::DeviceSize, 1> vertexOffsets{ 0 };
    commandBuffer.bindVertexBuffers(0, vertexBuffers, vertexOffsets);
    commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);

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
    commandBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
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

void Renderer::Impl::DrawFrame(GLFWwindow* window)
{
    if (!initialized || shutdown)
    {
        throw std::logic_error("The renderer must be initialized before drawing a frame.");
    }

    if (device.waitForFences(*inFlightFences[currentFrame], true, UINT64_MAX) != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to wait for the frame fence.");
    }

    const auto [result, imageIndex] = swapChain.acquireNextImage(
        UINT64_MAX,
        *imageAvailableSemaphores[currentFrame],
        nullptr);
    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        RecreateSwapChain(window);
        return;
    }
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
    if (presentResult != vk::Result::eSuccess &&
        presentResult != vk::Result::eSuboptimalKHR &&
        presentResult != vk::Result::eErrorOutOfDateKHR)
    {
        throw std::runtime_error("Failed to present a swap chain image.");
    }

    currentFrame = (currentFrame + 1) % MaxFramesInFlight;

    if (result == vk::Result::eSuboptimalKHR ||
        presentResult == vk::Result::eSuboptimalKHR ||
        presentResult == vk::Result::eErrorOutOfDateKHR ||
        framebufferResized)
    {
        RecreateSwapChain(window);
    }
}
