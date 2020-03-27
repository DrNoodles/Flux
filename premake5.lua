
-------------------------------------------------------------------------------
workspace "Flux"
	configurations {"Debug", "Release"}
	--prebuildcommands { 'path "($SolutionDir)../compile-shaders.bat"' }
	architecture "x86_64"
	location "Build"
	cppdialect "C++17"
	startproject "App"


-------------------------------------------------------------------------------
project "Framework"
	location "Build"
	kind "StaticLib"
	language "C++"
	targetname "Framework_%{cfg.buildcfg}"
	targetdir "Projects/Framework/Lib"
	objdir "Build/Intermediate/Framework/%{cfg.buildcfg}"

	includedirs {
		"Projects/Framework/Include/Framework/",
		"External/tinyfiledialogs/",
	}
	files {
		"Projects/Framework/Include/Framework/**.h",
		"Projects/Framework/Source/**.h",
		"Projects/Framework/Source/**.cpp",
		"External/tinyfiledialogs/tinyfiledialogs/tinyfiledialogs.c",
	}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"


-------------------------------------------------------------------------------
project "State"
	dependson { "Framework", }
	location "Build"
	kind "StaticLib"
	language "C++"
	targetname "State_%{cfg.buildcfg}"
	targetdir "Projects/State/Lib"
	objdir "Build/Intermediate/State/%{cfg.buildcfg}"

	includedirs {
		"Projects/State/Include/State/",
		"Projects/Framework/Include/",
		"External/glm/include",
	}
	files {
		"Projects/State/Include/State/**.h",
		"Projects/State/Source/**.h",
		"Projects/State/Source/**.cpp",
	}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"

-------------------------------------------------------------------------------
project "App"
	dependson { "Framework", "State", }
	location "Build"
	kind "ConsoleApp"
	language "C++"
	targetname "Flux_%{cfg.buildcfg}"
	targetdir "Bin"
	objdir "Build/Intermediate/App/%{cfg.buildcfg}"

	includedirs {
		"Projects/App",
		"Projects/Framework/Include/",
		"Projects/State/Include/",

		"External/assimp/include",
		"External/glfw/include",
		"External/glm/include",
		"External/stbi/include",
		"External/vulkan/include",
		"External/imgui/",
	}

	libdirs { 
		"Projects/Framework/Lib/",
		"Projects/State/Lib/",

		"External/assimp/lib",
		"External/glfw/lib",
		"External/vulkan/lib",
	}

	files {
		"Projects/App/**.h",
		"Projects/App/**.cpp",
		"Projects/App/Renderer/**.frag",
		"Projects/App/Renderer/**.vert",

		"External/stbi/src/stb_image.cpp",
		"External/imgui/**",
		--"External/imgui-filebrowser/**",
	}

	excludes { 
		"External/imgui/imgui/main_vulkan.cpp",
	}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"
		links { 
			"glfw3_x64_debug.lib", 
			"vulkan-1.lib", 
			"assimp-vc142-mtd.lib", 
			"IrrXMLd.lib", 
			"zlibstaticd.lib", 
			"State_Debug.lib", 
			"Framework_Debug.lib", }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
		links { 
			"glfw3_x64_release.lib", 
			"vulkan-1.lib", 
			"assimp-vc142-mt.lib", 
			"IrrXML.lib", 
			"zlibstatic.lib", 
			"State_Release.lib", 
			"Framework_Release.lib", }


