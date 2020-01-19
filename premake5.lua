workspace "Vulkan"
	configurations {"Debug", "Release"}
	--prebuildcommands { 'path "($SolutionDir)../compile-shaders.bat"' }
	architecture "x86_64"
	location "Build"
	cppdialect "C++17"
	
project "Renderer"
	location "Build"
	kind "ConsoleApp"
	language "C++"
	targetdir "Bin/%{cfg.buildcfg}"
	objdir "Build/Intermediate/%{cfg.buildcfg}"

	includedirs {
		"Source/", -- for <app/*.h> <renderer/*.h> <shared/*.h> includes
		"External/assimp/include",
		"External/glfw/include",
		"External/glm/include",
		"External/stbi/include",
		"External/vulkan/include",
		"External/tinyfiledialogs/",
		"External/imgui/",
	}

	libdirs { 
		"External/assimp/lib",
		"External/glfw/lib",
		"External/vulkan/lib",
	}

	files {
		"Source/**.h",
		"Source/**.cpp",
		"Source/**.frag",
		"Source/**.vert",
		"External/stbi/src/stb_image.cpp",
		"External/tinyfiledialogs/**",
		"External/imgui/**",
		--"External/imgui-filebrowser/**",
	}

	excludes { 
		"External/imgui/imgui/main_vulkan.cpp",
	}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"
		links { "glfw3_x64_debug.lib", "vulkan-1.lib", "assimp-vc142-mtd.lib", "IrrXMLd.lib", "zlibstaticd.lib", }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
		links { "glfw3_x64_release.lib", "vulkan-1.lib", "assimp-vc142-mt.lib", "IrrXML.lib", "zlibstatic.lib", }
