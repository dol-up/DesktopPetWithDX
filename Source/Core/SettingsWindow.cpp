#include "SettingsWindow.hpp"
#include "Graphics.hpp"
#include "AnimationSettingsStore.hpp"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <algorithm>
#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
    constexpr wchar_t SettingsClassName[] = L"DesktopPetSettingsWindow";
    constexpr const char* StateNames[] = { "대기", "잡힘", "낙하", "착지", "걷기" };
    constexpr double Pi = 3.14159265358979323846;
    bool ClipSelector(const char* label, PetBehavior& behavior, const std::vector<AnimationClip>& clips,
        std::size_t state, StateAnimationSettings& config) {
        bool changed = false;
        const char* selection = config.selection == ClipSelection::Automatic ? "자동 선택" :
            config.selection == ClipSelection::None ? "재생 안 함" :
            config.clipIndex < clips.size() ? clips[config.clipIndex].displayName.c_str() : "선택";
        if (ImGui::BeginCombo(label, selection)) {
            if (ImGui::Selectable("자동 선택", config.selection == ClipSelection::Automatic)) {
                config.selection = ClipSelection::Automatic; config.clipIndex = InvalidSkeletonNode; changed = true;
            }
            if (ImGui::Selectable("재생 안 함", config.selection == ClipSelection::None)) {
                config.selection = ClipSelection::None; config.clipIndex = InvalidSkeletonNode; changed = true;
            }
            for (std::size_t c = 0; c < clips.size(); ++c) {
                const bool supported = state == WalkStateIndex ? behavior.IsWalkClipSupported(c) :
                    behavior.IsClipSupported(c) && (state != 3 || clips[c].durationSeconds > 0);
                ImGui::PushID(static_cast<int>(c));
                ImGui::BeginDisabled(!supported);
                if (ImGui::Selectable(clips[c].displayName.c_str(), config.selection == ClipSelection::Clip && config.clipIndex == c)) {
                    config.selection = ClipSelection::Clip; config.clipIndex = c; changed = true;
                }
                ImGui::EndDisabled();
                if (!supported && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip(state == WalkStateIndex
                        ? "길이가 있는 제자리 동작을 선택하세요. 지원하지 않는 형식과 전진 루트 이동은 사용할 수 없습니다."
                        : "이 상태에서 사용할 수 없는 클립입니다.");
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        return changed;
    }
    void Require(HRESULT result, const char* message) { if (FAILED(result)) throw std::runtime_error(message); }
    std::string PathLabel(const std::string& path) {
        const int size = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, nullptr, 0);
        if (!size) return path;
        std::wstring wide(size, 0);
        MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, &wide[0], size);
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (!bytes) return path;
        std::string result(bytes, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], bytes, nullptr, nullptr);
        result.pop_back();
        return result;
    }
}

