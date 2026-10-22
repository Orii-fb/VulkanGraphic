#include "Application.h"

void Application::Init()
{
    InitWindow();
    InitVulkan();
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
    DestroyVulkan();
    DestroyWindow();
}

void Application::InitWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(800, 600, "Vulkano", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);
}

void Application::InitVulkan()
{
    renderer.CreateInstance();
    renderer.CreateSurface(window);
    renderer.PickPhysicalDevice();
    renderer.CreateLogicalDevice();
    renderer.CreateVertexBuffer();
    renderer.CreateSwapChainResources(window);
}

void Application::DestroyWindow()
{
    glfwDestroyWindow(window);

    glfwTerminate();
}

void Application::DestroyVulkan()
{
    renderer.DestroySwapChain();
    renderer.DestroySurface();
}

void Application::FramebufferResizeCallback(GLFWwindow* window, int, int)
{
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (application != nullptr)
    {
        application->renderer.NotifyFramebufferResized();
    }
}
