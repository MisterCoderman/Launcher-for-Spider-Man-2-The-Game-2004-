// Spider-Man 2 (2005) Silent Launcher — MR.CODERMAN 2025
// No black console window, no key press required
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <map>
#include <iomanip>
#include <cmath>
#include <io.h>
#include <locale>
#include <codecvt>

#pragma warning(disable: 4996)
#pragma comment(lib, "shell32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

using namespace std;

// === Helper functions ===
BOOL IsRunAsAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    return fIsRunAsAdmin;
}

void RelaunchAsAdmin() {
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = szPath;
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;

    if (!ShellExecuteExW(&sei)) {
        MessageBoxW(NULL, L"Administrator rights required!", L"Spider-Man 2 Launcher", MB_ICONERROR);
        ExitProcess(1);
    }
    ExitProcess(0);
}

void SetXPCompatibility(const wstring& exePath) {
    HKEY hKey;
    const wchar_t* subkey = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        const wchar_t* value = L"WINXPSP3";
        RegSetValueExW(hKey, exePath.c_str(), 0, REG_SZ, (BYTE*)value, (wcslen(value) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
}

double GetHorPlusFOV(double aspect) {
    aspect = floor(aspect * 100 + 0.5) / 100.0;
    static const map<double, double> table = {
        {1.25, 56.85}, {1.56, 68.16}, {1.60, 69.43}, {1.67, 71.64},
        {1.78, 75.18}, {1.85, 77.39}, {2.37, 91.49}, {2.39, 91.94},
        {2.40, 92.20}, {2.76, 100.16}, {3.20, 108.36}, {3.56, 113.99},
        {3.75, 116.75}, {4.00, 120.00}, {4.80, 128.61}, {5.00, 130.42}, {5.33, 133.17}
    };
    auto it = table.find(aspect);
    if (it != table.end()) return it->second;

    const double defFOV = 60.0, defAspect = 4.0 / 3.0;
    double rad = defFOV * 3.14159265358979323846 / 180.0 / 2.0;
    double t = tan(rad) * (aspect / defAspect);
    return 2.0 * atan(t) * 180.0 / 3.14159265358979323846;
}

void UpdateINIResolution(const wstring& file, int w, int h) {
    wifstream in(file);
    if (!in.is_open()) return;
    in.imbue(locale(in.getloc(), new codecvt_utf8<wchar_t>));

    vector<wstring> lines;
    wstring line;
    bool inSection = false;

    while (getline(in, line)) {
        wstring l = line;
        transform(l.begin(), l.end(), l.begin(), ::towlower);

        if (l.find(L"[windrv.windowsclient]") != wstring::npos) inSection = true;

        if (l.find(L"fullscreenviewportx=") == 0)
            line = L"FullscreenViewportX=" + to_wstring(w);
        else if (l.find(L"fullscreenviewporty=") == 0)
            line = L"FullscreenViewportY=" + to_wstring(h);

        lines.push_back(line);
    }
    in.close();

    wofstream out(file);
    out.imbue(locale(out.getloc(), new codecvt_utf8<wchar_t>));
    for (const auto& l : lines) out << l << L"\r\n";
    out.close();
}

void UpdateUserINIFOV(const wstring& file, double fov) {
    wifstream in(file);
    if (!in.is_open()) return;
    in.imbue(locale(in.getloc(), new codecvt_utf8<wchar_t>));

    vector<wstring> lines;
    wstring line;
    bool found = false;

    while (getline(in, line)) {
        wstring l = line;
        transform(l.begin(), l.end(), l.begin(), ::towlower);

        if (l.find(L"desiredfov=") == 0 || l.find(L"defaultfov=") == 0 || l.find(L"fovangle=") == 0) {
            line = L"DesiredFOV=" + to_wstring((int)round(fov));
            found = true;
        }
        lines.push_back(line);
    }
    in.close();

    if (!found) return;

    wofstream out(file);
    out.imbue(locale(out.getloc(), new codecvt_utf8<wchar_t>));
    for (const auto& l : lines) out << l << L"\r\n";
    out.close();
}

bool PatchCutscenes(const wstring& systemDir, int width, int height) {
    wstring path = systemDir + L"\\Webhead.u";
    if (_waccess(path.c_str(), 0) == -1) return false;

    fstream file(path, ios::in | ios::out | ios::binary);
    if (!file) return false;

    file.seekg(0, ios::end);
    size_t size = file.tellg();
    file.seekg(0, ios::beg);

    vector<char> data(size);
    file.read(data.data(), size);

    const char pattern[12] = { 0x4F,0x48,0x1B,0x24, 0x00,0x00,0x70,0x42, 0x00,0x47,0x01,0x00 };
    const char mask[12] = { 'x','x','x','x', '?','?','?','?', 'x','x','x','x' };

    size_t offset = -1;
    for (size_t i = 0; i <= size - 12; ++i) {
        bool match = true;
        for (int j = 0; j < 12; ++j)
            if (mask[j] == 'x' && data[i + j] != pattern[j]) { match = false; break; }
        if (match) { offset = i + 4; break; }
    }

    if (offset == -1) { file.close(); return false; }

    double aspect = (double)width / height;
    double newFOV = GetHorPlusFOV(aspect);
    float fovFloat = (float)newFOV;

    file.seekp(offset);
    file.write((char*)&fovFloat, sizeof(float));
    file.close();
    return true;
}

// === GUI Entry point ===
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    if (!IsRunAsAdmin()) {
        RelaunchAsAdmin();
        return 0;
    }

    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wstring launcherDir = exePath;
    size_t pos = launcherDir.find_last_of(L"\\/");
    if (pos != wstring::npos) launcherDir.erase(pos);

    wstring systemDir = launcherDir + L"\\system";
    wstring exe = systemDir + L"\\Webhead.exe";

    if (_waccess((systemDir + L"\\Webhead.exe").c_str(), 0) == -1 ||
        _waccess((systemDir + L"\\Default.ini").c_str(), 0) == -1) {
        MessageBoxW(NULL, L"Webhead.exe or Default.ini not found!\nPlace launcher in the game folder.",
            L"Spider-Man 2 Launcher", MB_ICONERROR);
        return 1;
    }

    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    OSVERSIONINFO osvi = { sizeof(osvi) };
    GetVersionEx(&osvi);
    if (osvi.dwMajorVersion >= 6) SetXPCompatibility(exe);

    // Resolution
    UpdateINIResolution(systemDir + L"\\Default.ini", width, height);
    if (_waccess((systemDir + L"\\Webhead.ini").c_str(), 0) == 0)
        UpdateINIResolution(systemDir + L"\\Webhead.ini", width, height);

    // FOV
    double fov = GetHorPlusFOV((double)width / height);
    UpdateUserINIFOV(systemDir + L"\\DefUser.ini", fov);
    UpdateUserINIFOV(systemDir + L"\\User.ini", fov);

    // Cutscenes
    PatchCutscenes(systemDir, width, height);

    // Launch game
    ShellExecuteW(NULL, L"open", exe.c_str(), NULL, systemDir.c_str(), SW_SHOWNORMAL);

    return 0;
}