
-------------------------------------------------------------------------------
workspace "Flux"
	configurations {"Debug", "Release"}
	--prebuildcommands { 'path "($SolutionDir)../compile-shaders.bat"' }
	architecture "x86_64"
	location "Build"
	cppdialect "c++latest"
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

	libdirs { 
		"Projects/Framework/Lib/",
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
project "Renderer"
	dependson { "Framework", }
	location "Build"
	kind "StaticLib"
	language "C++"
	targetname "Renderer_%{cfg.buildcfg}"
	targetdir "Projects/Renderer/Lib"
	objdir "Build/Intermediate/Renderer/%{cfg.buildcfg}"

	includedirs {
		--"Projects/Renderer/Source/",
		"Projects/Renderer/Include/",
		"Projects/Framework/Include/",
		"External/glfw/include",
		"External/glm/include",
		"External/stbi/include",
		"External/vulkan/include",
		"External/imgui/",
	}

	libdirs { 
		"Projects/Framework/Lib/",
		"External/glfw/lib",
		"External/vulkan/lib",
	}

	files {
		"Projects/Renderer/Include/Renderer/**.h",
		"Projects/Renderer/Include/Renderer/**.cpp",
		"Projects/Renderer/Source/**.h",
		"Projects/Renderer/Source/**.cpp",
		"Projects/Renderer/Source/**.frag",
		"Projects/Renderer/Source/**.vert",

		"External/stbi/src/stb_image.cpp",
		"External/imgui/**",
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
			--"IrrXMLd.lib", zlibstaticd.lib", State_Debug.lib", 
			"Framework_Debug.lib", }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
		links { 
			"glfw3_x64_release.lib", 
			"vulkan-1.lib", 
			--"assimp-vc142-mt.lib", "IrrXML.lib", "zlibstatic.lib", 
			"Framework_Release.lib", }


-------------------------------------------------------------------------------
project "App"
	dependson { "Framework", "State", "Renderer" }
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
		"Projects/Renderer/Include/",

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
		"Projects/Renderer/Lib/",

		"External/assimp/lib",
		"External/glfw/lib",
		"External/vulkan/lib",
	}

	files {
		"Projects/App/**.h",
		"Projects/App/**.cpp",

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
			"assimp-vc142-mtd.lib", "IrrXMLd.lib", "zlibstaticd.lib", 
			"State_Debug.lib", 
			"Renderer_Debug.lib", 
			"Framework_Debug.lib", }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
		links { 
			"glfw3_x64_release.lib", 
			"vulkan-1.lib", 
			"assimp-vc142-mt.lib", "IrrXML.lib", "zlibstatic.lib", 
			"State_Release.lib", 
			"Renderer_Release.lib", 
			"Framework_Release.lib", }


