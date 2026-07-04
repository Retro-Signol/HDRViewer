#pragma once
#include <windows.h>

class RegistrySetup {
public:
    static void Ensure();

private:
    static void SetDWORD(HKEY root, const wchar_t* subkey,
                         const wchar_t* name, DWORD value);
};
