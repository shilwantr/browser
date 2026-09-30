#include <windows.h>
#include <commctrl.h>
#include "AppConfig.h"
#include "../ui/BrowserWindow.h"
#include "../storage/StorageManager.h"
#include "../adblock/AdBlockEngine.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Initialize COM for UI thread
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return 1;
    }

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Initialize Persistent Storage
    LiteBrowser::StorageManager::GetInstance().Initialize();

    // Initialize AdBlock & Tracker Blocking Engine
    LiteBrowser::AdBlockEngine::GetInstance().Initialize();

    // Create Main Browser Window
    LiteBrowser::BrowserWindow mainWindow;
    if (!mainWindow.Create()) {
        MessageBoxW(nullptr, L"Failed to create main browser window.", LiteBrowser::APP_NAME, MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }

    mainWindow.Show(nCmdShow);

    // Standard Win32 Message Loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    LiteBrowser::StorageManager::GetInstance().Close();
    CoUninitialize();

    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR /*lpCmdLine*/, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
