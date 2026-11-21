#include "Dx12Hook.hpp"
#include "GuiOverlay.hpp"
#include "SteamTransport.hpp"
#include "RemoteProxyManager.hpp"
#include "Interpolation.hpp"
#include "Logger.hpp"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <MinHook.h>
#include <iostream>
#include <vector>

// Forward declare ImGui Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace CrimsonCoop {

bool Dx12Hook::s_isInstalled = false;

typedef HRESULT(WINAPI* tPresent)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(WINAPI* tPresent1)(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters);
typedef HRESULT(WINAPI* tResizeBuffers)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
typedef void(WINAPI* tExecuteCommandLists)(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);

static tPresent oPresent = nullptr;
static tPresent1 oPresent1 = nullptr;
static tResizeBuffers oResizeBuffers = nullptr;
static tExecuteCommandLists oExecuteCommandLists = nullptr;
static WNDPROC oWndProc = nullptr;
static HWND g_hGameWindow = nullptr;

static ID3D12Device* g_d3d12Device = nullptr;
static ID3D12DescriptorHeap* g_d3d12SrvHeap = nullptr;
static ID3D12DescriptorHeap* g_d3d12RtvHeap = nullptr;
static ID3D12CommandQueue* g_d3d12CommandQueue = nullptr;
static ID3D12GraphicsCommandList* g_d3d12CommandList = nullptr;
static ID3D12CommandAllocator* g_d3d12CommandAllocators[4] = { nullptr };
static ID3D12Resource* g_d3d12RenderTargetBuffers[4] = { nullptr };
static D3D12_CPU_DESCRIPTOR_HANDLE g_d3d12RenderTargetHandles[4] = {};
static UINT g_bufferCount = 0;
static bool g_dx12Initialized = false;

SessionManager* g_pActiveSession = nullptr;

static LRESULT CALLBACK Hook_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && (wParam == VK_INSERT || wParam == VK_F2)) {
        GuiOverlay::ToggleVisible();
        Logger::Log("[Dx12Hook] Overlay toggled. Visible: %d\n", GuiOverlay::IsVisible());
        return 0;
    }

    if (GuiOverlay::IsVisible()) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
            return true;
        }
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

static void CleanupRenderTargets() {
    for (UINT i = 0; i < 4; ++i) {
        if (g_d3d12RenderTargetBuffers[i]) {
            g_d3d12RenderTargetBuffers[i]->Release();
            g_d3d12RenderTargetBuffers[i] = nullptr;
        }
    }
}

static void WINAPI Hook_ExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
    if (!g_d3d12CommandQueue && pCommandQueue) {
        D3D12_COMMAND_QUEUE_DESC desc = pCommandQueue->GetDesc();
        if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            g_d3d12CommandQueue = pCommandQueue;
            Logger::Log("[Dx12Hook] Captured Direct Command Queue: %p\n", (void*)g_d3d12CommandQueue);
        }
    }
    oExecuteCommandLists(pCommandQueue, NumCommandLists, ppCommandLists);
}

