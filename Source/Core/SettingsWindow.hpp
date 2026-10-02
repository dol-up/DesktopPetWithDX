#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <wrl.h>
#include <string>

class Graphics;
class AnimationSettingsStore;
struct ImGuiContext;
struct PetAnimationSettings;

class SettingsWindow {
public:
    SettingsWindow(HWND owner, Graphics& graphics, AnimationSettingsStore& store, const std::string& modelPath);
    ~SettingsWindow();
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;
    void Show();
    void Render();
    void ModelChanged();
    bool SaveChanges(bool force = false);
    bool SetAutonomousWalkingEnabled(bool enabled);
    void SetStatus(std::string message) { status = std::move(message); }
    bool IsVisible() const { return hWnd && IsWindowVisible(hWnd); }
private:
    friend void TestSettingsWindow(const char* modelPath);
    static LRESULT CALLBACK WindowProcSetup(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK WindowProcForward(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);
    void DrawControls();
    void DrawAnimationControls();
    void DrawWalkingControls();
    void ApplySettings(const PetAnimationSettings& value);
    void RenderFrame();
    void CreateRenderTarget();
    void Shutdown();
    Graphics& graphics;
    AnimationSettingsStore& store;
    const std::string& modelPath;
    HWND hWnd = nullptr;
    HINSTANCE hInstance = nullptr;
    ImGuiContext* imgui = nullptr;
    bool platformReady = false;
    bool rendererReady = false;
    bool dirty = false;
    UINT resizeWidth = 0, resizeHeight = 0;
    std::size_t previewClip = 0;
    bool previewLoop = true;
    float previewSpeed = 1;
    std::string status;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTarget;
};