SettingsWindow::SettingsWindow(HWND owner, Graphics& graphics, AnimationSettingsStore& store, const std::string& modelPath)
    : graphics(graphics), store(store), modelPath(modelPath), hInstance(GetModuleHandleW(nullptr)) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProcSetup;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = SettingsClassName;
    RegisterClassExW(&wc);
    try {
        RECT rect{ 0, 0, 720, 650 };
        const DWORD style = WS_OVERLAPPEDWINDOW;
        AdjustWindowRectEx(&rect, style, FALSE, WS_EX_TOOLWINDOW);
        hWnd = CreateWindowExW(WS_EX_TOOLWINDOW, SettingsClassName, L"데스크톱 펫 · 애니메이션 설정", style,
            CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, owner, nullptr, hInstance, this);
        if (!hWnd) throw std::runtime_error("Cannot create settings window");
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        Microsoft::WRL::ComPtr<IDXGIFactory> factory;
        Require(graphics.GetDevice()->QueryInterface(IID_PPV_ARGS(&dxgiDevice)), "Settings DXGI device");
        Require(dxgiDevice->GetAdapter(&adapter), "Settings adapter");
        Require(adapter->GetParent(IID_PPV_ARGS(&factory)), "Settings factory");
        DXGI_SWAP_CHAIN_DESC description{};
        description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.BufferCount = 2;
        description.OutputWindow = hWnd;
        description.Windowed = TRUE;
        description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        Require(factory->CreateSwapChain(graphics.GetDevice(), &description, &swapChain), "Settings swap chain");
        factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
        CreateRenderTarget();
        IMGUI_CHECKVERSION();
        imgui = ImGui::CreateContext();
        ImGui::SetCurrentContext(imgui);
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        wchar_t windows[MAX_PATH]{};
        GetWindowsDirectoryW(windows, MAX_PATH);
        const std::wstring fontPath = std::wstring(windows) + L"\\Fonts\\malgun.ttf";
        if (GetFileAttributesW(fontPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, fontPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string font(bytes, 0);
            WideCharToMultiByte(CP_UTF8, 0, fontPath.c_str(), -1, &font[0], bytes, nullptr, nullptr);
            io.Fonts->AddFontFromFileTTF(font.c_str(), 18);
        }
        ImGui::StyleColorsDark();
        ImGui::GetStyle().FrameRounding = 4;
        ImGui::GetStyle().Colors[ImGuiCol_PopupBg].w = 1;
        ImGui::GetStyle().ItemSpacing = ImVec2(10, 10);
        platformReady = ImGui_ImplWin32_Init(hWnd);
        rendererReady = ImGui_ImplDX11_Init(graphics.GetDevice(), graphics.GetContext());
        if (!platformReady || !rendererReady) throw std::runtime_error("Cannot initialize ImGui settings");
        ModelChanged();
    } catch (...) { Shutdown(); throw; }
}

SettingsWindow::~SettingsWindow() { Shutdown(); }
void SettingsWindow::Shutdown() {
    if (imgui) {
        ImGui::SetCurrentContext(imgui);
        if (rendererReady) ImGui_ImplDX11_Shutdown();
        if (platformReady) ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(imgui);
        imgui = nullptr;
    }
    if (hWnd) { DestroyWindow(hWnd); hWnd = nullptr; }
    UnregisterClassW(SettingsClassName, hInstance);
}

void SettingsWindow::CreateRenderTarget() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer;
    Require(swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)), "Settings back buffer");
    Require(graphics.GetDevice()->CreateRenderTargetView(buffer.Get(), nullptr, &renderTarget), "Settings render target");
}

void SettingsWindow::ModelChanged() {
    auto restored = store.Restore(modelPath, graphics.GetAnimationClips());
    auto& walk = restored.settings.states[WalkStateIndex];
    if (walk.selection == ClipSelection::Clip && !graphics.GetBehavior().IsWalkClipSupported(walk.clipIndex)) {
        walk.selection = ClipSelection::Automatic;
        walk.clipIndex = InvalidSkeletonNode;
        ++restored.missingClips;
    }
    graphics.GetBehavior().SetSettings(restored.settings);
    const auto idle = graphics.GetBehavior().GetAssignedClip(PetState::Idle);
    previewClip = idle == InvalidSkeletonNode ? 0 : idle;
    previewSpeed = 1;
    previewLoop = true;
    dirty = false;
    status = restored.missingClips ? "저장된 클립을 찾지 못한 항목은 자동 선택으로 복원했습니다." :
        restored.found ? "이 모델의 저장된 설정을 불러왔습니다." : "클립 이름으로 자동 선택한 기본 설정입니다.";
}

bool SettingsWindow::SaveChanges(bool force) {
    if (!dirty && !force) return true;
    std::string error;
    if (!store.Save(modelPath, graphics.GetBehavior().GetSettings(), graphics.GetAnimationClips(), error)) {
        status = "저장 실패: " + error;
        return false;
    }
    dirty = false;
    status = "저장했습니다. 이 모델을 다시 열면 자동으로 복원됩니다.";
    return true;
}

void SettingsWindow::ApplySettings(const PetAnimationSettings& value) {
    graphics.GetBehavior().SetSettings(value);
    dirty = true;
    status = "변경 내용을 적용했습니다. 저장하거나 창을 닫으면 보관됩니다.";
}

