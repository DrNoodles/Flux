#pragma once

#include <Framework/CommonTypes.h>
#include <Framework/Event.h>

#include <string>

enum class VirtualKey
{
	/* The unknown key */
	Unknown            = -1,
											 
	/* Printable keys */				 
	Space              = 32,
	Apostrophe         = 39,  /* ' */
	Comma              = 44,  /* , */
	Minus              = 45,  /* - */
	Period             = 46,  /* . */
	Slash              = 47,  /* / */
	Number0            = 48,
	Number1            = 49,
	Number2            = 50,
	Number3            = 51,
	Number4            = 52,
	Number5            = 53,
	Number6            = 54,
	Number7            = 55,
	Number8            = 56,
	Number9            = 57,
	Semicolon          = 59,  /* ; */
	Equal              = 61,  /* = */
	A                  = 65,
	B                  = 66,
	C                  = 67,
	D                  = 68,
	E                  = 69,
	F                  = 70,
	G                  = 71,
	H                  = 72,
	I                  = 73,
	J                  = 74,
	K                  = 75,
	L                  = 76,
	M                  = 77,
	N                  = 78,
	O                  = 79,
	P                  = 80,
	Q                  = 81,
	R                  = 82,
	S                  = 83,
	T                  = 84,
	U                  = 85,
	V                  = 86,
	W                  = 87,
	X                  = 88,
	Y                  = 89,
	Z                  = 90,
	LeftBracket        = 91,  /* [ */
	Backslash          = 92,  /* \ */
	RightBracket       = 93,  /* ] */
	GraveAccent        = 96,  /* ` */
	World1             = 161, /* non-US #1 */
	World2             = 162, /* non-US #2 */

	/* Function keys */
	Escape             = 256,
	Enter              = 257,
	Tab                = 258,
	Backspace          = 259,
	Insert             = 260,
	Delete             = 261,
	Right              = 262,
	Left               = 263,
	Down               = 264,
	Up                 = 265,
	PageUp            = 266,
	PageDown          = 267,
	Home               = 268,
	End                = 269,
	CapsLock          = 280,
	ScrollLock        = 281,
	NumLock           = 282,
	PrintScreen       = 283,
	Pause              = 284,
	F1                 = 290,
	F2                 = 291,
	F3                 = 292,
	F4                 = 293,
	F5                 = 294,
	F6                 = 295,
	F7                 = 296,
	F8                 = 297,
	F9                 = 298,
	F10                = 299,
	F11                = 300,
	F12                = 301,
	F13                = 302,
	F14                = 303,
	F15                = 304,
	F16                = 305,
	F17                = 306,
	F18                = 307,
	F19                = 308,
	F20                = 309,
	F21                = 310,
	F22                = 311,
	F23                = 312,
	F24                = 313,
	F25                = 314,
	NumberPad0         = 320,
	NumberPad1         = 321,
	NumberPad2         = 322,
	NumberPad3         = 323,
	NumberPad4         = 324,
	NumberPad5         = 325,
	NumberPad6         = 326,
	NumberPad7         = 327,
	NumberPad8         = 328,
	NumberPad9         = 329,
	NumberPadDecimal   = 330,
	NumberPadDivide    = 331,
	NumberPadMultiply  = 332,
	NumberPadSubtract  = 333,
	NumberPadAdd       = 334,
	NumberPadEnter     = 335,
	NumberPadEqual     = 336,
	LeftShift          = 340,
	LeftControl        = 341,
	LeftAlt            = 342,
	LeftSuper          = 343,
	RightShift         = 344,
	RightControl       = 345,
	RightAlt           = 346,
	RightSuper         = 347,
	Menu               = 348,
	
	Last               = Menu,
};

enum class KeyAction
{
	Released = 0,
	Pressed  = 1,
	Repeat   = 2,
};

enum class MouseButton
{
	Button1 = 0,
	Button2 = 1,
	Button3 = 2,
	Button4 = 3,
	Button5 = 4,
	Button6 = 5,
	Button7 = 6,
	Button8 = 7,
	Last    = Button8,
	Left    = Button1,
	Right   = Button2,
	Middle  = Button3,
};

enum class MouseButtonAction
{
	Released = 0,
	Pressed  = 1,
};

struct VirtualKeyModifiers
{
	//const bool None;
	const bool Control;
	const bool Shift;
	const bool Alt;

	VirtualKeyModifiers(bool ctrl, bool shift, bool alt)
		: /*None(ctrl || shift || alt), */Control(ctrl), Shift(shift), Alt(alt)
	{
	}
};

struct KeyEventArgs
{
	// Gets the virtual key that maps to the key that was pressed.
	const VirtualKey Key;
	
