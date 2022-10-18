#include "pch.h"
#include "VulkanContext.h"

#include <optional>
#include <set>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>

namespace VulkanTestbed
{
	static VkInstance s_VkInstance = nullptr;
	static VkPhysicalDevice s_PhysicalDevice = nullptr;
	static VkDevice s_LogicalDevice = nullptr;
	static VkQueue s_GraphicsQueue = nullptr;
	static VkQueue s_PresentQueue = nullptr;
	static VkDebugReportCallbackEXT s_DebugReport = nullptr;

	static GLFWwindow* s_GlfwWindow = nullptr;
	static VkSurfaceKHR s_Surface = nullptr;
	static VkSwapchainKHR s_Swapchain = nullptr;

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> GraphicsFamily;
		std::optional<uint32_t> PresentFamily;

		bool IsComplete() const
		{
			return GraphicsFamily.has_value() && PresentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	enum class Vendor
	{
		None = 0,
		AMD, Intel, NVidia,
		ARM, Qualcomm
	};

	static std::unordered_map<uint32_t, Vendor> VendorMap =
	{
		{ 0x1002, Vendor::AMD },
		{ 0x8086, Vendor::Intel },
		{ 0x10DE, Vendor::NVidia },
		{ 0x13B5, Vendor::ARM },
		{ 0x5143, Vendor::Qualcomm },
	};

	static std::string GetDriverVersionString(const VkPhysicalDeviceProperties& deviceProps, const VkPhysicalDeviceDriverProperties& driverProps)
	{
		// Intel
		if (VendorMap[deviceProps.vendorID] == Vendor::Intel)
		{
			return fmt::format("30.0.{}.{}",
				(deviceProps.driverVersion >> 14),
				(deviceProps.driverVersion) & 0x3fff);
		}

		return driverProps.driverInfo;
	}

	static const std::unordered_map<Vendor, const char*> VendorIdToStringMap =
	{
		{ Vendor::AMD,		"Advanced Micro Devices" },
		{ Vendor::Intel,	"Intel" },
		{ Vendor::NVidia,	"NVIDIA Corporation" },
		{ Vendor::ARM,		"ARM" },
		{ Vendor::Qualcomm, "Qualcomm" },
	};

	static bool CheckLayersSupport(const std::vector<const char*>& layers, const std::vector<VkLayerProperties>& availableLayers)
	{
#ifdef TESTBED_DEBUG
		for (const char* layerName : layers)
		{
			bool layerFound = false;
			for (const auto& layer : availableLayers)
			{
				if (strcmp(layerName, layer.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
				return false;
		}

		return true;
#else
		return false;
#endif
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
		[[maybe_unused]] VkDebugReportFlagsEXT flags,
		[[maybe_unused]] VkDebugReportObjectTypeEXT objectType,
		[[maybe_unused]] uint64_t object,
		[[maybe_unused]] size_t location,
		[[maybe_unused]] int32_t messageCode,
		[[maybe_unused]] const char* pLayerPrefix,
		[[maybe_unused]] const char* pMessage,
		[[maybe_unused]] void* pUserData)
	{
		switch (flags)
		{
			case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:			LOG_INFO("VULKAN: {}", pMessage); break;
			case VK_DEBUG_REPORT_DEBUG_BIT_EXT:					LOG_DEBUG("VULKAN: {}", pMessage); break;
			case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
			case VK_DEBUG_REPORT_WARNING_BIT_EXT:				LOG_WARN("VULKAN: {}", pMessage); break;
			case VK_DEBUG_REPORT_ERROR_BIT_EXT:					LOG_ERROR("VULKAN: {}", pMessage); break;
		}

		return VK_FALSE;
	}

	static void QuerrySwapChainSupport(SwapChainSupportDetails& outDetails)
	{
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_PhysicalDevice, s_Surface, &outDetails.Capabilities);

		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(s_PhysicalDevice, s_Surface, &formatCount, nullptr);
		outDetails.Formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(s_PhysicalDevice, s_Surface, &formatCount, outDetails.Formats.data());

		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(s_PhysicalDevice, s_Surface, &presentModeCount, nullptr);
		outDetails.PresentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(s_PhysicalDevice, s_Surface, &presentModeCount, outDetails.PresentModes.data());
	}

	static VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
	{
		for (const auto& fmt : formats)
		{
			if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return fmt;
		}

		return formats[0];
	}

	static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
	{
		for (const auto& mode : presentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
				return mode;
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;

		int width, height;
		glfwGetFramebufferSize(s_GlfwWindow, &width, &height);
		VkExtent2D actualExtent = { (uint32_t)width, (uint32_t)height };
		actualExtent.width = glm::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = glm::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return actualExtent;
	}

	void VulkanContext::Init(void* glfwWindow)
	{
		s_GlfwWindow = (GLFWwindow*)glfwWindow;

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan Testbed";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "NA";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#ifdef TESTBED_DEBUG
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = (uint32_t)extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef TESTBED_DEBUG
		const std::vector<const char*> validationLayers =
		{
			"VK_LAYER_KHRONOS_validation"
		};
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
		if (CheckLayersSupport(validationLayers, availableLayers))
		{
			createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}
#else
		createInfo.enabledLayerCount = 0;
#endif

		VkResult result = vkCreateInstance(&createInfo, nullptr, &s_VkInstance);
		TESTBED_ASSERT(result == VK_SUCCESS, "Failed to create vulkan instance!");
		
#ifdef TESTBED_DEBUG
		auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(s_VkInstance, "vkCreateDebugReportCallbackEXT");
		TESTBED_ASSERT(vkCreateDebugReportCallbackEXT);

		VkDebugReportCallbackCreateInfoEXT debugReportCI = {};
		debugReportCI.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		debugReportCI.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		debugReportCI.pfnCallback = &VulkanDebugCallback;
		debugReportCI.pUserData = nullptr;
		vkCreateDebugReportCallbackEXT(s_VkInstance, &debugReportCI, nullptr, &s_DebugReport);
#endif

		// Surface
		{
			VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
			surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			surfaceCreateInfo.hwnd = glfwGetWin32Window((GLFWwindow*)glfwWindow);

			surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
			result = vkCreateWin32SurfaceKHR(s_VkInstance, &surfaceCreateInfo, nullptr, &s_Surface);
			TESTBED_ASSERT(result == VK_SUCCESS, "Failed to create vulkan surface!");
		}

		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(s_VkInstance, &deviceCount, nullptr);
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(s_VkInstance, &deviceCount, devices.data());
		for (const auto& device : devices)
		{
			VkPhysicalDeviceProperties deviceProps;
			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceProperties(device, &deviceProps);
			vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

			if (deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				s_PhysicalDevice = device;
				break;
			}
		}

		// Fall back to the first GPU
		if (!s_PhysicalDevice)
			s_PhysicalDevice = devices[0];

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

		QueueFamilyIndices queueFamilyIndices;
		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				queueFamilyIndices.GraphicsFamily = i;

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(s_PhysicalDevice, i, s_Surface, &presentSupport);
			if (presentSupport)
				queueFamilyIndices.PresentFamily = i;

			i++;
		}


		SwapChainSupportDetails swapChainSupport = {};
		QuerrySwapChainSupport(swapChainSupport);
		bool swapchainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
		TESTBED_ASSERT(swapchainAdequate, "Swapchain Not supported!");
		{
			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
			std::set<uint32_t> uniqueQueueFamilies =
			{
				queueFamilyIndices.GraphicsFamily.value(),
				queueFamilyIndices.PresentFamily.value()
			};

			const float queuePriorities = 1.0f;
			for (uint32_t queueFamily : uniqueQueueFamilies)
			{
				VkDeviceQueueCreateInfo queueCreateInfo = {};
				queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueCreateInfo.queueFamilyIndex = queueFamily;
				queueCreateInfo.queueCount = 1;
				queueCreateInfo.pQueuePriorities = &queuePriorities;
				queueCreateInfos.push_back(queueCreateInfo);
			}

			const std::vector<const char*> deviceExtensions =
			{
				VK_KHR_SWAPCHAIN_EXTENSION_NAME
			};
			uint32_t deviceExtensionsCount = 0;
			vkEnumerateDeviceExtensionProperties(s_PhysicalDevice, nullptr, &deviceExtensionsCount, nullptr);
			std::vector<VkExtensionProperties> availableDeviceExtensions(deviceExtensionsCount);
			vkEnumerateDeviceExtensionProperties(s_PhysicalDevice, nullptr, &deviceExtensionsCount, availableDeviceExtensions.data());
			std::set<std::string> remainingExtensions(deviceExtensions.begin(), deviceExtensions.end());
			for (const auto& ext : availableDeviceExtensions)
				remainingExtensions.erase(ext.extensionName);
			TESTBED_ASSERT(remainingExtensions.empty(), "Not all device extensions are supported!");
			
			VkPhysicalDeviceFeatures deviceFeatures = {};

			VkDeviceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
			createInfo.pQueueCreateInfos = queueCreateInfos.data();
			createInfo.pEnabledFeatures = &deviceFeatures;
			createInfo.enabledExtensionCount = deviceExtensions.size();
			createInfo.ppEnabledExtensionNames = deviceExtensions.data();
#ifdef TESTBED_DEBUG
			createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();
#else
			createInfo.enabledLayerCount = 0;
#endif
			VkResult result = vkCreateDevice(s_PhysicalDevice, &createInfo, nullptr, &s_LogicalDevice);
			TESTBED_ASSERT(result == VK_SUCCESS);

			vkGetDeviceQueue(s_LogicalDevice, queueFamilyIndices.GraphicsFamily.value(), 0, &s_GraphicsQueue);
			vkGetDeviceQueue(s_LogicalDevice, queueFamilyIndices.PresentFamily.value(), 0, &s_PresentQueue);
		}

		{
			VkPhysicalDeviceDriverProperties driverProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
			VkPhysicalDeviceProperties2 deviceProps2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &driverProps };
			vkGetPhysicalDeviceProperties2(s_PhysicalDevice, &deviceProps2);
			const VkPhysicalDeviceProperties& deviceProps = deviceProps2.properties;

			const char* vendorName = "Unknown Vendor";
			if (VendorMap.find(deviceProps.vendorID) != VendorMap.end())
				vendorName = VendorIdToStringMap.at(VendorMap.at(deviceProps.vendorID));

			std::string driverString = GetDriverVersionString(deviceProps, driverProps);

			LOG_INFO("Vulkan Info:");
			LOG_INFO("\tVendor: {}", vendorName);
			LOG_INFO("\tRenderer: {}", deviceProps.deviceName);
			LOG_INFO("\tVersion: {}.{}.{} {} {}",
				VK_VERSION_MAJOR(deviceProps.apiVersion),
				VK_VERSION_MINOR(deviceProps.apiVersion),
				VK_VERSION_PATCH(deviceProps.apiVersion),
				driverProps.driverName,
				driverString);
		}

		// Create SwapChain
		{
			VkSurfaceFormatKHR surfaceFormat = ChooseSwapChainSurfaceFormat(swapChainSupport.Formats);
			VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.PresentModes);
			VkExtent2D extent = ChooseSwapExtent(swapChainSupport.Capabilities);
			uint32_t imageCount = swapChainSupport.Capabilities.minImageCount + 1;
			if (swapChainSupport.Capabilities.maxImageCount > 0 && imageCount > swapChainSupport.Capabilities.maxImageCount)
				imageCount = swapChainSupport.Capabilities.maxImageCount;

			VkSwapchainCreateInfoKHR createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = s_Surface;
			createInfo.minImageCount = imageCount;
			createInfo.imageFormat = surfaceFormat.format;
			createInfo.imageColorSpace = surfaceFormat.colorSpace;
			createInfo.imageExtent = extent;
			createInfo.imageArrayLayers = 1;
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			uint32_t queueFamilyIndicesArray[] = { queueFamilyIndices.GraphicsFamily.value(), queueFamilyIndices.PresentFamily.value() };
			if (queueFamilyIndices.GraphicsFamily != queueFamilyIndices.PresentFamily)
			{
				createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				createInfo.queueFamilyIndexCount = 2;
				createInfo.pQueueFamilyIndices = queueFamilyIndicesArray;
			}
			else
			{
				createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				createInfo.queueFamilyIndexCount = 0;
				createInfo.pQueueFamilyIndices = nullptr;
			}

			createInfo.preTransform = swapChainSupport.Capabilities.currentTransform;
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			createInfo.presentMode = presentMode;
			createInfo.clipped = VK_TRUE;
			createInfo.oldSwapchain = VK_NULL_HANDLE;

			result = vkCreateSwapchainKHR(s_LogicalDevice, &createInfo, nullptr, &s_Swapchain);
			TESTBED_ASSERT(result == VK_SUCCESS, "Failed to create the swapchain!");
		}
	}

	void VulkanContext::Shutdown()
	{
		vkDestroySwapchainKHR(s_LogicalDevice, s_Swapchain, nullptr);
		vkDestroyDevice(s_LogicalDevice, nullptr);
		vkDestroySurfaceKHR(s_VkInstance, s_Surface, nullptr);

#ifdef TESTBED_DEBUG
		// Remove the debug report callback
		auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(s_VkInstance, "vkDestroyDebugReportCallbackEXT");
		vkDestroyDebugReportCallbackEXT(s_VkInstance, s_DebugReport, nullptr);
#endif

		vkDestroyInstance(s_VkInstance, nullptr);
	}
}
