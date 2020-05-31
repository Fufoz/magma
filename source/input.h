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
	uint32_t x, y;
};

MousePos getMousePos();
void initInput(void* windowHandle);
bool isBtnPressed(KeyBoardBtn btn);
bool isMouseBtnPressed(MouseBtn btn);

#endif