	// Gets any virtual modifier keys that maps to any keys that were pressed.
	const VirtualKeyModifiers Modifiers;

	const KeyAction Action;
	
	// The scan code for a key that was pressed.
	const u32 ScanCode;

	KeyEventArgs(VirtualKey key, KeyAction action, VirtualKeyModifiers mods, u32 scanCode)
		: Key(key), Modifiers(mods), Action(action), ScanCode(scanCode)
	{
	}
};

enum class PointerUpdateKind
{
	Other                = 0,
	LeftButtonPressed    = 1,
	LeftButtonReleased   = 2,
	RightButtonPressed   = 3,
	RightButtonReleased  = 4,
	MiddleButtonPressed  = 5,
	MiddleButtonReleased = 6,
	XButton1Pressed      = 7,
	XButton1Released     = 8,
	XButton2Pressed      = 9,
	XButton2Released     = 10,
};

struct PointerPointProperties
{
	/*bool IsLeftButtonPressed = false;
	bool IsMiddleButtonPressed = false;
	bool IsRightButtonPressed = false;
	bool IsXButton1Pressed = false;
	bool IsXButton2Pressed = false;
	
	PointerUpdateKind PointerUpdateKind = PointerUpdateKind::Other;*/

	bool IsHorizonalMouseWheel = false;
	f64 MouseWheelDelta = 0; // https://docs.microsoft.com/en-us/uwp/api/windows.ui.input.pointerpointproperties.mousewheeldelta?view=winrt-19041#Windows_UI_Input_PointerPointProperties_MouseWheelDelta
};

struct PointerPoint
{
	// Gets the location of the pointer input in client coordinates.
	//TODO Should this be DIP(device-independent pixels)? Figure out if needed with ImGui/Glfw.
	Point2D Position;

	// The time, relative to the system boot time, in microseconds. -
	// TODO Confirm i can calculate this or some equivalent
	//u64 Timestamp;

	// Gets extended information about the input pointer.
	PointerPointProperties Properties;
};

struct PointerEventArgs
{
	// Gets the pointer data of the last pointer event.
	PointerPoint CurrentPoint;
	
	// Gets any virtual modifier keys that maps to any keys that were pressed.
	//const VirtualKeyModifiers Modifiers;

	explicit PointerEventArgs(PointerPoint currentPoint)
		: CurrentPoint(currentPoint)
	{
	}
};
struct WindowSizeChangedEventArgs
{
	Extent2D Size = {};
};

class IWindow
{
public:
	virtual ~IWindow() = default;
	
	virtual bool CloseRequested() = 0;
	virtual void PollEvents() = 0;
	virtual void SetWindowTitle(const std::string& title) = 0;
	virtual Extent2D GetSize() = 0;
	virtual Extent2D GetFramebufferSize() = 0;
	virtual Extent2D WaitTillFramebufferHasSize() = 0;
	virtual void SetIcon(const std::string& path) = 0;
	virtual MouseButtonAction GetMouseButton(MouseButton b) = 0;
	virtual KeyAction GetKey(VirtualKey k) = 0;


	// Specifies the event that is fired when the window completes activation or deactivation.
	//Event Activated;
	
	// Specifies the event that is fired when a window is closed (or the app terminates altogether).
	//Event Closed;
	
	Event<IWindow*, WindowSizeChangedEventArgs> WindowSizeChanged;

	//Event<IWindow*, PointerEventArgs> PointerCaptureLost;
	//Event<IWindow*, PointerEventArgs> PointerEntered;
	//Event<IWindow*, PointerEventArgs> PointerExited;
	Event<IWindow*, PointerEventArgs> PointerMoved;
	//Event<IWindow*, PointerEventArgs> PointerPressed;
	//Event<IWindow*, PointerEventArgs> PointerReleased;
	
	// Specifies the event that occurs when the mouse wheel is rotated.
	Event<IWindow*, PointerEventArgs> PointerWheelChanged;

	// Specifies the event that is fired when a non-system key is pressed down.
	Event<IWindow*, KeyEventArgs> KeyDown;

	// Specifies the event that is fired when a non-system key is released after a press.
	Event<IWindow*, KeyEventArgs> KeyUp;
};

// Event Delegates
typedef std::function<void(IWindow* sender, WindowSizeChangedEventArgs args)> WindowSizeChangedDelegate;
typedef std::function<void(IWindow* sender, PointerEventArgs args)> PointerMovedDelegate;
typedef std::function<void(IWindow* sender, PointerEventArgs args)> PointerWheelChangedDelegate;
typedef std::function<void(IWindow* sender, KeyEventArgs args)> KeyDownDelegate;
typedef std::function<void(IWindow* sender, KeyEventArgs args)> KeyUpDelegate;
