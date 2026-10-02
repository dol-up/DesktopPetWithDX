#include "Window.hpp"
#include "SettingsWindow.hpp"
#include "AnimationSettingsStore.hpp"
#include "Graphics.hpp"
#include "AutonomousMotion.hpp"
#include <memory>
#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <sstream>


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

struct LastSession {
    std::string modelPath;
    POINT position{};
    bool hasPosition = false;
};

void SaveLastSession(const std::string& path, HWND window) {
    RECT rect{};
    const bool hasPosition = GetWindowRect(window, &rect) != FALSE;
    std::ofstream ofs("last_model.txt");
    if (ofs.is_open()) {
        ofs << path << '\n';
        if (hasPosition) ofs << "position " << rect.left << ' ' << rect.top << '\n';
    }
}

LastSession LoadLastSession() {
    std::ifstream ifs("last_model.txt");
    LastSession session;
    if (ifs.is_open()) {
        std::getline(ifs, session.modelPath);
        std::string positionLine;
        if (std::getline(ifs, positionLine)) {
            std::istringstream values(positionLine);
            std::string tag;
            LONG x, y;
            if (values >> tag >> x >> y && tag == "position") {
                values >> std::ws;
                if (values.eof()) {
                    session.position = { x, y };
                    session.hasPosition = true;
                }
            }
        }
    }
    return session;
}

