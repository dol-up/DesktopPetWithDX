#include "Window.hpp"
#include "Graphics.hpp"
#include <memory>
#include <Windows.h>

// 콘솔의 main() 대신 윈도우 프로그램은 WinMain()을 사용합니다.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    bool fixedMode = true;
    bool wasModeKeyPressed = false;
    int width = 300;
    int height = 600;

    // 1. 투명 윈도우 생성
    Window window(width, height, "DesktopPetWindow");

    // 2. 다이렉트X 그래픽스 엔진 생성 (윈도우의 핸들(HWND)을 넘겨줌)
    Graphics gfx(window.GetHWND(), width, height);

    // 3. 메인 게임 루프
    while (window.ProcessMessages()) {

        HWND hWnd = window.GetHWND();
        RECT rect;
        GetWindowRect(hWnd, &rect);

        int currentX = rect.left;
        int currentY = rect.top;

        int speed = 5; // 이동 속도
        bool isMoved = false;
        bool isChanged = false;

        bool isModeKeyPressed = (GetAsyncKeyState('K') & 0x8000) && (GetAsyncKeyState('L') & 0x8000);

        if (isModeKeyPressed && !wasModeKeyPressed) {
            fixedMode = !fixedMode; // 1번만 뒤집힘
        }

        wasModeKeyPressed = isModeKeyPressed;

        if (!fixedMode) {
            if (GetAsyncKeyState('W') & 0x8000) { currentY -= speed; isMoved = true; }
            if (GetAsyncKeyState('S') & 0x8000) { currentY += speed; isMoved = true; }
            if (GetAsyncKeyState('A') & 0x8000) { currentX -= speed; isMoved = true; }
            if (GetAsyncKeyState('D') & 0x8000) { currentX += speed; isMoved = true; }
        }
        

        if (GetAsyncKeyState('O') & 0x8000) { width += 10; height += 20; isChanged = true; }
        if (GetAsyncKeyState('P') & 0x8000) { 
            if (width > 100) { width -= 10; height -= 20;}
            isChanged = true; 
        }

        if (isMoved || isChanged) {
            // width, height는 맨 위에서 선언한 창 크기 변수
            SetWindowPos(hWnd, HWND_TOP, currentX, currentY, width, height, SWP_SHOWWINDOW);
        }

        // 매 프레임마다 화면을 지우고 새로 그립니다.
        gfx.Render();

    }

    return 0;
}