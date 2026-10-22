#pragma once

#include "Renderer.h"

#include <GLFW/glfw3.h>

class Application
{
public:
	void Init();
	void Run();
	void Destroy();

private:
	GLFWwindow* window = nullptr;
	Renderer renderer;

	void InitWindow();
	void DestroyWindow();
	static void FramebufferResizeCallback(GLFWwindow* window, int, int);
};
