#pragma once

#include "dllmain.h"

#include "Py4GW.h"
#include "Headers.h"
#include "WinUser.h"
#include "hidusage.h"
#include <cassert>

#ifndef ASSERT
#define ASSERT(expr) \
    ((void)(!!(expr) || \
    (Logger::Instance().LogError( \
        std::string("Assertion failed: ") + #expr + \
        ", File: " + __FILE__ + \
        ", Line: " + std::to_string(__LINE__) \
    ), 0)))
#endif


// Global module handle
HMODULE g_DllModule = nullptr;

// Forward declare the WndProc handler for ImGui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// DLLMain singleton implementation
DLLMain& DLLMain::Instance() {
    static DLLMain instance;
    return instance;
}

DLLMain::DLLMain()
    : running(false)
    , initialized(false)
    , imgui_initialized(false)
    , render_hook_attached(false)
    , wndproc_attached(false)
    , old_wndproc(nullptr)
    , gw_window_handle(nullptr)
    , last_tick(0)
{
}

DLLMain::~DLLMain() {
    Terminate();
}

bool DLLMain::Initialize() {

    Logger::Instance().SetLogFile("Py4GW_injection_log.txt");

    // Initialize GWCA
    if (!InitializeGWCA()) {
        Logger::Instance().LogError("Failed to initialize GWCA");
        return false;
    }

    // Get Guild Wars window handle
    gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
    if (!gw_window_handle) {
		Logger::Instance().LogError("Failed to get GW window handle");
        return false;
    }

    // Attach render hook
    if (!AttachRenderHook()) {
        Logger::Instance().LogError("Failed to attach render hook");
        return false;
    }

    // Attach window procedure hook
    if (!AttachWndProc()) {
		Logger::Instance().LogError("Failed to attach window procedure");
        return false;
    }

    running = true;
    initialized = true;
    last_tick = GetTickCount64();
    Logger::Instance().LogInfo("DLL Initialize complete.");
    return true;
}


void DLLMain::Terminate() {
    if (!initialized) return;
	GW::GameThread::RemoveGameThreadCallback(&Update_Entry);
    Py4GW::Instance().Terminate();

    // Clean up ImGui
    if (imgui_initialized) {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        imgui_initialized = false;
    }

    DetachWndProc();
    DetachRenderHook();

    FreeLibraryAndExitThread(g_DllModule, EXIT_SUCCESS);

    running = false;
    initialized = false;
}

void SetupImGuiFonts() {
    ImGuiIO& io = ImGui::GetIO();

    // Increase font atlas size
    io.Fonts->TexDesiredWidth = 4096;

    // Configure font loading
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 1; // Higher horizontal oversampling
    fontConfig.OversampleV = 1; // Higher vertical oversampling

    // Add default font as fallback
    io.Fonts->AddFontDefault();

    // Load custom font with configuration
    std::string fontPath = dllDirectory + "\\fonts\\friz-quadrata-std-medium-5870338ec7ef8.otf";
    ImFont* customFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 14.0f, &fontConfig);
    io.FontDefault = customFont; // Set as default font

    // Configure Font Awesome font loading
    fontConfig.MergeMode = true; // Merge with the existing font to support icons
	fontConfig.PixelSnapH = true; // Enable pixel snapping for consistent rendering
    fontConfig.GlyphOffset = ImVec2(0.0f, 5.0f); // Adjust Y-axis offset as needed


    // Define icon range for Font Awesome
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 }; // Range of Font Awesome icons

    // Load Font Awesome fonts

    std::string faRegularPath = dllDirectory + "\\fonts\\Font Awesome 6 Free-Regular-400.otf";
    std::string faSolidPath = dllDirectory + "\\fonts\\Font Awesome 6 Free-Solid-900.otf";

    io.Fonts->AddFontFromFileTTF(faRegularPath.c_str(), 20.0f, &fontConfig, icons_ranges);
    io.Fonts->AddFontFromFileTTF(faSolidPath.c_str(), 20.0f, &fontConfig, icons_ranges);


    // Create DirectX 9 device objects
    ImGui_ImplDX9_CreateDeviceObjects();
}





