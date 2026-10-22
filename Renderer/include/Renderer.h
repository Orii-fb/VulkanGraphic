#pragma once

// The public renderer interface intentionally avoids Vulkan headers.
#include <array>
#include <cstdint>
#include <memory>
#include <span>

struct GLFWwindow;

struct Vertex
{
    std::array<float, 2> position;
    std::array<float, 3> color;
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void Initialize(
        GLFWwindow* window,
        std::span<const Vertex> vertices,
        std::span<const uint16_t> indices);
    void DrawFrame(GLFWwindow* window);
    void NotifyFramebufferResized() noexcept;
    void Shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