bool SettingsWindow::SetAutonomousWalkingEnabled(bool enabled) {
    auto& behavior = graphics.GetBehavior();
    if (enabled && !behavior.HasWalkAnimation()) {
        status = "걷기에 사용할 제자리 클립을 먼저 선택해 주세요.";
        return false;
    }
    auto settings = behavior.GetSettings();
    settings.autonomousWalking = enabled;
    ApplySettings(settings);
    status = enabled ? "자율 걷기를 켰습니다. 설정 창을 닫으면 시작합니다." : "자율 걷기를 껐습니다.";
    return true;
}

void SettingsWindow::Show() {
    POINT cursor{};
    GetCursorPos(&cursor);
    RECT rect{};
    GetWindowRect(hWnd, &rect);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor);
    const int x = std::max(monitor.rcWork.left, std::min(cursor.x + 12, monitor.rcWork.right - (rect.right - rect.left)));
    const int y = std::max(monitor.rcWork.top, std::min(cursor.y + 12, monitor.rcWork.bottom - (rect.bottom - rect.top)));
    SetWindowPos(hWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(hWnd);
}

void SettingsWindow::Render() {
    if (!IsVisible() || IsIconic(hWnd)) return;
    RenderFrame();
    swapChain->Present(0, 0);
}

void SettingsWindow::RenderFrame() {
    ImGui::SetCurrentContext(imgui);
    if (resizeWidth && resizeHeight) {
        graphics.GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
        renderTarget.Reset();
        Require(swapChain->ResizeBuffers(0, resizeWidth, resizeHeight, DXGI_FORMAT_UNKNOWN, 0), "Resize settings window");
        resizeWidth = resizeHeight = 0;
        CreateRenderTarget();
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Animation settings", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    DrawControls();
    ImGui::End();
    ImGui::Render();
    const float color[] = { 0.1f, 0.1f, 0.12f, 1 };
    auto* context = graphics.GetContext();
    context->OMSetRenderTargets(1, renderTarget.GetAddressOf(), nullptr);
    context->ClearRenderTargetView(renderTarget.Get(), color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void SettingsWindow::DrawControls() {
    auto& behavior = graphics.GetBehavior();
    const auto& animator = graphics.GetAnimator();
    const auto& clips = graphics.GetAnimationClips();
    ImGui::TextUnformatted("애니메이션 설정");
    ImGui::TextWrapped("%s", PathLabel(modelPath).c_str());
    ImGui::Text("현재 동작: %s%s", StateNames[static_cast<std::size_t>(behavior.GetState())], behavior.IsPreviewing() ? " · 미리보기 중" : "");
    const auto current = animator.GetClipIndex();
    ImGui::TextWrapped("재생 클립: %s", current == InvalidSkeletonNode ? "기본 자세" : clips[current].displayName.c_str());
    ImGui::Separator();
    const float bodyHeight = std::max(100.0f, ImGui::GetContentRegionAvail().y - 110);
    ImGui::BeginChild("SettingsBody", ImVec2(0, bodyHeight));
    if (ImGui::BeginTabBar("SettingsTabs")) {
        if (ImGui::BeginTabItem("애니메이션")) { DrawAnimationControls(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("자율 걷기")) { DrawWalkingControls(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
    ImGui::Separator();
    if (ImGui::Button("저장")) SaveChanges(true);
    ImGui::SameLine();
    if (ImGui::Button("기본 설정으로 초기화")) { ApplySettings(PetAnimationSettings{}); status = "기본 설정을 적용했습니다."; }
    ImGui::SameLine();
    if (ImGui::Button("저장하고 닫기") && SaveChanges()) { behavior.StopPreview(); ShowWindow(hWnd, SW_HIDE); }
    ImGui::TextWrapped("%s", status.c_str());
    if (dirty) ImGui::TextUnformatted("저장하지 않은 변경 사항이 있습니다.");
}

void SettingsWindow::DrawAnimationControls() {
    auto& behavior = graphics.GetBehavior();
    auto& animator = graphics.GetAnimator();
    const auto& clips = graphics.GetAnimationClips();
    auto settings = behavior.GetSettings();
    bool changed = false;
    if (ImGui::BeginTable("States", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("상태", ImGuiTableColumnFlags_WidthFixed, 65);
        ImGui::TableSetupColumn("클립", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("속도", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("반복", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();
        for (std::size_t s = 0; s < PetStateCount; ++s) {
            auto& config = settings.states[s];
            ImGui::PushID(static_cast<int>(s));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(StateNames[s]);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            changed |= ClipSelector("##clip", behavior, clips, s, config);
            ImGui::TableNextColumn();
            float speed = static_cast<float>(config.speed);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##speed", &speed, 0.05f, 4, "%.2fx", ImGuiSliderFlags_AlwaysClamp)) { config.speed = speed; changed = true; }
            ImGui::TableNextColumn();
            if (s == 3) ImGui::TextUnformatted("1회");
            else if (s == WalkStateIndex) ImGui::TextUnformatted("반복");
            else changed |= ImGui::Checkbox("##loop", &config.loop);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    float transition = static_cast<float>(settings.transitionSeconds);
    if (ImGui::SliderFloat("전환 시간", &transition, 0, 2, "%.2f초", ImGuiSliderFlags_AlwaysClamp)) {
        settings.transitionSeconds = transition; changed = true;
    }
    if (changed) ApplySettings(settings);
    ImGui::TextWrapped("자동 선택에서 잡힘·낙하 클립이 없으면 대기를 사용하고, 착지 클립이 없으면 바로 대기로 돌아갑니다.");
    ImGui::Separator();
    ImGui::TextUnformatted("클립 미리보기");
    if (clips.empty()) ImGui::TextWrapped("이 모델에는 애니메이션 클립이 없습니다. 기본 자세로 표시합니다.");
    else {
        if (ImGui::BeginCombo("클립", previewClip < clips.size() ? clips[previewClip].displayName.c_str() : "선택")) {
            for (std::size_t c = 0; c < clips.size(); ++c) {
                ImGui::PushID(static_cast<int>(c));
                ImGui::BeginDisabled(!behavior.IsClipSupported(c));
                if (ImGui::Selectable(clips[c].displayName.c_str(), previewClip == c)) previewClip = c;
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        bool previewChanged = ImGui::Checkbox("미리보기 반복", &previewLoop);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160);
        previewChanged |= ImGui::SliderFloat("미리보기 속도", &previewSpeed, 0.05f, 4, "%.2fx", ImGuiSliderFlags_AlwaysClamp);
        if (previewChanged) behavior.SetPreviewOptions(previewLoop, previewSpeed);
        ImGui::BeginDisabled(!behavior.IsClipSupported(previewClip));
        if (ImGui::Button("재생 / 처음부터")) behavior.Preview(previewClip, previewLoop, previewSpeed);
        ImGui::EndDisabled();
        if (behavior.IsPreviewing()) {
            ImGui::SameLine();
            if (animator.IsPlaying()) { if (ImGui::Button("일시정지")) behavior.PausePreview(); }
            else if (ImGui::Button("계속")) behavior.ResumePreview();
            ImGui::SameLine();
            if (ImGui::Button("자동 동작으로 복귀")) behavior.StopPreview();
            if (behavior.IsPreviewing() && animator.GetClipIndex() != InvalidSkeletonNode) {
                float time = static_cast<float>(animator.GetTimeSeconds());
                const float duration = static_cast<float>(clips[animator.GetClipIndex()].durationSeconds);
                if (duration > 0 && ImGui::SliderFloat("재생 위치", &time, 0, duration, "%.2f초", ImGuiSliderFlags_AlwaysClamp)) behavior.SeekPreview(time);
            }
            ImGui::TextUnformatted("미리보기 중에는 중력을 멈춥니다.");
        }
    }
}

void SettingsWindow::DrawWalkingControls() {
    auto& behavior = graphics.GetBehavior();
    auto settings = behavior.GetSettings();
    auto& walk = settings.states[WalkStateIndex];
    auto& w = settings.wander;
    bool changed = ClipSelector("걷기 클립", behavior, graphics.GetAnimationClips(), WalkStateIndex, walk);
    if (changed) ApplySettings(settings);
    const bool available = behavior.HasWalkAnimation();
    ImGui::BeginDisabled(!available && !settings.autonomousWalking);
    changed |= ImGui::Checkbox("자율 걷기 사용", &settings.autonomousWalking);
    ImGui::EndDisabled();
    if (!available) ImGui::TextWrapped("사용할 걷기 클립이 없습니다. 길이가 있는 제자리 클립을 선택하면 자율 걷기를 켤 수 있습니다.");
    else ImGui::TextWrapped("걷기를 마치면 정면으로 돌아와 쉽니다. 잡기·회전·수동 조작·미리보기·착지 중에는 멈춥니다.");
    float speed = static_cast<float>(w.pixelsPerSecond);
    if (ImGui::SliderFloat("이동 속도", &speed, 1, 2000, "%.0f px/초", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp)) {
        w.pixelsPerSecond = speed; changed = true;
    }
    float playback = static_cast<float>(walk.speed);
    if (ImGui::SliderFloat("걷기 배속", &playback, 0.05f, 4, "%.2fx", ImGuiSliderFlags_AlwaysClamp)) {
        walk.speed = playback; changed = true;
    }
    ImGui::TextWrapped("이동 속도는 모델 크기와 걷기 배속에 맞춰 조절됩니다. 발이 미끄러져 보이면 이동 속도를 조정하세요.");
    float wait[] = { static_cast<float>(w.minimumWaitSeconds), static_cast<float>(w.maximumWaitSeconds) };
    if (ImGui::SliderFloat2("대기 시간 범위", wait, 0, 30, "%.1f초", ImGuiSliderFlags_AlwaysClamp)) {
        w.minimumWaitSeconds = std::min(wait[0], wait[1]); w.maximumWaitSeconds = std::max(wait[0], wait[1]); changed = true;
    }
    float move[] = { static_cast<float>(w.minimumMoveSeconds), static_cast<float>(w.maximumMoveSeconds) };
    if (ImGui::SliderFloat2("걷는 시간 범위", move, 0.1f, 60, "%.1f초", ImGuiSliderFlags_AlwaysClamp)) {
        w.minimumMoveSeconds = std::min(move[0], move[1]); w.maximumMoveSeconds = std::max(move[0], move[1]); changed = true;
    }
    float turn = static_cast<float>(w.turnSeconds);
    if (ImGui::SliderFloat("방향 전환 시간", &turn, 0, 2, "%.2f초", ImGuiSliderFlags_AlwaysClamp)) { w.turnSeconds = turn; changed = true; }
    ImGui::SetItemTooltip("걷기 시작·경계 반전·걷기 종료 후 정면 복귀에 같은 시간을 사용합니다.");
    float forward = static_cast<float>(w.forwardYawRadians * 180 / Pi);
    if (ImGui::SliderFloat("정면 방향 보정", &forward, -180, 180, "%.0f도", ImGuiSliderFlags_AlwaysClamp)) {
        w.forwardYawRadians = forward * Pi / 180; changed = true;
    }
    ImGui::TextWrapped("이동 방향과 얼굴 방향이 다르면 정면 방향 보정을 조정하세요. 회전 중에는 이동을 멈춥니다. B 키로도 걷기를 켜고 끌 수 있습니다.");
    if (changed) ApplySettings(settings);
}

LRESULT CALLBACK SettingsWindow::WindowProcSetup(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hWnd = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProcForward));
        return self->HandleMessage(window, msg, wParam, lParam);
    }
    return DefWindowProcW(window, msg, wParam, lParam);
}
LRESULT CALLBACK SettingsWindow::WindowProcForward(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    return self ? self->HandleMessage(window, msg, wParam, lParam) : DefWindowProcW(window, msg, wParam, lParam);
}
LRESULT SettingsWindow::HandleMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (imgui && platformReady) {
        ImGui::SetCurrentContext(imgui);
        if (ImGui_ImplWin32_WndProcHandler(window, msg, wParam, lParam)) return 1;
    }
    switch (msg) {
    case WM_CLOSE:
        if (SaveChanges()) { graphics.GetBehavior().StopPreview(); ShowWindow(window, SW_HIDE); }
        return 0;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) { resizeWidth = LOWORD(lParam); resizeHeight = HIWORD(lParam); }
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = { 620, 540 };
        return 0;
    }
    return DefWindowProcW(window, msg, wParam, lParam);
}