static void RenderImGuiFrame(IDXGISwapChain* pSwapChain) {
    if (!pSwapChain) return;

    // Fail-safe hotkey toggle via GetAsyncKeyState
    if ((GetAsyncKeyState(VK_F2) & 1) || (GetAsyncKeyState(VK_INSERT) & 1)) {
        GuiOverlay::ToggleVisible();
        Logger::Log("[Dx12Hook] Hotkey F2/Insert detected via GetAsyncKeyState. Visible: %d\n", GuiOverlay::IsVisible());
    }

    if (!g_dx12Initialized) {
        if (FAILED(pSwapChain->GetDevice(IID_PPV_ARGS(&g_d3d12Device)))) {
            return;
        }

        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        g_hGameWindow = desc.OutputWindow;
        g_bufferCount = desc.BufferCount;
        if (g_bufferCount > 4) g_bufferCount = 4;
        if (g_bufferCount == 0) g_bufferCount = 2;

        Logger::Log("[Dx12Hook] SwapChain: Device=%p, Window=%p, BufferCount=%u, Format=%d\n",
                    (void*)g_d3d12Device, (void*)g_hGameWindow, g_bufferCount, desc.BufferDesc.Format);

        // Hook Window Procedure
        if (g_hGameWindow && !oWndProc) {
            oWndProc = (WNDPROC)SetWindowLongPtr(g_hGameWindow, GWLP_WNDPROC, (LONG_PTR)Hook_WndProc);
            Logger::Log("[Dx12Hook] Hooked WndProc for game window: %p\n", (void*)g_hGameWindow);
        }

        // 1. Create RTV Descriptor Heap
        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.NumDescriptors = g_bufferCount;
        rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(g_d3d12Device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_d3d12RtvHeap)))) {
            Logger::Log("[Dx12Hook] Error: Failed to create RTV Descriptor Heap!\n");
            return;
        }

        SIZE_T rtvDescriptorSize = g_d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_d3d12RtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < g_bufferCount; ++i) {
            g_d3d12RenderTargetHandles[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;

            if (SUCCEEDED(pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_d3d12RenderTargetBuffers[i])))) {
                g_d3d12Device->CreateRenderTargetView(g_d3d12RenderTargetBuffers[i], nullptr, g_d3d12RenderTargetHandles[i]);
            }
        }

        // 2. Create Command Allocator for each backbuffer
        for (UINT i = 0; i < g_bufferCount; ++i) {
            if (FAILED(g_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_d3d12CommandAllocators[i])))) {
                Logger::Log("[Dx12Hook] Error: Failed to create Command Allocator %u!\n", i);
                return;
            }
        }

        // 3. Create Graphics Command List
        if (FAILED(g_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_d3d12CommandAllocators[0], nullptr, IID_PPV_ARGS(&g_d3d12CommandList)))) {
            Logger::Log("[Dx12Hook] Error: Failed to create Graphics Command List!\n");
            return;
        }
        g_d3d12CommandList->Close();

        // 4. Create SRV Descriptor Heap for ImGui Font Textures
        D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
        srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvDesc.NumDescriptors = 1;
        srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(g_d3d12Device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&g_d3d12SrvHeap)))) {
            Logger::Log("[Dx12Hook] Error: Failed to create SRV Descriptor Heap!\n");
            return;
        }

        // 5. Initialize ImGui DX12
        DXGI_FORMAT rtvFormat = desc.BufferDesc.Format != DXGI_FORMAT_UNKNOWN ? desc.BufferDesc.Format : DXGI_FORMAT_R8G8B8A8_UNORM;
        if (GuiOverlay::Initialize(g_hGameWindow, g_d3d12Device, g_bufferCount, rtvFormat, g_d3d12SrvHeap)) {
            g_dx12Initialized = true;
            Logger::Log("[Dx12Hook] In-Game ImGui Overlay initialized successfully!\n");
        }
    }

    // Capture queue from SwapChain if not yet set by ExecuteCommandLists
    if (!g_d3d12CommandQueue) {
        pSwapChain->GetDevice(__uuidof(ID3D12CommandQueue), (void**)&g_d3d12CommandQueue);
    }

    static TransformInterpolator g_interpolator(0.05);

    // 1. Pump P2P packets from Steamworks SDR
    SteamTransport::PollAndDispatch(g_pActiveSession, &g_interpolator);

    // 2. Drive remote proxies using Hermite-interpolated transforms
    auto nowTime = std::chrono::steady_clock::now().time_since_epoch();
    double currentSeconds = std::chrono::duration<double>(nowTime).count();
    RemoteProxyManager::Instance().Update(currentSeconds, g_interpolator);

    if (g_dx12Initialized && g_d3d12CommandQueue) {
        IDXGISwapChain3* pSwapChain3 = nullptr;
        if (SUCCEEDED(pSwapChain->QueryInterface(IID_PPV_ARGS(&pSwapChain3)))) {
            UINT backBufferIdx = pSwapChain3->GetCurrentBackBufferIndex();
            pSwapChain3->Release();

            if (backBufferIdx < g_bufferCount && g_d3d12CommandAllocators[backBufferIdx]) {
                g_d3d12CommandAllocators[backBufferIdx]->Reset();

                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = g_d3d12RenderTargetBuffers[backBufferIdx];
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

                g_d3d12CommandList->Reset(g_d3d12CommandAllocators[backBufferIdx], nullptr);
                g_d3d12CommandList->ResourceBarrier(1, &barrier);
                g_d3d12CommandList->OMSetRenderTargets(1, &g_d3d12RenderTargetHandles[backBufferIdx], FALSE, nullptr);

                ID3D12DescriptorHeap* heaps[] = { g_d3d12SrvHeap };
                g_d3d12CommandList->SetDescriptorHeaps(1, heaps);

                // Render Dear ImGui frame
                GuiOverlay::Render(g_pActiveSession);
                ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_d3d12CommandList);

                // Transition backbuffer back to PRESENT
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
                g_d3d12CommandList->ResourceBarrier(1, &barrier);

                g_d3d12CommandList->Close();

                ID3D12CommandList* const ppCommandLists[] = { g_d3d12CommandList };
                g_d3d12CommandQueue->ExecuteCommandLists(1, ppCommandLists);
            }
        }
    }
}

