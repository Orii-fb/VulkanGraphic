#include "Renderer.h"

#include <GLFW/glfw3.h>

#include <array>
#include <exception>
#include <iostream>

namespace
{
    void FramebufferResizeCallback(GLFWwindow* window, int, int)
    {
        auto* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
        if (renderer != nullptr)
        {
            renderer->NotifyFramebufferResized();
        }
    }
}

int main()
{
    if (glfwInit() != GLFW_TRUE)
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Renderer Client Test", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create the client window.\n";
        glfwTerminate();
        return 1;
    }

    Renderer renderer;
    glfwSetWindowUserPointer(window, &renderer);
    glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);

    int exitCode = 0;
    try
    {
        constexpr std::array<Vertex, 3> vertices
        {
            Vertex{ { 0.0f, 0.65f }, { 1.0f, 0.8f, 0.1f } },
            Vertex{ { -0.65f, -0.55f }, { 0.1f, 0.8f, 1.0f } },
            Vertex{ { 0.65f, -0.55f }, { 0.9f, 0.2f, 0.8f } }
        };
        constexpr std::array<uint16_t, 3> indices{ 0, 1, 2 };

        renderer.Initialize(window, vertices, indices);
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            renderer.DrawFrame(window);
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        exitCode = 1;
    }

    renderer.Shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
