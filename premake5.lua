workspace "Vulkan"
	configurations {"Debug", "Release"}
	--prebuildcommands { 'path "($SolutionDir)../compile-shaders.bat"' }
	architecture "x86_64"
	location "Build"
	cppdialect "C++17"


project "Test"
	location "Build"
	kind "StaticLib"
	language "C++"
	targetname "Test_%{cfg.buildcfg}"
	targetdir "Test/Lib"
	objdir "Build/Intermediate/Test/%{cfg.buildcfg}" -- todo add project. eg. intermediate/project/config/

	includedirs {
		"Test/Include/Test/",
	}
	files {
		"Test/**.h",
		"Test/**.cpp"
	}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"
		--links { "glfw3_x64_debug.lib", "vulkan-1.lib", "assimp-vc142-mtd.lib", "IrrXMLd.lib", "zlibstaticd.lib", }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
		--links { "glfw3_x64_release.lib", "vulkan-1.lib", "assimp-vc142-mt.lib", "IrrXML.lib", "zlibstatic.lib", }


-- TODO Fix linking
-- TODO Project dependencies?


project "Renderer"
	location "Build"
	kind "ConsoleApp"
	language "C++"
	targetname "Flux_%{cfg.buildcfg}"
	targetdir "Bin"
	objdir "Build/Intermediate/Renderer/%{cfg.buildcfg}"
	dependson { "Test", }

	includedirs {
		"Source/", -- for <app/*.h> <renderer/*.h> <shared/*.h> includes
		"External/assimp/include",
		"External/glfw/include",
		"External/glm/include",
		"External/stbi/include",
		"External/vulkan/include",
		"External/tinyfiledialogs/",
		"External/imgui/",
		"Test/Include/",
	}

	libdirs { 
		"External/assimp/lib",
		"External/glfw/lib",
		"External/vulkan/lib",
		"Test/Lib/"
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
		links { "glfw3_x64_debug.lib", "vulkan-1.lib", "assimp-vc142-mtd.lib", "IrrXMLd.lib", "zlibstaticd.lib", "Test_Debug.lib", }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
		links { "glfw3_x64_release.lib", "vulkan-1.lib", "assimp-vc142-mt.lib", "IrrXML.lib", "zlibstatic.lib", "Test_Release.lib",}
