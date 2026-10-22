#include "Application.h"

#include <stdexcept>

void Application::Init()
{
    InitWindow();

    constexpr std::array<Vertex, 4> vertices
    {
        Vertex{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
        Vertex{ { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
        Vertex{ { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
        Vertex{ { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f } }
    };
    constexpr std::array<uint16_t, 6> indices{ 0, 1, 2, 2, 3, 0 };
    renderer.Initialize(window, vertices, indices);
}

void Application::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        renderer.DrawFrame(window);
    }
}

void Application::Destroy()
{
    renderer.Shutdown();
    DestroyWindow();
}

void Application::InitWindow()
{
    if (glfwInit() != GLFW_TRUE)
    {
        throw std::runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(800, 600, "Vulkano", nullptr, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create the application window.");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);
}

void Application::DestroyWindow()
{
    if (window != nullptr)
    {
        glfwDestroyWindow(window);
        window = nullptr;
        glfwTerminate();
    }

}

void Application::FramebufferResizeCallback(GLFWwindow* window, int, int)
{
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (application != nullptr)
    {
        application->renderer.NotifyFramebufferResized();
    }
}