void ApplyImGuiTheme() {
    ImGuiStyle* style = &ImGui::GetStyle();

    style->WindowPadding = ImVec2(10, 10);
    style->WindowRounding = 5.0f;
    style->FramePadding = ImVec2(5, 5);
    style->FrameRounding = 4.0f;
    style->ItemSpacing = ImVec2(10, 6);
    style->ItemInnerSpacing = ImVec2(6, 4);
    style->IndentSpacing = 20.0f;
    style->ScrollbarSize = 20.0f;
    style->ScrollbarRounding = 9.0f;
    style->GrabMinSize = 5.0f;
    style->GrabRounding = 3.0f;

    style->Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style->Colors[ImGuiCol_WindowBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.8f);
    //style->Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.15f, 0.2f, 1.00f);
	style->Colors[ImGuiCol_TabHovered] = ImVec4(0.2f, 0.3f, 0.4f, 1.00f);
	style->Colors[ImGuiCol_TabActive] = ImVec4(0.4f, 0.5f, 0.6f, 1.00f);


    style->Colors[ImGuiCol_PopupBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.8f);
    style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
    style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.1f, 0.1f, 0.1f, 0.5f);
    style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.8f);
    style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.02f, 0.02f, 0.02f, 0.8f);
    style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
    style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.8f);
    style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.2f, 0.3f, 0.3f, 0.5f);
    style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.2f, 0.3f, 0.4f, 0.5f);
    style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.2f, 0.3f, 0.4f, 0.5f);
    //style->Colors[ImGuiCol_ComboBg] = ImVec4(0.19f, 0.18f, 0.21f, 1.00f);
    style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.2f, 0.3f, 0.3f, 0.5f);
    style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.2f, 0.3f, 0.4f, 0.5f);
    style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.15f, 0.2f, 1.00f);
    style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.2f, 0.3f, 0.4f, 1.00f);
    style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.5f, 0.6f, 1.00f);
    style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    //style->Colors[ImGuiCol_Column] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    //style->Colors[ImGuiCol_ColumnHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    //style->Colors[ImGuiCol_ColumnActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    //style->Colors[ImGuiCol_CloseButton] = ImVec4(0.40f, 0.39f, 0.38f, 0.16f);
    //style->Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.40f, 0.39f, 0.38f, 0.39f);
    //style->Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.40f, 0.39f, 0.38f, 1.00f);
    style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.1f, 1.00f, 0.1f, 0.43f);
    //style->Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);
}


bool DLLMain::InitializeGWCA() {
    return GW::Initialize();
}

bool DLLMain::InitializePy4GW() {
	return Py4GW::Instance().Initialize();
}

bool DLLMain::InitializeImGui(IDirect3DDevice9* device) {
    // Check if ImGui context exists
    if (ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();

        // Check if backends are already initialized
        if (io.BackendPlatformUserData && io.BackendRendererUserData) {
            imgui_initialized = true; // Mark as initialized
            
            return true; // Already initialized
        }

        // Backends not fully initialized, continue initialization
    }
    else {
        // Create ImGui context if it doesn't exist
        ImGui::CreateContext();
        SetupImGuiFonts();
		ApplyImGuiTheme();
    }

    // Ensure device and window handle are valid
    gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
    if (!gw_window_handle || !device) return false;

    OldWndProc = (WNDPROC)SetWindowLongPtr(gw_window_handle, GWLP_WNDPROC, (LONG_PTR)WndProc);

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = false;
    io.MouseDrawCursor = false;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;

    ImGui_ImplWin32_Init(gw_window_handle);
    ImGui_ImplDX9_Init(device);

    GW::Render::SetResetCallback([](IDirect3DDevice9*) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        });

    imgui_initialized = true;
    return true;
}



