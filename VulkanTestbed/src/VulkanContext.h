#pragma once

namespace VulkanTestbed
{
	class VulkanContext
	{
	public:
		static void Init(void* glfwWindow);
		static void Shutdown();
	};
}
