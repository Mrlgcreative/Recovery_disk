/*
 * main_gui.cpp
 * --------------------------------------------------------------------------
 * Point d'entree de l'interface graphique.
 *
 * Setup :
 *   1. Fenetre Win32 native
 *   2. Contexte DirectX 11
 *   3. Dear ImGui (Win32 + DX11 backends) avec Docking
 *   4. Boucle de rendu appelant GUI::render_frame()
 *
 * Pour compiler : necessite d3d11.lib, dxgi.lib (linkage dans CMakeLists)
 * --------------------------------------------------------------------------
 */

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "gui.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <dxgi.h>

/* ----- DirectX11 globals ----- */
static ID3D11Device*           g_pd3dDevice       = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain       = nullptr;
static ID3D11RenderTargetView* g_mainRenderTarget  = nullptr;

/* ----- Forward declarations ----- */
static bool  CreateDeviceD3D(HWND hWnd);
static void  CleanupDeviceD3D();
static void  CreateRenderTarget();
static void  CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* Forward declare message handler from imgui_impl_win32.cpp */
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* ==========================================================================
 *  WinMain - Point d'entree Windows
 * ========================================================================== */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    /* -- Fenetre Win32 -- */
    WNDCLASSEXA wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXA);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "HddUnlockGUI";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(
        0, wc.lpszClassName,
        "HDD Password Recovery Tool v1.0.0",
        WS_OVERLAPPEDWINDOW,
        100, 100, 1400, 850,
        nullptr, nullptr, hInstance, nullptr);

    /* -- DirectX 11 -- */
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassA(wc.lpszClassName, hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    /* -- ImGui setup -- */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    /* Ne pas creer de imgui.ini pour forcer notre layout au prochain lancement */
    io.IniFilename = "imgui.ini";

    /* Theme personnalise */
    GUI::apply_theme();

    /* Ajuster la taille de police par defaut */
    io.FontGlobalScale = 1.0f;

    /* Backends */
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    /* -- Application GUI -- */
    GUI gui;

    /* Scan automatique au demarrage */
    gui.do_scan();

    /* -- Boucle de rendu -- */
    const ImVec4 clear_color(0.05f, 0.05f, 0.07f, 1.00f);
    bool running = true;

    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        /* Raccourcis clavier */
        if (GetAsyncKeyState(VK_F5) & 1) gui.do_scan();

        /* Nouvelle frame */
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        /* Rendu de tous les panels */
        gui.render_frame();

        /* --- Rendu --- */
        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTarget, nullptr);
        const float clear_rgba[4] = {
            clear_color.x, clear_color.y, clear_color.z, clear_color.w
        };
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTarget, clear_rgba);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);  /* VSync on */
    }

    /* -- Cleanup -- */
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, hInstance);

    return 0;
}

/* ==========================================================================
 *  DirectX 11 Helpers
 * ========================================================================== */

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, 2,
        D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel,
        &g_pd3dDeviceContext);

    if (FAILED(hr)) return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain)       { g_pSwapChain->Release();       g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)       { g_pd3dDevice->Release();       g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTarget);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTarget) { g_mainRenderTarget->Release(); g_mainRenderTarget = nullptr; }
}

/* ==========================================================================
 *  Window Procedure
 * ========================================================================== */

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0,
                (UINT)LOWORD(lParam), (UINT)HIWORD(lParam),
                DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

#else /* Non-Windows */

#include <cstdio>

int main() {
    fprintf(stderr, "L'interface GUI necessite Windows (DirectX 11).\n");
    fprintf(stderr, "Utilisez 'hdd_unlock' pour l'interface CLI.\n");
    return 1;
}

#endif
