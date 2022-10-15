#include "pch.h"
#include "Application.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include "VulkanContext.h"

namespace VulkanTestbed
{
	Application::Application()
	{
		Log::Init();

		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		m_Window = glfwCreateWindow(1600, 900, "Vulkan Testbed", nullptr, nullptr);
		
		VulkanContext::Init();
	}

	Application::~Application()
	{
		VulkanContext::Shutdown();

		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}

	void Application::Run()
	{
		while (!glfwWindowShouldClose(m_Window))
		{
			glfwPollEvents();
		}
	}
}
