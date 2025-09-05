#include "WindowContainer.h"

HWND RenderWindow::GetHWND() const {
	return handle;
}

bool RenderWindow::Initialize(WindowContainer* pWindowContainer, HINSTANCE hInstance, std::string window_title, std::string window_class, int width, int height) {
	this->hInstance = hInstance;
	this->window_title = window_title;
	this->window_title_wide = StringConverter::StringToWide(this->window_title);
	this->window_class = window_class;
	this->window_class_wide = StringConverter::StringToWide(this->window_class);
	this->width = width;
	this->height = height;

	this->RegisterWindowClass();

	int centerX = (GetSystemMetrics(SM_CXSCREEN) - this->width) / 2;	
	int centerY = (GetSystemMetrics(SM_CYSCREEN) - this->height) / 2;

	RECT wr;
	wr.left = centerX;
	wr.top = centerY;
	wr.right = wr.left+ this->width;
	wr.bottom = wr.top+ this->height;
	AdjustWindowRect(&wr, WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, FALSE);


	handle = CreateWindowEx(0,
		this->window_class_wide.c_str(),
		this->window_title_wide.c_str(),
		WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU,
		wr.left,
		wr.top,
		wr.right - wr.left,
		wr.bottom - wr.top,
		NULL,
		NULL,
		hInstance,
		pWindowContainer);
	if (handle == NULL) {
		ErrorLogger::Log(GetLastError(), "CreateWindowEX Failed for window: " + window_title);
		return false;
	}

	ShowWindow(handle, SW_SHOW);
	SetForegroundWindow(handle);
	SetFocus(handle);

	return true;
}

LRESULT CALLBACK HandleMsgDirect(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg)
	{
		//other messages

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;

	default:
	{
		WindowContainer* const pWindow = reinterpret_cast<WindowContainer*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		return pWindow->WindowProc(hwnd, uMsg, wParam, lParam);
	}
	}
}

LRESULT CALLBACK HandleMessageSetup(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	switch (uMsg)
	{
	case WM_NCCREATE:
	{
		const CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		WindowContainer* pWindow = reinterpret_cast<WindowContainer*>(pCreate->lpCreateParams);
		if (pWindow == nullptr) {
			ErrorLogger::Log("Critical Error: Pointer to window container is null during WM_NCCREATE!");
			exit(-1);
		}
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HandleMsgDirect));
		return pWindow->WindowProc(hwnd, uMsg, wParam, lParam);
	}
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

bool RenderWindow::ProcessMessages() {
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));
	while (PeekMessage(&msg, handle, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	if (msg.message == WM_NULL) {
		if (!IsWindow(handle)) {
			handle = NULL;
			UnregisterClass(this->window_class_wide.c_str(), this->hInstance);
			return false;
		}
	}
	return true;
}

RenderWindow::~RenderWindow() {
	if (handle != NULL) {
		UnregisterClass(this->window_class_wide.c_str(), this->hInstance);
		DestroyWindow(handle);
	}
}

void RenderWindow::RegisterWindowClass() {
	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEX));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = HandleMessageSetup;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hIcon = NULL;
	wc.hIconSm = NULL;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpszClassName = this->window_class_wide.c_str();
	wc.lpszMenuName = NULL;
	RegisterClassEx(&wc);
	
}