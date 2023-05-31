#pragma once

typedef struct
{
	HINSTANCE	hInstance;
	void* wndproc;

	HWND    hWnd;			// handle to window

	qboolean minidriver;
	qboolean allowdisplaydepthchange;
	qboolean mcd_accelerated;

	FILE* log_fp;
} vkwstate_t;

extern vkwstate_t vkw_state;
