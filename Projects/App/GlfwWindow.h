#pragma once


#include "IWindow.h"

#include "Framework/CommonTypes.h"
#include "Renderer/VulkanService.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN // glfw includes vulkan.h
#include <GLFW/glfw3.h>
#include <stbi/stb_image.h>
#include <vulkan/vulkan.h>

#include <stdexcept>
#include <unordered_map>


class GlfwWindow;
inline std::unordered_map<GLFWwindow*, GlfwWindow*> g_window_map;


class GlfwVkSurfaceBuilder final : public ISurfaceBuilder
{
public:
	explicit GlfwVkSurfaceBuilder(GLFWwindow* window)
		: _window(window)
	{
	}

	VkSurfaceKHR CreateSurface(VkInstance instance) override
	{
		VkSurfaceKHR surface;
		if (glfwCreateWindowSurface(instance, _window, nullptr, &surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface");
		}
		return surface;
	}
	
private:
	GLFWwindow* _window = nullptr;
};


class GlfwWindow final : public IWindow
{
public:  // Data
private: // Data
	GLFWwindow* _window = nullptr;
	Extent2D _size = { 1600,900 };
	
	
public:  // Methods
	GlfwWindow() = default;

	~GlfwWindow()
	{
		// RAII
		glfwDestroyWindow(_window);
		glfwTerminate();
	}

	void InitWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't use opengl

		GLFWwindow* window = glfwCreateWindow(_size.Width, _size.Height, "Vulkan", nullptr, nullptr);
		if (window == nullptr)
		{
			throw std::runtime_error("Failed to create GLFWwindow");
		}
		_window = window;
		g_window_map.insert(std::make_pair(_window, this));

		//glfwSetWindowUserPointer(_window, this);
		
		glfwSetWindowSizeCallback(_window, WindowSizeCallback);
		glfwSetFramebufferSizeCallback(_window, FramebufferSizeCallback);
		glfwSetKeyCallback(_window, KeyCallback);
		glfwSetCursorPosCallback(_window, CursorPosCallback);
		glfwSetScrollCallback(_window, ScrollCallback);
	}

	void SetIcon(const std::string& path)
	{
		int outChannelsInFile;
		GLFWimage icon;
		icon.pixels = stbi_load(path.c_str(), &icon.width, &icon.height, &outChannelsInFile, 4);
		if (!icon.pixels)
		{
			stbi_image_free(icon.pixels);
			throw std::runtime_error("Failed to load texture image: " + path);
		}
		
		glfwSetWindowIcon(_window, 1, &icon);
		
		stbi_image_free(icon.pixels);
	}

	Extent2D GetSize() override
	{
		return _size;
	}
	
	Extent2D GetFramebufferSize() override
	{
		i32 width, height;
		glfwGetFramebufferSize(_window, &width, &height);
		return { (u32)width, (u32)height };
	}
	
	Extent2D WaitTillFramebufferHasSize() override
	{
		// This handles a minimized window. Wait until it has size > 0
		i32 width, height;
		glfwGetFramebufferSize(_window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(_window, &width, &height);
			glfwWaitEvents();
		}

		return { (u32)width, (u32)height };
	}

	
	bool CloseRequested() override
	{
		return glfwWindowShouldClose(_window);
	}

	void PollEvents() override
	{
		glfwPollEvents();
	}

	void SetWindowTitle(const std::string& title) override
	{
		glfwSetWindowTitle(_window, title.c_str());
	}

	GLFWwindow* GetGlfwWindow() const
	{
		return _window;
	}

	
private: // Methods


	
	#pragma region GLFW Callbacks, event handling

	void OnWindowSizeChanged(i32 width, i32 height)
	{
		_size = Extent2D{ (u32)width, (u32)height };
		WindowSizeChanged.Invoke(this, WindowSizeChangedEventArgs{_size});
	}
	
	void OnKeyCallback(int key, int scancode, int action, int mods)
	{
		const auto args = KeyEventArgs{
			ToKey(key), 
			ToKeyAction(action), 
			ToKeyModifiers(mods), 
			(u32)scancode};

		if (args.Action == KeyAction::Pressed || args.Action == KeyAction::Repeat) {
			KeyDown.Invoke(this, args);
		}
		else if (args.Action == KeyAction::Released) {
			KeyUp.Invoke(this, args);
		}
		else {
			throw std::invalid_argument("Unhandled key state");
		}
	}

	Point2D _mousePos;
	
