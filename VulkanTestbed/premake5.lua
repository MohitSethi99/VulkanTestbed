project "VulkanTestbed"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "pch.h"
	pchsource "src/pch.cpp"

	files
	{
		"src/**.h",
		"src/**.cpp",
		"vendor/stb_image/**.h",
		"vendor/stb_image/**.cpp",
		"vendor/glm/glm/**.hpp",
		"vendor/glm/glm/**.inl",

		"%{IncludeDir.glm}/util/glm.natvis",
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE"
	}

	includedirs
	{
		"src",
		"vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.tinyobj}",
		"%{IncludeDir.vulkan}",
	}

	links
	{
		"GLFW",

		"%{Lib.vulkan}"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "TESTBED_DEBUG"
		runtime "Debug"
		symbols "on"
		postbuildcommands
		{
			'{COPY} "%{Lib.vulkan}" "%{cfg.targetdir}"'
		}

	filter "configurations:Release"
		defines "TESTBED_RELEASE"
		runtime "Release"
		optimize "speed"
		postbuildcommands
		{
			'{COPY} "%{Lib.vulkan}" "%{cfg.targetdir}"'
		}

	filter "configurations:Dist"
		defines "TESTBED_DIST"
		runtime "Release"
		optimize "speed"
		postbuildcommands
		{
			'{COPY} "%{Lib.vulkan}" "%{cfg.targetdir}"'
		}