void DLLMain::Update(GW::HookStatus*) {
    if (!running || !initialized) return;

	// Update Py4GW
	if (!gw_client_window_handle) { gw_client_window_handle = gw_window_handle; }

	Py4GW::Instance().Update();

    static ULONGLONG last_tick_count;
    if (last_tick_count == 0) {
        last_tick_count = GetTickCount64();
    }

    const auto tick = GetTickCount64();
    const auto delta = tick - last_tick_count;
    const auto delta_f = static_cast<float>(delta) / 1000.f;


    last_tick_count = tick;

}


void DLLMain::Draw(IDirect3DDevice9* device) {
    if (!initialized) return;

    // Initialize ImGui on first render
    if (!imgui_initialized && !InitializeImGui(device)) {
        return;
    }

    // Validate device state
    HRESULT result = device->TestCooperativeLevel();
    if (FAILED(result)) {
        // Device lost, invalidate ImGui
        if (imgui_initialized) {
            ImGui_ImplDX9_InvalidateDeviceObjects();
            imgui_initialized = false; // Force reinitialization later
        }
        return; // Skip rendering
    }

    // Start new frame
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Draw the main window
    Py4GW::Instance().Draw(device);

    // Render ImGui
    ImGui::EndFrame();
    ImGui::Render();

    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    // Reset device states
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
}



bool DLLMain::AttachRenderHook() {
    if (render_hook_attached) return true;

    GW::Render::SetRenderCallback([](IDirect3DDevice9* device) {
        DLLMain::Instance().Draw(device);
        });

    render_hook_attached = true;
    return true;
}

void DLLMain::DetachRenderHook() {
    if (!render_hook_attached) return;

    GW::Render::SetRenderCallback(nullptr);
    render_hook_attached = false;
}

bool DLLMain::AttachWndProc()
{
    if (wndproc_attached) return true;

    Logger::Instance().LogInfo("installing event handler.");
    gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
    old_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(gw_window_handle, GWL_WNDPROC, reinterpret_cast<LONG>(SafeWndProc)));
    Logger::Instance().LogInfo("Installed input event handler, oldwndproc = 0x%X\n");

    // RegisterRawInputDevices to be able to receive WM_INPUT via WndProc
    static RAWINPUTDEVICE rid;
    rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid.usUsage = HID_USAGE_GENERIC_MOUSE;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = gw_window_handle;
    ASSERT(RegisterRawInputDevices(&rid, 1, sizeof(rid)));

    wndproc_attached = true;
    return true;
}

void DLLMain::DetachWndProc() {
    if (!wndproc_attached || !old_wndproc) return;

    SetWindowLongPtr(gw_window_handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old_wndproc));
    wndproc_attached = false;
    old_wndproc = nullptr;
}

LRESULT CALLBACK DLLMain::SafeWndProc(const HWND hWnd, const UINT Message, const WPARAM wParam, const LPARAM lParam) noexcept
{
    __try {
        return WndProc(hWnd, Message, wParam, lParam);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
    }
}


