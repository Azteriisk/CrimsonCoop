#include "Dx12Hook.hpp"
#include "GuiOverlay.hpp"
#include <imgui.h>
#include <MinHook.h>
#include <iostream>
#include <vector>

// Forward declare ImGui Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace CrimsonCoop {

bool Dx12Hook::s_isInstalled = false;

typedef HRESULT(WINAPI* tPresent)(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(WINAPI* tResizeBuffers)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

static tPresent oPresent = nullptr;
static tResizeBuffers oResizeBuffers = nullptr;
static WNDPROC oWndProc = nullptr;
static HWND g_hGameWindow = nullptr;

static ID3D12Device* g_d3d12Device = nullptr;
static ID3D12DescriptorHeap* g_d3d12DescriptorHeap = nullptr;
static ID3D12CommandQueue* g_d3d12CommandQueue = nullptr;
static ID3D12GraphicsCommandList* g_d3d12CommandList = nullptr;
static ID3D12CommandAllocator* g_d3d12CommandAllocators[4] = { nullptr };
static ID3D12Resource* g_d3d12RenderTargetBuffers[4] = { nullptr };
static D3D12_CPU_DESCRIPTOR_HANDLE g_d3d12RenderTargetHandles[4] = {};
static UINT g_bufferCount = 0;
static bool g_dx12Initialized = false;

// Externally accessible session pointer for the overlay
SessionManager* g_pActiveSession = nullptr;

static LRESULT CALLBACK Hook_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (GuiOverlay::IsVisible()) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
            return true;
        }
    }

    // Toggle overlay with Insert or F2 key
    if (uMsg == WM_KEYDOWN && (wParam == VK_INSERT || wParam == VK_F2)) {
        GuiOverlay::ToggleVisible();
        return 0;
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

static HRESULT WINAPI Hook_Present(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_dx12Initialized) {
        if (SUCCEEDED(pSwapChain->GetDevice(IID_PPV_ARGS(&g_d3d12Device)))) {
            DXGI_SWAP_CHAIN_DESC desc;
            pSwapChain->GetDesc(&desc);
            g_hGameWindow = desc.OutputWindow;
            g_bufferCount = desc.BufferCount;

            // Hook window procedure for mouse/keyboard inputs
            if (g_hGameWindow && !oWndProc) {
                oWndProc = (WNDPROC)SetWindowLongPtr(g_hGameWindow, GWLP_WNDPROC, (LONG_PTR)Hook_WndProc);
            }

            // Create Descriptor Heap for SRV (ImGui font textures)
            D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
            srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvDesc.NumDescriptors = 1;
            srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            g_d3d12Device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&g_d3d12DescriptorHeap));

            // Initialize ImGui DX12 backend
            if (GuiOverlay::Initialize(g_hGameWindow, g_d3d12Device, g_bufferCount, g_d3d12DescriptorHeap)) {
                g_dx12Initialized = true;
                std::cout << "[Dx12Hook] In-Game ImGui Overlay initialized successfully!\n";
            }
        }
    }

    if (g_dx12Initialized) {
        GuiOverlay::Render(g_pActiveSession);
    }

    return oPresent(pSwapChain, SyncInterval, Flags);
}

static HRESULT WINAPI Hook_ResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    if (g_dx12Initialized) {
        GuiOverlay::Shutdown();
        g_dx12Initialized = false;
    }
    return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

bool Dx12Hook::Install() {
    if (s_isInstalled) return true;

    // Create dummy window to obtain DXGI SwapChain VTable
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProcW, 0L, 0L, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr, L"DummyDX12", nullptr };
    RegisterClassExW(&wc);
    HWND hDummy = CreateWindowW(wc.lpszClassName, L"Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    ID3D12Device* pDummyDevice = nullptr;
    ID3D12CommandQueue* pDummyQueue = nullptr;
    IDXGISwapChain* pDummySwapChain = nullptr;

    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDummyDevice)))) {
        DestroyWindow(hDummy);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    pDummyDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pDummyQueue));

    IDXGIFactory4* pFactory = nullptr;
    CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));

    DXGI_SWAP_CHAIN_DESC scDesc = {};
    scDesc.BufferCount = 2;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow = hDummy;
    scDesc.SampleDesc.Count = 1;
    scDesc.Windowed = TRUE;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    pFactory->CreateSwapChain(pDummyQueue, &scDesc, &pDummySwapChain);

    void** pVMT = *reinterpret_cast<void***>(pDummySwapChain);
    void* presentAddress = pVMT[8];       // IDXGISwapChain::Present
    void* resizeBuffersAddress = pVMT[13]; // IDXGISwapChain::ResizeBuffers

    // Install MinHook on Present and ResizeBuffers
    MH_CreateHook(presentAddress, (LPVOID)&Hook_Present, reinterpret_cast<LPVOID*>(&oPresent));
    MH_CreateHook(resizeBuffersAddress, (LPVOID)&Hook_ResizeBuffers, reinterpret_cast<LPVOID*>(&oResizeBuffers));
    MH_EnableHook(MH_ALL_HOOKS);

    // Clean up dummy objects
    pDummySwapChain->Release();
    pDummyQueue->Release();
    pDummyDevice->Release();
    pFactory->Release();
    DestroyWindow(hDummy);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    s_isInstalled = true;
    std::cout << "[Dx12Hook] Successfully hooked DirectX 12 SwapChain::Present.\n";
    return true;
}

void Dx12Hook::Remove() {
    if (!s_isInstalled) return;

    MH_DisableHook(MH_ALL_HOOKS);
    if (g_hGameWindow && oWndProc) {
        SetWindowLongPtr(g_hGameWindow, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        oWndProc = nullptr;
    }

    GuiOverlay::Shutdown();
    s_isInstalled = false;
}

} // namespace CrimsonCoop
