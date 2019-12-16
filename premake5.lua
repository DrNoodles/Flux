workspace "Vulkan"
	configurations {"Debug", "Release"}
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
		"External/assimp/include",
		"External/glad/include",
		"External/glfw/include",
		"External/glm/include",
		"External/stbi/include",
	}

	libdirs { 
		"External/assimp/lib",
		"External/glfw/lib",
	}

	files {
		"Source/**.h",
		"Source/**.cpp",
		"Source/**.frag",
		"Source/**.vert",
		"External/glad/src/glad.c",
		"External/stbi/src/stb_image.cpp",
		"External/imgui/**",
		"External/tinyfiledialogs/**",
		"External/imgui-filebrowser/**",
	}
	excludes { "External/imgui/main.cpp" }

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"
		links { "assimp-vc142-mtd.lib", "IrrXMLd.lib", "zlibstaticd.lib", "glfw3_x64_debug.lib", }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
		links { "assimp-vc142-mt.lib", "IrrXML.lib", "zlibstatic.lib", "glfw3_x64_release.lib", }