LRESULT CALLBACK DLLMain::WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
    static bool right_mouse_down = false; // Track problematic actions
    auto& instance = DLLMain::Instance();
    ImGuiIO& io = ImGui::GetIO();

    if (Message == WM_RBUTTONUP) {right_mouse_down = false;}
    if (Message == WM_RBUTTONDOWN) {right_mouse_down = true;}
    if (Message == WM_RBUTTONDBLCLK) {right_mouse_down = true;}

	if (right_mouse_down) {
		return CallWindowProc(instance.old_wndproc, hWnd, Message, wParam, lParam);
	}

	if (Message == WM_LBUTTONDOWN && !dragging_initialized) {
		if (io.WantCaptureMouse) {
            is_dragging = true;
			is_dragging_imgui = true;
		}
		else {
			is_dragging = true;
			is_dragging_imgui = false;
            return CallWindowProc(instance.old_wndproc, hWnd, Message, wParam, lParam);
		}
	}

	if (Message == WM_LBUTTONUP) {
		is_dragging = false;
		is_dragging_imgui = false;
		dragging_initialized = false;
	}

	if (is_dragging) {
		dragging_initialized = true;
		if (is_dragging_imgui) {
			ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam);
            return TRUE;
		}
		else { return CallWindowProc(instance.old_wndproc, hWnd, Message, wParam, lParam); }
	}

    ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam);
    io = ImGui::GetIO();
	if (io.WantCaptureMouse && 
		(Message == WM_MOUSEMOVE || 
         Message == WM_LBUTTONDOWN || 
	     Message == WM_LBUTTONDBLCLK ||
         Message == WM_RBUTTONDBLCLK ||
	     Message == WM_RBUTTONDOWN ||
	     Message == WM_RBUTTONUP ||
         Message == WM_LBUTTONUP || 
         Message == WM_MOUSEWHEEL || 
         Message == WM_MOUSEHWHEEL)) {
            return TRUE;
    }

	if (io.WantCaptureKeyboard && io.WantTextInput && (Message == WM_KEYDOWN || Message == WM_KEYUP || Message == WM_CHAR)) {
		return TRUE;
	}

    return CallWindowProc(instance.old_wndproc, hWnd, Message, wParam, lParam);
}






DWORD WINAPI MainLoop(LPVOID) {
    auto& dll = DLLMain::Instance();

    if (!dll.Initialize()) {
        return 1;
    }

    // Main loop
    while (dll.IsRunning()) {
        if (dll_shutdown) { // Check shutdown flag
            dll.Terminate();
            break; // Exit the loop
        }
        dll.Update();
        Sleep(10);
    }

    return 0;
}

std::string WStringToString(const std::wstring& s)
{
    // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
    if (s.empty()) {
        return "Error In Wstring";
    }
    // NB: GW uses code page 0 (CP_ACP)
    const auto size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), strTo.data(), size_needed, NULL, NULL);
    return strTo;
}

std::string GetDllDirectory(HMODULE hDllModule) {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(hDllModule, path, MAX_PATH) == 0) {
        // Handle error if needed
        return "";
    }

    std::wstring full_path = path;

    // Find the last backslash in the path
    size_t last_slash_idx = full_path.find_last_of(L"\\/");
    if (last_slash_idx != std::wstring::npos) {
        // Erase everything after the last backslash
        full_path.erase(last_slash_idx);
    }

    return WStringToString(full_path);
}

// DLL Entry Point
//BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
BOOL WINAPI DllMain(_In_ const HMODULE hDllHandle, _In_ const DWORD reason, _In_opt_ const LPVOID){

    DisableThreadLibraryCalls(hDllHandle);
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            g_DllModule = hDllHandle;
            
            dllDirectory = GetDllDirectory(g_DllModule);

            Logger::Instance().SetLogFile("Py4GW_injection_log.txt");

		    if (!Logger::Instance().LogInfo("Starting injection process.")) {
                MessageBoxA(nullptr, "Error writitng to file", "ERROR", MB_OK);
                FreeLibraryAndExitThread(g_DllModule, EXIT_SUCCESS);
			    return FALSE;
	        }

            auto& dll = DLLMain::Instance();
            dll.Initialize();

            // Create main thread
            HANDLE hThread = CreateThread(
                nullptr, 
                0, 
                reinterpret_cast<LPTHREAD_START_ROUTINE>(MainLoop),
                nullptr, 
                0, 
                nullptr);
            if (hThread) {
                CloseHandle(hThread);
                Logger::Instance().LogInfo("Main thread Created.");
            }
            else {
                Logger::Instance().LogError("Unable to create main thread.");
                return FALSE;
            }
            break;
        }

        case DLL_PROCESS_DETACH: {
            DLLMain::Instance().Terminate();
            break;
        }
    }
    return TRUE;
}