#include "Window.hpp"
#include "Graphics.hpp"
#include <memory>
#include <Windows.h>
#include <commdlg.h>
#include <fstream>
#include <string>


std::string OpenFileDialog() {
    char fileName[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL; // 윈도우 핸들 넣어주면 더 좋음
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "3D Models (*.fbx;*.obj)\0*.fbx;*.obj\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(fileName);
    }
    return "";
}

void SaveLastModelPath(const std::string& path) {
    std::ofstream ofs("last_model.txt");
    if (ofs.is_open()) {
        ofs << path;
    }
}

std::string LoadLastModelPath() {
    std::ifstream ifs("last_model.txt");
    std::string path;
    if (ifs.is_open()) {
        std::getline(ifs, path);
    }
    return path;
}

// 콘솔의 main() 대신 윈도우 프로그램은 WinMain()을 사용합니다.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    bool fixedMode = true;
    bool wasModeKeyPressed = false;
    bool wasModelChangeKeyPressed = false;
    int width = 600;
    int height = 600;

    // 1. 투명 윈도우 생성
    Window window(width, height, "DesktopPetWindow");

    std::string modelPath = LoadLastModelPath();
    // 탐색기 띄우기
    if (modelPath.empty()) {
        modelPath = OpenFileDialog();
    }

    // 경로 저장
    SaveLastModelPath(modelPath);

    // 2. 다이렉트X 그래픽스 엔진 생성 (윈도우의 핸들(HWND)을 넘겨줌)
    Graphics gfx(window.GetHWND(), width, height, modelPath);


    int width_diff = width / 100;
    int height_diff = height / 100;

    // 윈도우 크기 조절
    SetWindowPos(window.GetHWND(), HWND_TOPMOST, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);

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

            if (GetAsyncKeyState('O') & 0x8000) { width += width_diff; height += height_diff; isChanged = true; }
            if (GetAsyncKeyState('P') & 0x8000) {
                if (width > 100) { width -= width_diff; height -= height_diff; }
                isChanged = true;
            }
        }

        bool isModelChangeKeyPressed = (GetAsyncKeyState('M') & 0x8000);

        // 키를 꾹 누르고 있어도 창이 무한으로 뜨지 않게 "방금 막 눌렀을 때"만 실행
        if (isModelChangeKeyPressed && !wasModelChangeKeyPressed) {

            // 1. 탐색기 띄워서 경로 받아오기
            std::string newPath = OpenFileDialog();

            // 2. 유저가 파일을 제대로 골랐다면?
            if (!newPath.empty()) {
                // 3. 모델 교체 실행!
                gfx.LoadNewModel(newPath);

                // 4. 다음에 켤 때도 이 모델이 나오게 경로 저장!
                SaveLastModelPath(newPath);
            }
        }
        // 상태 업데이트
        wasModelChangeKeyPressed = isModelChangeKeyPressed;
        

        if (isMoved || isChanged) {
            // width, height는 맨 위에서 선언한 창 크기 변수
            SetWindowPos(hWnd, HWND_TOP, currentX, currentY, width, height, SWP_SHOWWINDOW);
        }

        // 매 프레임마다 화면을 지우고 새로 그립니다.
        gfx.Render();

    }

    return 0;
}