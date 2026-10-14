#pragma once

#include "Dependencies.h"
#include "Renderer.h"

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
	Renderer renderer;

	void InitWindow();
	void InitVulkan();

	void DestroyWindow();
	void DestroyVulkan();
};
