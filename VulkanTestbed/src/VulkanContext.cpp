#include "pch.h"
#include "VulkanContext.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace VulkanTestbed
{
	static VkInstance s_VkInstance = nullptr;
	static VkPhysicalDevice s_PhysicalDevice = nullptr;
	static VkDevice s_LogicalDevice = nullptr;
	static VkQueue s_GraphicsQueue = nullptr;
	static VkDebugReportCallbackEXT s_DebugReport = nullptr;

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

	void VulkanContext::Init()
	{
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan Testbed";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "NA";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

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

		uint32_t graphicsFamily = 0;
		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				graphicsFamily = i;
				break;
			}
			i++;
		}

		{
			const float queuePriorities[] = { 1.0f };
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = graphicsFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = queuePriorities;

			VkPhysicalDeviceFeatures deviceFeatures = {};

			VkDeviceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.pQueueCreateInfos = &queueCreateInfo;
			createInfo.queueCreateInfoCount = 1;
			createInfo.pEnabledFeatures = &deviceFeatures;
			createInfo.enabledExtensionCount = 0;
#ifdef TESTBED_DEBUG
			createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();
#else
			createInfo.enabledLayerCount = 0;
#endif
			VkResult result = vkCreateDevice(s_PhysicalDevice, &createInfo, nullptr, &s_LogicalDevice);
			TESTBED_ASSERT(result == VK_SUCCESS);

			vkGetDeviceQueue(s_LogicalDevice, graphicsFamily, 0, &s_GraphicsQueue);
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
	}

	void VulkanContext::Shutdown()
	{
		vkDestroyDevice(s_LogicalDevice, nullptr);

#ifdef TESTBED_DEBUG
		// Remove the debug report callback
		auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(s_VkInstance, "vkDestroyDebugReportCallbackEXT");
		vkDestroyDebugReportCallbackEXT(s_VkInstance, s_DebugReport, nullptr);
#endif

		vkDestroyInstance(s_VkInstance, nullptr);
	}
}
