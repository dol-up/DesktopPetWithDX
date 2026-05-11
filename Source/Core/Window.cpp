#include "Window.hpp"
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

Window::Window(int width, int height, const char* name)
	: hInstance(GetModuleHandle(nullptr))
{
	// 윈도우 클래스 등록
	WNDCLASSEX wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WindowProcSetup;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = nullptr;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = nullptr; //CreateSolidBrush(RGB(255, 255, 255));
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = name;
	RegisterClassEx(&wc);

	// 윈도우 스타일 설정 (팝업창 + 최상단 + 레이어드)
	DWORD dwStyle = WS_POPUP;  //WS_OVERLAPPEDWINDOW
	DWORD dwExStyle = WS_EX_TOPMOST; // 항상 위, 투명도 지원

	// 윈도우 생성
	hWnd = CreateWindowEx(
		dwExStyle, name, name, dwStyle,
		0, 0, width, height,
		nullptr, nullptr, hInstance, this
	);


	DWM_BLURBEHIND bb = { 0 };
	bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
	bb.fEnable = true;
	bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
	DwmEnableBlurBehindWindow(hWnd, &bb);

	ShowWindow(hWnd, SW_SHOWDEFAULT);
}

Window::~Window() {
	DestroyWindow(hWnd);
}

bool Window::ProcessMessages() {
	MSG msg;
	// 윈도우 메시지 처리하는 루프
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			return false;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;
}

//Win32 API의 C 스타일 콜백을 C++ 클래스 멤버 함수와 연결
LRESULT CALLBACK Window::WindowProcSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_NCCREATE) {
		const CREATESTRUCTW* const pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
		Window* const pWnd = static_cast<Window*>(pCreate->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Window::WindowProcForward));
		return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK Window::WindowProcForward(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	Window* const pWnd = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
}

LRESULT Window::HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;

	case WM_KEYDOWN:
		// wParam 에는 어떤 키가 눌렸는지에 대한 정보가 들어있음!!!!
		// Win32 API에서 영문자 키 코드는 항상 '대문자'로 비교
		if (wParam == 'Q') {
			PostQuitMessage(0); // Q 누르면 종료
		}
		return 0;
	case WM_LBUTTONDOWN: {
		isDragging = true;
		SetCapture(hWnd); 
		GetCursorPos(&lastMousePos);
		dragStartPos = lastMousePos;
		return 0;
	}

	case WM_MOUSEMOVE: {
		if (isDragging) {
			POINT currentPos;
			GetCursorPos(&currentPos);

			// 마우스가 얼마나 이동했는지(Delta) 계산
			int dx = currentPos.x - lastMousePos.x;
			int dy = currentPos.y - lastMousePos.y;

			// 현재 창의 위치(크기)를 가져옴
			RECT rect;
			GetWindowRect(hWnd, &rect);

			// 계산된 거리만큼 창 밀기
			SetWindowPos(hWnd, nullptr, rect.left + dx, rect.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

			// 다음 프레임 계산을 위해 좌표 갱신
			lastMousePos = currentPos;
		}
		return 0;
	}

	case WM_LBUTTONUP: {
		isDragging = false;
		ReleaseCapture();

		// 마우스를 뗄 때의 위치를 가져옵니다.
		POINT currentPos;
		GetCursorPos(&currentPos);

		// 처음 누른 곳과 지금 뗀 곳의 거리 차이를 계산합니다 (절댓값)
		int dx = abs(currentPos.x - dragStartPos.x);
		int dy = abs(currentPos.y - dragStartPos.y);

		// 오차 범위 5픽셀 미만이면 클릭
		if (dx < 5 && dy < 5) {
			
			OutputDebugStringA("클릭 감지\n"); // 디버그에 출력
		}

		return 0;
	}

	case WM_NCHITTEST: {
		// 현재 마우스 좌표 (모니터 전체 화면 기준)
		int mouseX = (int)(short)LOWORD(lParam);
		int mouseY = (int)(short)HIWORD(lParam);

		// 2. 창의 위치와 크기를 가져옴
		RECT rc;
		GetWindowRect(hWnd, &rc);

		// 3. 마우스 좌표를 '우리 창의 내부' 좌표(0~width, 0~height)로 변환
		int localX = mouseX - rc.left;
		int localY = mouseY - rc.top;

		// 정중앙 좌표 계산 
		int centerX = (rc.right - rc.left) / 2;
		int centerY = (rc.bottom - rc.top) / 2;

		// 마우스와 중앙 사이의 거리(제곱)를 계산
		int dx = localX - centerX;
		int dy = localY - centerY;
		int distanceSquared = (dx * dx) + (dy * dy);

		// 6. 충돌 판정! (반지름 150픽셀짜리 원 안에 마우스가 들어왔는가?)
		int radius = 150;

		if (distanceSquared < (radius * radius)) {
			// [모델 영역 적중!] "여긴 내 구역이야! 클릭을 허락한다!"
			return HTCLIENT;
		}
		else {
			return HTTRANSPARENT;
		}
	}

	}

	return DefWindowProc(hWnd, msg, wParam, lParam);

}