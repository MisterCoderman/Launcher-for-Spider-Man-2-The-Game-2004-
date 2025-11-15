// Spider-Man 2 (2005) Launcher
// VS 2015 + v140_xp | MR.CODERMAN 2025

#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <io.h>
#include <fcntl.h>
#include <locale>
#include <codecvt>
#include <cmath>
#include <map>
#include <iomanip>
#include <ios>

#pragma warning(disable: 4996)
#pragma comment(lib, "shell32.lib")

using namespace std;

// === ФУНКЦИИ АДМИНА И XP (БЕЗ ИЗМЕНЕНИЙ) ===
BOOL IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

void RunAsAdmin() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    SHELLEXECUTEINFO sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.nShow = SW_NORMAL;
    if (!ShellExecuteExW(&sei)) {
        system("pause");
        ExitProcess(1);
    }
    ExitProcess(0);
}

void SetXPCompatibilityForGame(const wstring& gameExePath) {
    HKEY hKey;
    const wchar_t* subkey = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        const wchar_t* compat = L"WINXPSP3";
        RegSetValueExW(hKey, gameExePath.c_str(), 0, REG_SZ, (BYTE*)compat, (wcslen(compat) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
        cout << "XP SP3 compatibility set for Webhead.exe." << endl;
    }
}

wstring to_lower(wstring s) {
    transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

// === Hor+ FOV Table ===
double get_hor_plus_fov(double aspect) {
    aspect = floor(aspect * 100 + 0.5) / 100.0;
    static const map<double, double> fov_table = {
        {1.25, 56.85}, {1.56, 68.16}, {1.60, 69.43}, {1.67, 71.64},
        {1.78, 75.18}, {1.85, 77.39}, {2.37, 91.49}, {2.39, 91.94},
        {2.40, 91.96}, {2.40, 92.20}, {2.76, 100.16}, {3.20, 108.36},
        {3.56, 113.99}, {3.75, 116.75}, {4.00, 120.00}, {4.80, 128.61},
        {5.00, 130.42}, {5.33, 133.17}
    };
    auto it = fov_table.find(aspect);
    if (it != fov_table.end()) return it->second;

    const double default_fov = 60.0, default_aspect = 4.0 / 3.0;
    double rad = default_fov * 3.14159265358979323846 / 180.0 / 2.0;
    double tan_fov = tan(rad);
    double new_rad = 2.0 * atan(tan_fov * (aspect / default_aspect));
    return new_rad * 180.0 / 3.14159265358979323846;
}

// === Обновление разрешения ===
void update_ini_resolution(const wstring& filename, int width, int height) {
    wifstream in(filename);
    if (!in.is_open()) return;
    in.imbue(locale(in.getloc(), new codecvt_utf8<wchar_t>()));
    vector<wstring> lines;
    wstring line;
    bool section_found = false, found_x = false, found_y = false;

    while (getline(in, line)) {
        wstring lower = to_lower(line);
        if (lower.find(L"[windrv.windowsclient]") != wstring::npos) section_found = true;
        if (lower.find(L"fullscreenviewportx=") == 0) { line = L"FullscreenViewportX=" + to_wstring(width); found_x = true; }
        else if (lower.find(L"fullscreenviewporty=") == 0) { line = L"FullscreenViewportY=" + to_wstring(height); found_y = true; }
        lines.push_back(line);
    }
    in.close();

    if (!found_x) { if (!section_found) lines.push_back(L"[WinDrv.WindowsClient]"); lines.push_back(L"FullscreenViewportX=" + to_wstring(width)); }
    if (!found_y) { if (!section_found || found_x) lines.push_back(L"[WinDrv.WindowsClient]"); lines.push_back(L"FullscreenViewportY=" + to_wstring(height)); }

    wofstream out(filename);
    out.imbue(locale(out.getloc(), new codecvt_utf8<wchar_t>()));
    for (const auto& l : lines) out << l << L"\r\n";
    out.close();
}

// === НОВАЯ ФУНКЦИЯ: ТОЛЬКО ЗАМЕНА FOV (БЕЗ СОЗДАНИЯ ФАЙЛОВ) ===
void update_user_ini_fov(const wstring& filename, double fov) {
    // **КРИТИЧНО: Если файла НЕТ — ПРОПУСТИ!**
    wifstream in(filename);
    if (!in.is_open()) {
        cout << "Skipped " << wstring_convert<codecvt_utf8<wchar_t>>().to_bytes(filename) << " (file not found)" << endl;
        return;
    }

    in.imbue(locale(in.getloc(), new codecvt_utf8<wchar_t>()));
    vector<wstring> lines;
    wstring line;
    bool found_any_fov = false;

    // Читаем ВСЕ строки и ИЩЕМ ТОЛЬКО FOV поля
    while (getline(in, line)) {
        wstring lower = to_lower(line);

        // Заменяем ТОЛЬКО FOV значения
        if (lower.find(L"desiredfov=") == 0) {
            line = L"DesiredFOV=" + to_wstring(fov);
            found_any_fov = true;
        }
        else if (lower.find(L"defaultfov=") == 0) {
            line = L"DefaultFOV=" + to_wstring(fov);
            found_any_fov = true;
        }
        else if (lower.find(L"fovangle=") == 0) {
            line = L"FOVAngle=" + to_wstring(fov);
            found_any_fov = true;
        }

        lines.push_back(line);  // СОХРАНЯЕМ ВСЁ ОСТАЛЬНОЕ КАК ЕСТЬ
    }
    in.close();

    // Если FOV поля вообще НЕ НАЙДЕНЫ — НЕ добавляем (только замена!)
    if (!found_any_fov) {
        cout << "No FOV fields found in " << wstring_convert<codecvt_utf8<wchar_t>>().to_bytes(filename) << " — skipped" << endl;
        return;
    }

    // Сохраняем с изменениями
    wofstream out(filename);
    out.imbue(locale(out.getloc(), new codecvt_utf8<wchar_t>()));
    for (const auto& l : lines) out << l << L"\r\n";
    out.close();

    cout << "FOV updated in " << wstring_convert<codecvt_utf8<wchar_t>>().to_bytes(filename) << ": " << fixed << setprecision(2) << fov << "°" << endl;
}

// === Патч кат-сцен ===
bool patch_webhead_u(const wstring& system_dir, int width, int height) {
    wstring filepath = system_dir + L"\\Webhead.u";
    if (_waccess(filepath.c_str(), 0) == -1) {
        cout << "Webhead.u not found — cutscenes fix skipped." << endl;
        return false;
    }

    fstream file(filepath, ios::in | ios::out | ios::binary);
    if (!file.is_open()) {
        cout << "Cannot open Webhead.u" << endl;
        return false;
    }

    const char pattern[12] = { '\x4F','\x48','\x1B','\x24','\x00','\x00','\x70','\x42','\x00','\x47','\x01','\x00' };
    const char mask[12] = { 'x','x','x','x','?','?','?','?','x','x','x','x' };

    file.seekg(0, ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, ios::beg);

    vector<char> buffer(static_cast<size_t>(fileSize));
    file.read(buffer.data(), fileSize);

    std::streampos offset = std::streampos(-1);
    for (size_t i = 0; i <= static_cast<size_t>(fileSize) - 12; ++i) {
        bool match = true;
        for (int j = 0; j < 12; ++j) {
            if (mask[j] == 'x' && buffer[i + j] != pattern[j]) { match = false; break; }
        }
        if (match) {
            for (int j = 0; j < 12; ++j) {
                if (mask[j] == '?') { offset = static_cast<std::streampos>(i + j); break; }
            }
            break;
        }
    }

    if (offset == std::streampos(-1)) {
        cout << "Cutscene FOV pattern not found!" << endl;
        file.close();
        return false;
    }

    double aspect = static_cast<double>(width) / height;
    double default_aspect = 4.0 / 3.0;
    double default_fov = 60.0;
    double rad = default_fov * 3.14159265358979323846 / 180.0 / 2.0;
    double tan_fov = tan(rad);
    double new_rad = 2.0 * atan(tan_fov * (aspect / default_aspect));
    float new_fov = static_cast<float>(new_rad * 180.0 / 3.14159265358979323846);

    file.seekp(offset);
    file.write(reinterpret_cast<const char*>(&new_fov), sizeof(new_fov));
    file.close();

    cout << "Cutscenes FOV patched: " << fixed << setprecision(2) << new_fov << "°" << endl;
    return true;
}

// === MAIN ===
int main() {
    SetConsoleCP(65001);
    SetConsoleOutputCP(CP_UTF8);

    if (!IsRunAsAdmin()) {
        cout << "ERROR: Admin rights required!" << endl;
        RunAsAdmin();
        return 0;
    }

    cout << "================================================" << endl;
    cout << "Spider-Man 2 (2005) Launcher" << endl;
    cout << "MR.CODERMAN 2025" << endl;
    cout << "================================================" << endl << endl;

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wstring path = exePath;
    path = path.substr(0, path.find_last_of(L"\\"));

    wstring system_dir = path + L"\\system";
    wstring def_ini = system_dir + L"\\Default.ini";
    wstring web_ini = system_dir + L"\\Webhead.ini";
    wstring defuser_ini = system_dir + L"\\DefUser.ini";
    wstring user_ini = system_dir + L"\\User.ini";
    wstring exe = system_dir + L"\\Webhead.exe";

    OSVERSIONINFO osvi = { sizeof(osvi) };
    GetVersionEx(&osvi);
    if (osvi.dwMajorVersion >= 6) SetXPCompatibilityForGame(exe);

    if (_waccess(def_ini.c_str(), 0) == -1 || _waccess(exe.c_str(), 0) == -1) {
        cout << "[ERROR] Required files missing!" << endl;
        system("pause");
        return 1;
    }

    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    double aspect = static_cast<double>(width) / height;

    cout << "Resolution: " << width << " x " << height << endl;
    cout << "Aspect: " << fixed << setprecision(2) << aspect << ":1" << endl << endl;

    // Разрешение
    update_ini_resolution(def_ini, width, height);
    if (_waccess(web_ini.c_str(), 0) == 0) update_ini_resolution(web_ini, width, height);

    // FOV — ТОЛЬКО ЗАМЕНА!
    double fov = get_hor_plus_fov(aspect);
    update_user_ini_fov(defuser_ini, fov);
    update_user_ini_fov(user_ini, fov);

    // Кат-сцены
    patch_webhead_u(system_dir, width, height);

    cout << endl << "Launching..." << endl;
    ShellExecuteW(NULL, L"open", exe.c_str(), NULL, system_dir.c_str(), SW_SHOWNORMAL);

    cout << endl << "Done!" << endl;
    return 0;
}