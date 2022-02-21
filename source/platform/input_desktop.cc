#include <input.h>
#include <GLFW/glfw3.h>

#include <bitset>
#include "logging.h"

static int keyRemapTable[KeyBtnCount] = {
	GLFW_KEY_A, GLFW_KEY_B, GLFW_KEY_C,
	GLFW_KEY_D, GLFW_KEY_E, GLFW_KEY_F,
	GLFW_KEY_G, GLFW_KEY_H, GLFW_KEY_I,
	GLFW_KEY_J, GLFW_KEY_K, GLFW_KEY_L,
	GLFW_KEY_M, GLFW_KEY_N, GLFW_KEY_O,
	GLFW_KEY_P, GLFW_KEY_Q, GLFW_KEY_R,
	GLFW_KEY_S, GLFW_KEY_T, GLFW_KEY_U,
	GLFW_KEY_V, GLFW_KEY_W, GLFW_KEY_X,
	GLFW_KEY_Y, GLFW_KEY_Z, GLFW_KEY_SPACE,
	GLFW_KEY_ENTER, GLFW_KEY_ESCAPE, GLFW_KEY_LEFT_CONTROL
};

static int mouseRemapTable[MouseBtnCount] = {
	GLFW_MOUSE_BUTTON_LEFT,
	GLFW_MOUSE_BUTTON_RIGHT,
	GLFW_MOUSE_BUTTON_MIDDLE
};

enum keyState
{
	STATE_RELEASED,
	STATE_PRESSED
};

static MousePos mousePosition = {};
static std::bitset<GLFW_KEY_LAST> keyState = {};
static std::bitset<GLFW_MOUSE_BUTTON_LAST> mouseState = {};

void init_input(void* windowHandle)
{
	GLFWwindow* handle = (GLFWwindow*)windowHandle;

	auto keyEventCallback = [](GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		if(key == GLFW_KEY_ESCAPE)
		{
			glfwSetWindowShouldClose(window, 1);
		}

		if(action == GLFW_PRESS || action == GLFW_REPEAT)
		{
			keyState[key] = STATE_PRESSED;
		}
		else
		{
			keyState[key] = STATE_RELEASED;
		}
	};

	auto mouseButtonEventCallback = [] (GLFWwindow* window, int button, int action, int mods)
	{
		if(action == GLFW_PRESS || action == GLFW_REPEAT)
		{
			mouseState[button] = STATE_PRESSED;
		}
		else
		{
			mouseState[button] = STATE_RELEASED;
		}
	};
	
	auto mouseHoverCallback = [](GLFWwindow*, double x, double y)
	{
		mousePosition.x = int(x);
		mousePosition.y = int(y);
	};
	
	glfwSetKeyCallback(handle, keyEventCallback);
	glfwSetMouseButtonCallback(handle, mouseButtonEventCallback);
	glfwSetCursorPosCallback(handle, mouseHoverCallback);
}

MousePos get_mouse_position()
{
	return mousePosition;
}

MousePos get_delta_mouse_position()
{
	static MousePos previousMousePos = mousePosition;
	MousePos currentMousePos = mousePosition;
	MousePos delta = {
		currentMousePos.x - previousMousePos.x,
		currentMousePos.y - previousMousePos.y
	};
	previousMousePos = currentMousePos;
	return delta;
}

bool is_btn_pressed(KeyBoardBtn btn)
{
	return keyState[keyRemapTable[btn]];
}

bool is_mouse_btn_pressed(MouseBtn btn)
{
	return mouseState[mouseRemapTable[btn]];
}

void set_cursor_pos(void* windowHandle, int x, int y)
{
	glfwSetCursorPos((GLFWwindow*)windowHandle, x, y); 
}



