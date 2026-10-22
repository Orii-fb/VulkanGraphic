#include "RendererInternal.h"

#include <limits>
#include <stdexcept>

void Renderer::Impl::Initialize(
    GLFWwindow* window,
    std::span<const Vertex> vertices,
    std::span<const uint16_t> indices)
{
    if (window == nullptr)
    {
        throw std::invalid_argument("A valid GLFW window is required to initialize the renderer.");
    }
    if (initializationStarted || shutdown)
    {
        throw std::logic_error("A renderer instance can only be initialized once.");
    }
    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0)
    {
        throw std::invalid_argument("Geometry must contain vertices and triangle indices.");
    }
    if (indices.size() > std::numeric_limits<uint32_t>::max())
    {
        throw std::invalid_argument("The geometry has too many indices to draw.");
    }
    for (const uint16_t index : indices)
    {
        if (index >= vertices.size())
        {
            throw std::invalid_argument("A geometry index refers to a missing vertex.");
        }
    }

    initializationStarted = true;
    CreateInstance();
    CreateSurface(window);
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChainResources(window);
    CreateVertexBuffer(vertices);
    CreateIndexBuffer(indices);
    indexCount = static_cast<uint32_t>(indices.size());
    initialized = true;
}

void Renderer::Impl::Shutdown()
{
    if (shutdown)
    {
        return;
    }

    DestroySwapChain();
    DestroySurface();

    if (logicalDeviceCreated)
    {
        vertexBuffer = nullptr;
        vertexBufferMemory = nullptr;
        indexBuffer = nullptr;
        indexBufferMemory = nullptr;
    }

    graphicsQueue = nullptr;
    presentQueue = nullptr;
    device = nullptr;
    physicalDevice = nullptr;
    debugMessenger = nullptr;
    instance = nullptr;
    logicalDeviceCreated = false;
    initialized = false;
    shutdown = true;
}

Renderer::Renderer()
    : impl(std::make_unique<Impl>())
{
}

Renderer::~Renderer()
{
    Shutdown();
}

void Renderer::Initialize(
    GLFWwindow* window,
    std::span<const Vertex> vertices,
    std::span<const uint16_t> indices)
{
    impl->Initialize(window, vertices, indices);
}

void Renderer::DrawFrame(GLFWwindow* window)
{
    impl->DrawFrame(window);
}

void Renderer::NotifyFramebufferResized() noexcept
{
    impl->NotifyFramebufferResized();
}

void Renderer::Shutdown()
{
    if (impl)
    {
        impl->Shutdown();
    }
}
