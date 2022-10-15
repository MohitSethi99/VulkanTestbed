#pragma once

// This ignores all warnings raised inside External headers
#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/bundled/format.h>
#pragma warning(pop)

namespace VulkanTestbed
{
	class Log
	{
	public:
		static void Init();

		static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_Logger; }

	private:
		static std::shared_ptr<spdlog::logger> s_Logger;
	};
}

#define LOG_TRACE(...)		::VulkanTestbed::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)		::VulkanTestbed::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LOG_DEBUG(...)		::VulkanTestbed::Log::GetCoreLogger()->debug(__VA_ARGS__)
#define LOG_WARN(...)		::VulkanTestbed::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)		::VulkanTestbed::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...)	::VulkanTestbed::Log::GetCoreLogger()->critical(__VA_ARGS__)