void RestoreLastPosition(HWND window, const LastSession& session) {
    if (!session.hasPosition) return;
    RECT windowRect{};
    if (!GetWindowRect(window, &windowRect)) return;
    const LONG width = windowRect.right - windowRect.left;
    const LONG height = windowRect.bottom - windowRect.top;
    LONG x = session.position.x;
    LONG y = session.position.y;
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    if (GetMonitorInfoW(MonitorFromPoint(session.position, MONITOR_DEFAULTTONEAREST), &monitor)) {
        // A pet's transparent window can extend beyond the work area while the
        // model remains visible. Keep partially visible positions unchanged.
        const auto right = static_cast<long long>(x) + width;
        const auto bottom = static_cast<long long>(y) + height;
        if (right <= monitor.rcWork.left || x >= monitor.rcWork.right ||
            bottom <= monitor.rcWork.top || y >= monitor.rcWork.bottom) {
            x = std::max(monitor.rcWork.left, std::min(x, monitor.rcWork.right - width));
            y = std::max(monitor.rcWork.top, std::min(y, monitor.rcWork.bottom - height));
        }
    }
    SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// 콘솔의 main() 대신 윈도우 프로그램은 WinMain()을 사용합니다.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // WIC embedded textures need COM on the render thread.
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    struct ComScope { ~ComScope() { CoUninitialize(); } } comScope;

    bool fixedMode = true;
    bool wasModeKeyPressed = false;
    bool wasModelChangeKeyPressed = false;
    bool wasRotationResetKeyPressed = false;
    bool wasWanderKeyPressed = false;
    int width = 600;
    int height = 600;

    // 1. 투명 윈도우 생성
    Window window(width, height, "DesktopPetWindow");

    const LastSession lastSession = LoadLastSession();
    std::string modelPath = lastSession.modelPath;
    // 탐색기 띄우기
    if (modelPath.empty()) {
        modelPath = OpenFileDialog();
    }

    if (modelPath.empty()) return 0;

    // 2. 다이렉트X 그래픽스 엔진 생성 (윈도우의 핸들(HWND)을 넘겨줌)
    Graphics gfx(window.GetHWND(), width, height, modelPath);
    AutonomousMotion motion;
    window.SetModelHitTest([&gfx](int clientX, int clientY) {
        return gfx.HitTestModel(clientX, clientY);
    });
    window.SetModelRotate([&gfx](float deltaX, float deltaY) {
        gfx.RotateModel(deltaX, deltaY);
    });

    AnimationSettingsStore animationSettings;
    std::string settingsLoadError;
    animationSettings.Load(settingsLoadError);
    SettingsWindow settingsWindow(window.GetHWND(), gfx, animationSettings, modelPath);
    if (!settingsLoadError.empty()) settingsWindow.SetStatus("Settings load failed: " + settingsLoadError);
    window.SetSettingsRequested([&settingsWindow]() {
        settingsWindow.Show();
    });


    int width_diff = width / 100;
    int height_diff = height / 100;

    // 윈도우 크기 조절
    SetWindowPos(window.GetHWND(), HWND_TOPMOST, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    RestoreLastPosition(window.GetHWND(), lastSession);
    SaveLastSession(modelPath, window.GetHWND());

    auto previousFrameTime = std::chrono::steady_clock::now();

    // 3. 메인 게임 루프
    while (window.ProcessMessages()) {

        const auto currentFrameTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentFrameTime - previousFrameTime).count();
        previousFrameTime = currentFrameTime;
        deltaTime = std::min(deltaTime, 0.05f);

        HWND hWnd = window.GetHWND();
        const auto keyDown = [hWnd](int key) {
            return GetForegroundWindow() == hWnd && (GetAsyncKeyState(key) & 0x8000) != 0;
        };
        RECT rect;
        GetWindowRect(hWnd, &rect);

        int currentX = rect.left;
        int currentY = rect.top;

        int speed = 5; // 이동 속도
        bool isMoved = false;
        bool isChanged = false;

        bool isModeKeyPressed = keyDown('K') && keyDown('L');

        if (isModeKeyPressed && !wasModeKeyPressed) {
            fixedMode = !fixedMode; // 1번만 뒤집힘
        }

        wasModeKeyPressed = isModeKeyPressed;

        const bool isRotationResetKeyPressed = keyDown('R');
        if (!fixedMode && isRotationResetKeyPressed && !wasRotationResetKeyPressed) {
            gfx.ResetModelRotation();
            motion.Reset(gfx.GetAutonomousFacing());
        }
        wasRotationResetKeyPressed = isRotationResetKeyPressed;

        // Uses the same model profile and save path as the settings checkbox.
        const bool isWanderKeyPressed = keyDown('B');
        if (isWanderKeyPressed && !wasWanderKeyPressed) {
            settingsWindow.SetAutonomousWalkingEnabled(!gfx.GetBehavior().GetSettings().autonomousWalking);
        }
        wasWanderKeyPressed = isWanderKeyPressed;

        if (!fixedMode) {
            if (keyDown('W')) { currentY -= speed; isMoved = true; }
            if (keyDown('S')) { currentY += speed; isMoved = true; }
            if (keyDown('A')) { currentX -= speed; isMoved = true; }
            if (keyDown('D')) { currentX += speed; isMoved = true; }

            if (keyDown('O')) { width += width_diff; height += height_diff; isChanged = true; }
            if (keyDown('P')) {
                if (width > 100) { width -= width_diff; height -= height_diff; }
                isChanged = true;
            }
        }

        bool isModelChangeKeyPressed = keyDown('M');

        // 키를 꾹 누르고 있어도 창이 무한으로 뜨지 않게 "방금 막 눌렀을 때"만 실행
        if (isModelChangeKeyPressed && !wasModelChangeKeyPressed) {

            // 1. 탐색기 띄워서 경로 받아오기
            std::string newPath = OpenFileDialog();

            // 2. 유저가 파일을 제대로 골랐다면?
            if (!newPath.empty()) {
                if (!settingsWindow.SaveChanges()) {
                    settingsWindow.Show();
                } else {
                    try {
                        gfx.LoadNewModel(newPath);
                        motion.Reset(gfx.GetAutonomousFacing());
                        modelPath = newPath;
                        settingsWindow.ModelChanged();
                        SaveLastSession(modelPath, hWnd);
                    } catch (const std::exception& error) {
                        settingsWindow.SetStatus(std::string("Model load failed: ") + error.what());
                        settingsWindow.Show();
                    }
                }
            }
        }
        // 상태 업데이트
        wasModelChangeKeyPressed = isModelChangeKeyPressed;
        

        if (isMoved || isChanged) {
            // width, height는 맨 위에서 선언한 창 크기 변수
            SetWindowPos(hWnd, HWND_TOP, currentX, currentY, width, height, SWP_SHOWWINDOW);
        }

        const bool suspendPhysics = window.IsDragging() || window.IsRotating() || !fixedMode || gfx.GetBehavior().IsPreviewing();
        const auto physicsResult = motion.Update(hWnd, deltaTime, gfx, suspendPhysics, settingsWindow.IsVisible());
        PetBehaviorInput behaviorInput;
        behaviorInput.dragging = window.IsDragging();
        behaviorInput.physicsSuspended = suspendPhysics;
        behaviorInput.physicsValid = physicsResult.valid;
        behaviorInput.grounded = physicsResult.grounded;
        behaviorInput.justLanded = physicsResult.justLanded;
        behaviorInput.walking = motion.IsWalking();
        gfx.UpdateBehavior(deltaTime, behaviorInput);

        // 매 프레임마다 화면을 지우고 새로 그립니다.
        gfx.Render();
        settingsWindow.Render();

    }

    if (!settingsWindow.SaveChanges())
        MessageBoxA(window.GetHWND(), "Could not save animation settings. Check write access to animation_settings.txt.", "Desktop Pet", MB_OK | MB_ICONERROR);
    SaveLastSession(modelPath, window.GetHWND());
    return 0;
}
