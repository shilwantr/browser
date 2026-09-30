#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int confirm = MessageBoxW(nullptr,
        L"Are you sure you want to uninstall LITE Browser?",
        L"Uninstall LITE Browser", MB_YESNO | MB_ICONQUESTION);

    if (confirm != IDYES) return 0;

    // Delete Start Menu Shortcut
    wchar_t programsFolder[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, programsFolder))) {
        std::wstring shortcut = std::wstring(programsFolder) + L"\\LITE Browser.lnk";
        DeleteFileW(shortcut.c_str());
    }

    // Delete Registry
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LiteBrowser");

    // Ask to delete user data
    int rmData = MessageBoxW(nullptr,
        L"Do you want to delete your bookmarks, history, and cache?",
        L"Uninstall LITE Browser", MB_YESNO | MB_ICONQUESTION);

    if (rmData == IDYES) {
        wchar_t localApp[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localApp))) {
            std::wstring dataDir = std::wstring(localApp) + L"\\LiteBrowser";
            std::wstring cmd = L"rmdir /s /q \"" + dataDir + L"\"";
            _wsystem(cmd.c_str());
        }
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    std::wstring installDir = exePath;

    DeleteFileW((installDir + L"\\LiteBrowser.exe").c_str());
    DeleteFileW((installDir + L"\\WebView2Loader.dll").c_str());

    MessageBoxW(nullptr, L"LITE Browser has been removed from your computer.", L"Uninstall Complete", MB_OK | MB_ICONINFORMATION);

    // Self-delete Uninstall.exe and directory
    wchar_t uninstPath[MAX_PATH];
    GetModuleFileNameW(nullptr, uninstPath, MAX_PATH);
    std::wstring selfDel = L"/c ping 127.0.0.1 -n 2 > nul & del \"" + std::wstring(uninstPath) + L"\" & rmdir \"" + installDir + L"\"";
    ShellExecuteW(nullptr, L"open", L"cmd.exe", selfDel.c_str(), nullptr, SW_HIDE);

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
