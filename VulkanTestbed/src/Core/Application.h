#pragma once

struct GLFWwindow;

namespace VulkanTestbed
{
	class Application
	{
	public:
		Application();
		~Application();

		void Run();

	private:
		GLFWwindow* m_Window;
	};
}
