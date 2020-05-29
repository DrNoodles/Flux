
#include "GlfwWindow.h"

#include <stbi/stb_image.h> 

inline std::unordered_map<GLFWwindow*, GlfwWindow*> g_window_map;

GlfwWindow::GlfwWindow()
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

GlfwWindow::~GlfwWindow()
{
	// RAII
	glfwDestroyWindow(_window);
	glfwTerminate();
}

void GlfwWindow::SetIcon(const std::string& path)
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

GLFWwindow* GlfwWindow::GetGlfwWindow() const
{
	return _window;
}

Extent2D GlfwWindow::GetSize()
{
	return _size;
}

Extent2D GlfwWindow::GetFramebufferSize()
{
	i32 width, height;
	glfwGetFramebufferSize(_window, &width, &height);
	return {(u32)width, (u32)height};
}

Extent2D GlfwWindow::WaitTillFramebufferHasSize()
{
	// This handles a minimized window. Wait until it has size > 0
	i32 width, height;
	glfwGetFramebufferSize(_window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(_window, &width, &height);
		glfwWaitEvents();
	}

	return {(u32)width, (u32)height};
}

bool GlfwWindow::CloseRequested()
{
	return glfwWindowShouldClose(_window);
}

void GlfwWindow::PollEvents()
{
	glfwPollEvents();
}

void GlfwWindow::SetWindowTitle(const std::string& title)
{
	glfwSetWindowTitle(_window, title.c_str());
}

MouseButtonAction GlfwWindow::GetMouseButton(MouseButton button)
{
	const auto glfwButton = ToGlfwButton(button);
	const auto glfwButtonState = glfwGetMouseButton(_window, glfwButton);
	return ToMouseButtonState(glfwButtonState);
}

KeyAction GlfwWindow::GetKey(VirtualKey k)
{
	const auto glfwKey = ToGlfwKey(k);
	const auto glfwAction = glfwGetKey(_window, glfwKey);
	return ToKeyAction(glfwAction);
}

void GlfwWindow::OnWindowSizeChanged(i32 width, i32 height)
{
	_size = Extent2D{(u32)width, (u32)height};
	WindowSizeChanged.Invoke(this, WindowSizeChangedEventArgs{_size});
}

void GlfwWindow::OnKeyCallback(int key, int scancode, int action, int mods)
{
	const auto args = KeyEventArgs{
		ToKey(key),
		ToKeyAction(action),
		ToKeyModifiers(mods),
		(u32)scancode
	};

	if (args.Action == KeyAction::Pressed || args.Action == KeyAction::Repeat)
	{
		KeyDown.Invoke(this, args);
	}
	else if (args.Action == KeyAction::Released)
	{
		KeyUp.Invoke(this, args);
	}
	else
	{
		throw std::invalid_argument("Unhandled key state");
	}
}

void GlfwWindow::OnCursorPosChanged(f64 xPos, f64 yPos)
{
	_mousePos = Point2D{xPos, yPos};

	const auto args = PointerEventArgs{PointerPoint{_mousePos}};
	PointerMoved.Invoke(this, args);
}

void GlfwWindow::OnScrollChanged(f64 xOffset, f64 yOffset)
{
	const auto isHorizontal = abs(xOffset) > 0.0001;
	const auto props = PointerPointProperties{isHorizontal, isHorizontal ? xOffset : yOffset};
	const auto pointerPoint = PointerPoint{_mousePos, props};
	const auto args = PointerEventArgs{pointerPoint};
	PointerWheelChanged.Invoke(this, args);
}

void GlfwWindow::ScrollCallback(GLFWwindow* window, f64 xOffset, f64 yOffset)
{
	g_window_map[window]->OnScrollChanged(xOffset, yOffset);
}

void GlfwWindow::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	g_window_map[window]->OnKeyCallback(key, scancode, action, mods);
}

void GlfwWindow::CursorPosCallback(GLFWwindow* window, double xPos, double yPos)
{
	g_window_map[window]->OnCursorPosChanged(xPos, yPos);
}

void GlfwWindow::WindowSizeCallback(GLFWwindow* window, int width, int height)
{
	g_window_map[window]->OnWindowSizeChanged(width, height);
}

void GlfwWindow::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	//printf("TODO Handle FramebufferSizeCallback()\n");
}

MouseButtonAction GlfwWindow::ToMouseButtonState(i32 glfwButtonState)
{
	switch (glfwButtonState)
	{
	case 0: return MouseButtonAction::Released;
	case 1: return MouseButtonAction::Pressed;
	default:
		throw std::out_of_range("Unhandled glfwButtonState");
	}
}

i32 GlfwWindow::ToGlfwButtonState(MouseButtonAction mouseButtonState)
{
	switch (mouseButtonState)
	{
	case MouseButtonAction::Released: return 0;
	case MouseButtonAction::Pressed: return 1;
	default:
		throw std::out_of_range("Unhandled mouseButtonState");
	}
}

KeyAction GlfwWindow::ToKeyAction(i32 glfwKeyAction)
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

i32 GlfwWindow::ToGlfwKeyAction(KeyAction keyState)
{
	switch (keyState)
	{
	case KeyAction::Released: return 0;
	case KeyAction::Pressed: return 1;
	case KeyAction::Repeat: return 2;
	default:
		throw std::out_of_range("Unhandled keyState");
	}
}

MouseButton GlfwWindow::ToMouseButton(i32 glfwButton)
{
	return (MouseButton)glfwButton;
}

i32 GlfwWindow::ToGlfwButton(MouseButton button)
{
	return (i32)button;
}

VirtualKey GlfwWindow::ToKey(i32 glfwKey)
{
	return (VirtualKey)glfwKey;
}

i32 GlfwWindow::ToGlfwKey(VirtualKey virtualKey)
{
	return (i32)virtualKey;
}

VirtualKeyModifiers GlfwWindow::ToKeyModifiers(i32 glfwMods)
{
	const auto ctrl = GLFW_MOD_CONTROL & glfwMods;
	const auto shift = GLFW_MOD_SHIFT & glfwMods;
	const auto alt = GLFW_MOD_ALT & glfwMods;
	return VirtualKeyModifiers(ctrl, shift, alt);
}

i32 GlfwWindow::ToGlfwKeyModifiers(VirtualKeyModifiers vkm)
{
	i32 mods = 0;
	mods |= vkm.Control ? GLFW_MOD_CONTROL : 0;
	mods |= vkm.Alt ? GLFW_MOD_ALT : 0;
	mods |= vkm.Shift ? GLFW_MOD_SHIFT : 0;
	return mods;
}
