#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <iostream>
#include "../src/app/AppConfig.h"
#include "../src/app/ComUtils.h"

using LiteBrowser::ComPtr;

namespace {

bool IsWebView2Installed() {
    HKEY hKey;
    const wchar_t* subKey = L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t ver[64] = { 0 };
        DWORD sz = sizeof(ver);
        if (RegQueryValueExW(hKey, L"pv", nullptr, nullptr, (LPBYTE)ver, &sz) == ERROR_SUCCESS && wcslen(ver) > 0) {
            RegCloseKey(hKey);
            return true;
        }
        RegCloseKey(hKey);
    }
    const wchar_t* userSubKey = L"Software\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, userSubKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t ver[64] = { 0 };
        DWORD sz = sizeof(ver);
        if (RegQueryValueExW(hKey, L"pv", nullptr, nullptr, (LPBYTE)ver, &sz) == ERROR_SUCCESS && wcslen(ver) > 0) {
            RegCloseKey(hKey);
            return true;
        }
        RegCloseKey(hKey);
    }
    return false;
}

bool CreateShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath, const std::wstring& description) {
    ComPtr<IShellLinkW> psl;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)psl.GetAddressOf());
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetPath.c_str());
        psl->SetDescription(description.c_str());
        ComPtr<IPersistFile> ppf;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)ppf.GetAddressOf());
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(shortcutPath.c_str(), TRUE);
            return SUCCEEDED(hr);
        }
    }
    return false;
}

std::wstring GetInstallFolder() {
    wchar_t localApp[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localApp))) {
        return std::wstring(localApp) + L"\\Programs\\LiteBrowser";
    }
    return L"C:\\LiteBrowser";
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    CoInitialize(nullptr);

    // 1. Check WebView2 Runtime
    if (!IsWebView2Installed()) {
        int res = MessageBoxW(nullptr,
            L"LITE Browser requires Microsoft WebView2 Runtime.\nWould you like the installer to download and install it now?",
            L"LITE Browser Setup", MB_YESNO | MB_ICONQUESTION);
        if (res == IDYES) {
            // Download Evergreen bootstrapper
            ShellExecuteW(nullptr, L"open", L"https://go.microsoft.com/fwlink/p/?LinkId=2124703", nullptr, nullptr, SW_SHOW);
            MessageBoxW(nullptr, L"Please complete the WebView2 setup and restart LITE Browser installer.", L"LITE Browser Setup", MB_OK);
            CoUninitialize();
            return 0;
        } else {
            CoUninitialize();
            return 1;
        }
    }

    std::wstring installDir = GetInstallFolder();
    SHCreateDirectoryExW(nullptr, installDir.c_str(), nullptr);

    // Locate source files from installer directory or current dir
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    std::wstring srcDir = exePath;

    std::wstring srcExe = srcDir + L"\\LiteBrowser.exe";
    std::wstring srcDll = srcDir + L"\\WebView2Loader.dll";
    std::wstring srcUninst = srcDir + L"\\Uninstall.exe";

    std::wstring dstExe = installDir + L"\\LiteBrowser.exe";
    std::wstring dstDll = installDir + L"\\WebView2Loader.dll";
    std::wstring dstUninst = installDir + L"\\Uninstall.exe";

    CopyFileW(srcExe.c_str(), dstExe.c_str(), FALSE);
    CopyFileW(srcDll.c_str(), dstDll.c_str(), FALSE);
    if (PathFileExistsW(srcUninst.c_str())) {
        CopyFileW(srcUninst.c_str(), dstUninst.c_str(), FALSE);
    }

    // Create Start Menu Shortcut
    wchar_t programsFolder[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, programsFolder))) {
        std::wstring shortcut = std::wstring(programsFolder) + L"\\LITE Browser.lnk";
        CreateShortcut(dstExe, shortcut, L"Fast, minimal, ad-blocking Windows web browser");
    }

    // Register in Add/Remove Programs
    HKEY hKey;
    const wchar_t* uninstKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LiteBrowser";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, uninstKey, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        auto SetRegStr = [&](const wchar_t* name, const std::wstring& val) {
            RegSetValueExW(hKey, name, 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));
        };
        SetRegStr(L"DisplayName", L"LITE Browser");
        SetRegStr(L"DisplayVersion", LiteBrowser::APP_VERSION);
        SetRegStr(L"Publisher", LiteBrowser::APP_PUBLISHER);
        SetRegStr(L"InstallLocation", installDir);
        SetRegStr(L"DisplayIcon", dstExe);
        SetRegStr(L"UninstallString", dstUninst);
        RegCloseKey(hKey);
    }

    int launch = MessageBoxW(nullptr,
        L"LITE Browser installed successfully!\n\nWould you like to launch LITE Browser now?",
        L"LITE Browser Setup", MB_YESNO | MB_ICONINFORMATION);

    if (launch == IDYES) {
        ShellExecuteW(nullptr, L"open", dstExe.c_str(), nullptr, nullptr, SW_SHOW);
    }

    CoUninitialize();
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
