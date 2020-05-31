#pragma once

#include "IWindow.h"

#include "Framework/CommonTypes.h"
#include "Renderer/VulkanService.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN // glfw includes vulkan.h
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <stdexcept>

class GlfwWindow;

class GlfwVkSurfaceBuilder final : public ISurfaceBuilder
{
private:
	GLFWwindow* _window = nullptr;
	
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

};


class GlfwWindow final : public IWindow
{
public:  // Data

	
private: // Data
	GLFWwindow* _window = nullptr;
	Extent2D _size = { 1600,900 };
	Point2D _mousePos = {};

	
public:  // Methods
	GlfwWindow();
	GlfwWindow(const GlfwWindow&) = delete;
	GlfwWindow(GlfwWindow&&) = delete;
	GlfwWindow& operator=(const GlfwWindow&) = delete;
	GlfwWindow& operator=(GlfwWindow&&) = delete;
	~GlfwWindow();

	GLFWwindow* GetGlfwWindow() const;


	inline Extent2D GetSize() override;
	Extent2D GetFramebufferSize() override;
	Extent2D WaitTillFramebufferHasSize() override;
	inline bool CloseRequested() override;
	inline void PollEvents() override;
	inline void SetWindowTitle(const std::string& title) override;
	void SetIcon(const std::string& path) override;
	MouseButtonAction GetMouseButton(MouseButton button) override;
	KeyAction GetKey(VirtualKey k) override;

	
private: // Methods

	// Callback routers
	void OnWindowSizeChanged(i32 width, i32 height);
	void OnKeyCallback(int key, int scancode, int action, int mods);
	void OnCursorPosChanged(f64 xPos, f64 yPos);
	void OnScrollChanged(f64 xOffset, f64 yOffset);

	// Callbacks
	static void ScrollCallback(GLFWwindow* window, f64 xOffset, f64 yOffset);
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void CursorPosCallback(GLFWwindow* window, double xPos, double yPos);
	static void WindowSizeCallback(GLFWwindow* window, int width, int height);
	static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
	
	// Type Conversions: IWindow <-> GLFW
	static MouseButtonAction ToMouseButtonState(i32 glfwButtonState);
	static i32 ToGlfwButtonState(MouseButtonAction mouseButtonState);

	static KeyAction ToKeyAction(i32 glfwKeyAction);
	static i32 ToGlfwKeyAction(KeyAction keyState);

	static MouseButton ToMouseButton(i32 glfwButton);
	static i32 ToGlfwButton(MouseButton button);

	static VirtualKey ToKey(i32 glfwKey);
	static i32 ToGlfwKey(VirtualKey virtualKey);

	static VirtualKeyModifiers ToKeyModifiers(i32 glfwMods);
	static i32 ToGlfwKeyModifiers(VirtualKeyModifiers vkm);
};