static HRESULT WINAPI Hook_Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    RenderImGuiFrame(pSwapChain);
    return oPresent(pSwapChain, SyncInterval, Flags);
}

static HRESULT WINAPI Hook_Present1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    RenderImGuiFrame(pSwapChain);
    return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
}

static HRESULT WINAPI Hook_ResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    Logger::Log("[Dx12Hook] ResizeBuffers called (%u x %u)\n", Width, Height);
    CleanupRenderTargets();

    if (g_dx12Initialized) {
        ImGui_ImplDX12_InvalidateDeviceObjects();
    }

    HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

    if (g_dx12Initialized && SUCCEEDED(hr)) {
        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        g_bufferCount = desc.BufferCount;
        if (g_bufferCount > 4) g_bufferCount = 4;

        SIZE_T rtvDescriptorSize = g_d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_d3d12RtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < g_bufferCount; ++i) {
            g_d3d12RenderTargetHandles[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;

            if (SUCCEEDED(pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_d3d12RenderTargetBuffers[i])))) {
                g_d3d12Device->CreateRenderTargetView(g_d3d12RenderTargetBuffers[i], nullptr, g_d3d12RenderTargetHandles[i]);
            }
        }
        ImGui_ImplDX12_CreateDeviceObjects();
    }

    return hr;
}

bool Dx12Hook::Install() {
    if (s_isInstalled) return true;

    Logger::Log("[Dx12Hook] Creating dummy DirectX 12 context to obtain VTable addresses...\n");

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProcW, 0L, 0L, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr, L"DummyDX12", nullptr };
    RegisterClassExW(&wc);
    HWND hDummy = CreateWindowW(wc.lpszClassName, L"Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    ID3D12Device* pDummyDevice = nullptr;
    ID3D12CommandQueue* pDummyQueue = nullptr;
    IDXGISwapChain* pDummySwapChain = nullptr;

    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDummyDevice)))) {
        Logger::Log("[Dx12Hook] Failed to create dummy D3D12 device!\n");
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

    // 1. Hook ExecuteCommandLists (VTable index 10) on CommandQueue
    void** pQueueVMT = *reinterpret_cast<void***>(pDummyQueue);
    void* executeCommandListsAddr = pQueueVMT[10];
    MH_CreateHook(executeCommandListsAddr, (LPVOID)&Hook_ExecuteCommandLists, reinterpret_cast<LPVOID*>(&oExecuteCommandLists));

    // 2. Hook Present (VTable index 8) and ResizeBuffers (VTable index 13)
    void** pVMT = *reinterpret_cast<void***>(pDummySwapChain);
    void* presentAddress = pVMT[8];
    void* resizeBuffersAddress = pVMT[13];

    MH_CreateHook(presentAddress, (LPVOID)&Hook_Present, reinterpret_cast<LPVOID*>(&oPresent));
    MH_CreateHook(resizeBuffersAddress, (LPVOID)&Hook_ResizeBuffers, reinterpret_cast<LPVOID*>(&oResizeBuffers));

    // 3. Hook Present1 (VTable index 22) on IDXGISwapChain1
    IDXGISwapChain1* pDummySwapChain1 = nullptr;
    if (SUCCEEDED(pDummySwapChain->QueryInterface(IID_PPV_ARGS(&pDummySwapChain1)))) {
        void** pVMT1 = *reinterpret_cast<void***>(pDummySwapChain1);
        void* present1Address = pVMT1[22];
        MH_CreateHook(present1Address, (LPVOID)&Hook_Present1, reinterpret_cast<LPVOID*>(&oPresent1));
        pDummySwapChain1->Release();
    }

    MH_EnableHook(MH_ALL_HOOKS);

    // Clean up dummy objects
    pDummySwapChain->Release();
    pDummyQueue->Release();
    pDummyDevice->Release();
    pFactory->Release();
    DestroyWindow(hDummy);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    s_isInstalled = true;
    Logger::Log("[Dx12Hook] Successfully installed DX12 SwapChain & CommandQueue hooks!\n");
    return true;
}

void Dx12Hook::Remove() {
    if (!s_isInstalled) return;

    MH_DisableHook(MH_ALL_HOOKS);
    if (g_hGameWindow && oWndProc) {
        SetWindowLongPtr(g_hGameWindow, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        oWndProc = nullptr;
    }

    CleanupRenderTargets();
    GuiOverlay::Shutdown();
    s_isInstalled = false;
    Logger::Log("[Dx12Hook] Removed all hooks.\n");
}

} // namespace CrimsonCoop
