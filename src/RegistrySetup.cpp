#include "RegistrySetup.h"

void RegistrySetup::Ensure() {
    // 检查是否已写入
    HKEY hk;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\HDRViewer",
        0, KEY_READ, &hk) == ERROR_SUCCESS) {
        DWORD applied = 0;
        DWORD size = sizeof(DWORD);
        RegQueryValueExW(hk, L"RegistryApplied", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&applied), &size);
        RegCloseKey(hk);
        if (applied) return;
    }

    // 1. GPU 偏好 — 高性能 GPU
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    SetDWORD(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\DirectX\\UserGpuPreferences",
        exePath, 2);

    // 2. HDR 调度
    SetDWORD(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\DirectX\\GraphicsSettings",
        L"HDREnabledApp", 1);

    // 3. GameDVR — Auto HDR 兼容
    SetDWORD(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR",
        L"AutoHDRAllowed", 1);

    // 4. 标记已写入
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\HDRViewer",
        0, nullptr, 0, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        DWORD one = 1;
        RegSetValueExW(hk, L"RegistryApplied", 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&one), sizeof(DWORD));
        RegCloseKey(hk);
    }
}

void RegistrySetup::SetDWORD(HKEY root, const wchar_t* subkey,
                              const wchar_t* name, DWORD value) {
    HKEY hk;
    if (RegCreateKeyExW(root, subkey, 0, nullptr, 0,
        KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hk, name, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
        RegCloseKey(hk);
    }
}
