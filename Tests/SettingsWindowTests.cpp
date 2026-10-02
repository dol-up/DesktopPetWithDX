#include "SettingsWindow.hpp"
#include "Graphics.hpp"
#include "AnimationSettingsStore.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <ScreenGrab.h>
#include <wincodec.h>
#include <d3d11sdklayers.h>
#include <filesystem>
#include <iostream>
#include <cstring>

void Check(bool, const char*);

void TestSettingsWindow(const char* modelPath) {
    const wchar_t* className = L"DesktopPetHiddenUiTest";
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = className;
    RegisterClassW(&wc);
    HWND owner = CreateWindowW(className, L"Hidden rendering test", WS_OVERLAPPEDWINDOW,
        0, 0, 600, 600, nullptr, nullptr, wc.hInstance, nullptr);
    Check(owner != nullptr, "create hidden UI test owner");
    struct WindowScope { HWND window; const wchar_t* name; HINSTANCE instance;
        ~WindowScope() { DestroyWindow(window); UnregisterClassW(name, instance); }
    } windowScope{ owner, className, wc.hInstance };
    Graphics graphics(owner, 600, 600, modelPath, D3D_DRIVER_TYPE_WARP);
    const std::string file = "x64/Debug/settings-ui-test.txt";
    std::error_code cleanupError;
    std::filesystem::remove(file, cleanupError);
    AnimationSettingsStore store(file);
    std::string path(modelPath);
    SettingsWindow settings(owner, graphics, store, path);
    Microsoft::WRL::ComPtr<ID3D11InfoQueue> info;
    Check(SUCCEEDED(graphics.GetDevice()->QueryInterface(IID_PPV_ARGS(&info))), "UI debug info queue");
    info->ClearStoredMessages();
    const auto snapshot = [&](const wchar_t* filename) {
        settings.RenderFrame();
        Check(ImGui::GetDrawData()->TotalVtxCount > 500, "ImGui settings produced geometry");
        Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer;
        Check(SUCCEEDED(settings.swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer))), "UI snapshot buffer");
        Check(SUCCEEDED(DirectX::SaveWICTextureToFile(graphics.GetContext(), buffer.Get(), GUID_ContainerFormatPng, filename)), "UI snapshot PNG");
    };
    settings.RenderFrame(); // Allow ImGui's initial table/font measurement to settle.
    snapshot(L"x64/Debug/animation-settings-preview.png");
    const auto initialSettings = graphics.GetBehavior().GetSettings();
    const auto click = [&](float x, float y) {
        auto& io = ImGui::GetIO();
        io.AddFocusEvent(true);
        io.AddMousePosEvent(x, y);
        settings.RenderFrame();
        io.AddMouseButtonEvent(0, true);
        settings.RenderFrame();
        io.AddMouseButtonEvent(0, false);
        settings.RenderFrame();
    };
    const auto body = [&]() -> ImGuiWindow* {
        auto* parent = ImGui::FindWindowByName("Animation settings");
        for (auto* window : ImGui::GetCurrentContext()->Windows)
            if (window->ParentWindow == parent && std::strstr(window->Name, "SettingsBody")) return window;
        Check(false, "find actual settings child layout");
        return nullptr;
    };
    const auto save = [&]() {
        const auto* child = body();
        click(child->Pos.x + 20, child->Pos.y + child->Size.y + 2 * ImGui::GetStyle().ItemSpacing.y +
            1 + ImGui::GetFrameHeight() / 2);
    };
    const auto tabBar = [&]() -> ImGuiTabBar* {
        auto& pool = ImGui::GetCurrentContext()->TabBars;
        for (int i = 0; i < pool.GetBufSize(); ++i) if (pool.GetByIndex(i)->Tabs.Size == 2) return pool.GetByIndex(i);
        Check(false, "find settings tab bar");
        return nullptr;
    };
    const auto selectTab = [&](int index) {
        const auto* bar = tabBar();
        const auto& tab = bar->Tabs[index];
        click(bar->BarRect.Min.x + tab.Offset + tab.Width / 2, (bar->BarRect.Min.y + bar->BarRect.Max.y) / 2);
        settings.RenderFrame();
    };
    auto& tablePool = ImGui::GetCurrentContext()->Tables;
    Check(tablePool.GetBufSize() > 0, "state table exists");
    const auto* table = tablePool.GetByIndex(0);
    const float rowHeight = table->RowPosY2 - table->RowPosY1;
    const float idleY = (table->RowPosY1 + table->RowPosY2) / 2 - (PetStateCount - 1) * rowHeight;
    click(table->Columns[1].MaxX - 12, idleY);
    snapshot(L"x64/Debug/animation-settings-selector.png");
    auto& popups = ImGui::GetCurrentContext()->OpenPopupStack;
    Check(popups.Size > 0 && popups.back().Window, "clip selector opened its popup");
    const auto* popup = popups.back().Window;
    click(popup->Pos.x + popup->WindowPadding.x + 30,
        popup->DC.CursorStartPos.y + ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeight() / 2);
    Check(graphics.GetBehavior().GetSettings().states[0].selection == ClipSelection::None && settings.dirty,
        "ImGui selection input applies state settings");
    save();
    Check(!settings.dirty && std::filesystem::exists(file), "ImGui save button writes settings");
    graphics.GetBehavior().SetSettings(initialSettings);
    settings.status.clear();
    settings.RenderFrame();
    snapshot(L"x64/Debug/animation-settings-preview.png");
    selectTab(1);
    snapshot(graphics.GetBehavior().HasWalkAnimation() ? L"x64/Debug/walking-settings-preview.png" :
        L"x64/Debug/walking-settings-unavailable.png");
    if (graphics.GetBehavior().HasWalkAnimation()) {
        const float checkboxY = tabBar()->BarRect.Max.y + ImGui::GetStyle().ItemSpacing.y +
            ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeight() / 2;
        click(body()->Pos.x + body()->WindowPadding.x + 12, checkboxY);
        Check(graphics.GetBehavior().GetSettings().autonomousWalking && settings.dirty,
            "real walking checkbox enables model profile");
        auto walking = graphics.GetBehavior().GetSettings();
        walking.wander.pixelsPerSecond = 90;
        walking.wander.forwardYawRadians = 0.5;
        graphics.GetBehavior().SetSettings(walking);
        save();
        AnimationSettingsStore walkingStore(file);
        std::string error;
        Check(walkingStore.Load(error), "reopen UI walking settings");
        const auto loaded = walkingStore.Restore(path, graphics.GetAnimationClips());
        Check(loaded.settings.autonomousWalking && loaded.settings.wander.pixelsPerSecond == 90 &&
            loaded.settings.wander.forwardYawRadians == 0.5, "UI saves enable, movement speed and direction correction");
        const auto originalPath = path;
        path += ".new-model";
        settings.ModelChanged();
        Check(!graphics.GetBehavior().GetSettings().autonomousWalking, "switching to a new model restores disabled defaults");
        path = originalPath;
        settings.ModelChanged();
        Check(graphics.GetBehavior().GetSettings().autonomousWalking && graphics.GetBehavior().GetSettings().wander.pixelsPerSecond == 90,
            "returning to a model restores its walking profile");
        snapshot(L"x64/Debug/walking-settings-preview.png");
    } else {
        Check(!settings.SetAutonomousWalkingEnabled(true) && !graphics.GetBehavior().GetSettings().autonomousWalking,
            "UI refuses to enable walking without a usable clip");
    }
    selectTab(0);
    graphics.Render();
    const auto idle = graphics.GetBehavior().GetAssignedClip(PetState::Idle);
    if (idle != InvalidSkeletonNode) {
        graphics.GetBehavior().Preview(idle, true, 1);
        graphics.GetBehavior().PausePreview();
        graphics.GetBehavior().SeekPreview(0.25);
        snapshot(L"x64/Debug/animation-settings-playing.png");
        SendMessageW(settings.hWnd, WM_CLOSE, 0, 0);
        Check(!graphics.GetBehavior().IsPreviewing(), "closing UI restores automatic behavior");
    }
    auto custom = graphics.GetBehavior().GetSettings();
    custom.states[0].selection = ClipSelection::None;
    graphics.GetBehavior().SetSettings(custom);
    settings.dirty = true;
    SendMessageW(settings.hWnd, WM_CLOSE, 0, 0);
    AnimationSettingsStore reopened(file);
    std::string error;
    Check(reopened.Load(error) && reopened.Restore(path, graphics.GetAnimationClips()).settings.states[0].selection == ClipSelection::None,
        "closing UI saves changes");
    SetWindowPos(settings.hWnd, nullptr, 0, 0, 900, 740, SWP_NOZORDER | SWP_NOACTIVATE);
    snapshot(L"x64/Debug/animation-settings-resized.png");
    graphics.Render();
    for (UINT64 i = 0; i < info->GetNumStoredMessages(); ++i) {
        SIZE_T bytes = 0;
        info->GetMessage(i, nullptr, &bytes);
        std::vector<unsigned char> data(bytes);
        auto* message = reinterpret_cast<D3D11_MESSAGE*>(data.data());
        info->GetMessage(i, message, &bytes);
        if (message->Severity == D3D11_MESSAGE_SEVERITY_ERROR || message->Severity == D3D11_MESSAGE_SEVERITY_CORRUPTION) {
            std::cerr << message->pDescription << '\n';
            Check(false, "no D3D11 errors when alternating pet and settings rendering");
        }
    }
    std::filesystem::remove(file, cleanupError);
    std::cout << "ImGui hidden-window rendering, resize and close-save tests passed\n";
}
