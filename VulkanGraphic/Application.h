#pragma once

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

class Application
{
public:
	int width = 800;
	int height = 600;

	GLFWwindow* window = nullptr;

	void Init();
	void Run();
	void Destroy();

private:
	void InitWindow();
	void InitVulkan();

	void DestroyWindow();
	void DestroyVulkan();
};