	void OnCursorPosChanged(f64 xPos, f64 yPos)
	{
		_mousePos = Point2D{xPos, yPos};
		
		const auto args = PointerEventArgs{PointerPoint{_mousePos}};
		PointerMoved.Invoke(this, args);
	}
	
	void OnScrollChanged(f64 xOffset, f64 yOffset)
	{
		const auto isHorizontal = abs(xOffset) > 0.0001;
		const auto props = PointerPointProperties{ isHorizontal, isHorizontal ? xOffset : yOffset };
		const auto pointerPoint = PointerPoint{ _mousePos, props };
		const auto args = PointerEventArgs{ pointerPoint };
		PointerWheelChanged.Invoke(this, args);
	}

	// Callbacks
	static void ScrollCallback(GLFWwindow* window, f64 xOffset, f64 yOffset)
	{
		g_window_map[window]->OnScrollChanged(xOffset, yOffset);
	}
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		g_window_map[window]->OnKeyCallback(key, scancode, action, mods);
	}
	static void CursorPosCallback(GLFWwindow* window, double xPos, double yPos)
	{
		g_window_map[window]->OnCursorPosChanged(xPos, yPos);
	}
	static void WindowSizeCallback(GLFWwindow* window, int width, int height)
	{
		g_window_map[window]->OnWindowSizeChanged(width, height);
	}
	static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
	{
		//printf("TODO Handle FramebufferSizeCallback()\n");
	}

public:

	MouseButtonAction GetMouseButton(MouseButton button) override
	{
		const auto glfwButton = ToGlfwButton(button);
		const auto glfwButtonState = glfwGetMouseButton(_window, glfwButton);
		return ToMouseButtonState(glfwButtonState);
	}

	KeyAction GetKey(VirtualKey k) override
	{
		const auto glfwKey = ToGlfwKey(k);
		const auto glfwAction = glfwGetKey(_window, glfwKey);
		return ToKeyAction(glfwAction);
	}
private:

	// Conversions
	static MouseButtonAction ToMouseButtonState(i32 glfwButtonState)
	{
		switch (glfwButtonState)
		{
		case 0: return MouseButtonAction::Released;
		case 1: return MouseButtonAction::Pressed;
		default:
			throw std::out_of_range("Unhandled glfwButtonState");
		}
	}
	static i32 ToGlfwButtonState(MouseButtonAction mouseButtonState)
	{
		switch (mouseButtonState)
		{
		case MouseButtonAction::Released: return 0;
		case MouseButtonAction::Pressed:  return 1;
		default:
			throw std::out_of_range("Unhandled mouseButtonState");
		}
	}

	static KeyAction ToKeyAction(i32 glfwKeyAction)
	{
		switch (glfwKeyAction)
		{
		case 0: return KeyAction::Released;
		case 1: return KeyAction::Pressed;
		case 2: return KeyAction::Repeat;
		default:
			throw std::out_of_range("Unhandled glfwKeyState");
		}
	}
	static i32 ToGlfwKeyAction(KeyAction keyState)
	{
		switch (keyState)
		{
		case KeyAction::Released: return 0;
		case KeyAction::Pressed:  return 1;
		case KeyAction::Repeat:   return 2;
		default:
			throw std::out_of_range("Unhandled keyState");
		}
	}
	
	static MouseButton ToMouseButton(i32 glfwButton)
	{
		return (MouseButton)glfwButton;
	}
	static i32 ToGlfwButton(MouseButton button)
	{
		return (i32)button;
	}
	
	static VirtualKey ToKey(i32 glfwKey) 
	{
		return (VirtualKey)glfwKey;
	}
	static i32 ToGlfwKey(VirtualKey virtualKey)
	{
		return (i32)virtualKey;
	}

	static VirtualKeyModifiers ToKeyModifiers(i32 glfwMods)
	{
		auto ctrl = GLFW_MOD_CONTROL & glfwMods;
		auto shift = GLFW_MOD_SHIFT & glfwMods;
		auto alt = GLFW_MOD_ALT & glfwMods;
		return VirtualKeyModifiers(ctrl, shift, alt);
	}
	static i32 ToGlfwKeyModifiers(VirtualKeyModifiers vkm)
	{
		i32 mods = 0;
		mods |= vkm.Control ? GLFW_MOD_CONTROL : 0;
		mods |= vkm.Alt     ? GLFW_MOD_ALT     : 0;
		mods |= vkm.Shift   ? GLFW_MOD_SHIFT   : 0;
		return mods;
	}
};
