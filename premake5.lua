workspace "VulkanTestbed"
	architecture "x86_64"
	startproject "VulkanTestbed"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}";

-- Include directories relavtive to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "%{wks.location}/VulkanTestbed/vendor/GLFW/include"
IncludeDir["glm"] = "%{wks.location}/VulkanTestbed/vendor/glm"
IncludeDir["stb_image"] = "%{wks.location}/VulkanTestbed/vendor/stb_image"
IncludeDir["tinyobj"] = "%{wks.location}/VulkanTestbed/vendor/tinyobj"
IncludeDir["vulkan"] = "%VULKAN_SDK%/include"

LibDir = {}
LibDir["Vulkan"] = "%VULKAN_SDK%/Lib"

Lib = {}
Lib["vulkan"] = "%{LibDir.Vulkan}/vulkan-1.lib"

group "Dependencies"
	include "VulkanTestbed/vendor/GLFW"

group ""

include "VulkanTestbed"
