#ifndef MAGMA_INPUT_H
#define MAGMA_INPUT_H
#include <cstdint>

enum KeyBoardBtn
{
	A, B, C, D, E, F, G, H, I, J, K, L, M,
	N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
	Space, Enter, Escape, LeftCtrl,
	KeyBtnCount
};

enum MouseBtn
{
	LeftBtn,
	RightBtn,
	MiddleBtn,
	MouseBtnCount
};

struct MousePos
{
	int x, y;
};

void init_input(void* windowHandle);

MousePos get_mouse_position();

MousePos get_delta_mouse_position();

bool is_btn_pressed(KeyBoardBtn btn);

bool is_mouse_btn_pressed(MouseBtn btn);

void set_cursor_pos(void* windowHandle, int x, int y);

#endif