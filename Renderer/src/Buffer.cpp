#include "RendererInternal.h"

#include <cstring>
#include <optional>
#include <stdexcept>
#include <utility>

void Renderer::Impl::CreateVertexBuffer(std::span<const Vertex> vertices)
{
    const vk::DeviceSize bufferSize = vertices.size_bytes();
    auto [stagingBuffer, stagingBufferMemory] = CreateBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void* mappedMemory = stagingBufferMemory.mapMemory(0, bufferSize);
    std::memcpy(mappedMemory, vertices.data(), static_cast<size_t>(bufferSize));
    stagingBufferMemory.unmapMemory();

    auto [deviceBuffer, deviceBufferMemory] = CreateBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    CopyBuffer(stagingBuffer, deviceBuffer, bufferSize);
    vertexBuffer = std::move(deviceBuffer);
    vertexBufferMemory = std::move(deviceBufferMemory);
}

void Renderer::Impl::CreateIndexBuffer(std::span<const uint16_t> indices)
{
    const vk::DeviceSize bufferSize = indices.size_bytes();
    auto [stagingBuffer, stagingBufferMemory] = CreateBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void* mappedMemory = stagingBufferMemory.mapMemory(0, bufferSize);
    std::memcpy(mappedMemory, indices.data(), static_cast<size_t>(bufferSize));
    stagingBufferMemory.unmapMemory();

    auto [deviceBuffer, deviceBufferMemory] = CreateBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    CopyBuffer(stagingBuffer, deviceBuffer, bufferSize);
    indexBuffer = std::move(deviceBuffer);
    indexBufferMemory = std::move(deviceBufferMemory);
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Renderer::Impl::CreateBuffer(
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties) const
{
    const vk::BufferCreateInfo bufferCreateInfo
    {
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };
    vk::raii::Buffer buffer(device, bufferCreateInfo);

    const vk::DeviceBufferMemoryRequirements memoryRequirementsInfo
    {
        .pCreateInfo = &bufferCreateInfo
    };
    const vk::MemoryRequirements2 memoryRequirements2 =
        device.getBufferMemoryRequirements(memoryRequirementsInfo);
    const vk::MemoryRequirements& memoryRequirements = memoryRequirements2.memoryRequirements;
    const vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

    std::optional<uint32_t> memoryTypeIndex;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        const bool isAllowedType = (memoryRequirements.memoryTypeBits & (uint32_t{ 1 } << i)) != 0;
        const bool hasRequiredProperties =
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (isAllowedType && hasRequiredProperties)
        {
            memoryTypeIndex = i;
            break;
        }
    }

    if (!memoryTypeIndex.has_value())
    {
        throw std::runtime_error("Failed to find a compatible memory type for a buffer.");
    }

    const vk::MemoryAllocateInfo allocateInfo
    {
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = *memoryTypeIndex
    };
    vk::raii::DeviceMemory memory(device, allocateInfo);
    buffer.bindMemory(*memory, 0);
    return { std::move(buffer), std::move(memory) };
}

void Renderer::Impl::CopyBuffer(
    const vk::raii::Buffer& sourceBuffer,
    const vk::raii::Buffer& destinationBuffer,
    vk::DeviceSize size) const
{
    const vk::CommandBufferAllocateInfo allocateInfo
    {
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1
    };
    auto copyCommandBuffers = device.allocateCommandBuffers(allocateInfo);
    vk::raii::CommandBuffer& copyCommandBuffer = copyCommandBuffers.front();
    const vk::CommandBufferBeginInfo beginInfo
    {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    copyCommandBuffer.begin(beginInfo);
    const vk::BufferCopy copyRegion
    {
        .size = size
    };
    copyCommandBuffer.copyBuffer(*sourceBuffer, *destinationBuffer, copyRegion);
    copyCommandBuffer.end();

    const vk::CommandBuffer commandBuffer = *copyCommandBuffer;
    const vk::SubmitInfo submitInfo
    {
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    graphicsQueue.submit(submitInfo, nullptr);
    graphicsQueue.waitIdle();
}
