#include <stdio.h>

#include <windows.h>

/*

g++ dllmain.cpp -o patching_example.dll -std=c++20 -g -Wall -Wformat -s -shared

*/

#ifdef WIN32

HMODULE coreHandle;

typedef bool (*WriteBytesChecked_f)(unsigned char* at, unsigned char* code, size_t nbBytes);
WriteBytesChecked_f WriteBytesChecked = nullptr;

void patchFasterLoad()
{
    unsigned char* FASTERLOAD_ADDR1 = (unsigned char*)0x0045d0db;
    unsigned char* FASTERLOAD_ADDR2 = (unsigned char*)0x00463b87;
    float f = 0.0;
    WriteBytesChecked(FASTERLOAD_ADDR1, (unsigned char*)&f, sizeof(float));
    WriteBytesChecked(FASTERLOAD_ADDR2, (unsigned char*)&f, sizeof(float));
}

extern "C"
{
    __declspec(dllexport) void init(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
    {
        printf("Init QOL mod\n");

        coreHandle = GetModuleHandleA("core");
        if (coreHandle == nullptr)
        {
            printf("Error: Core DLL not loaded. Aborting\n");
            return;
        }
        WriteBytesChecked = (WriteBytesChecked_f)GetProcAddress(coreHandle, "WriteBytesChecked");
        if (WriteBytesChecked == nullptr)
        {
            printf("Error: Core DLL is invalid. Aborting\n");
            return;
        }

        patchFasterLoad();
    }
}

/**
 * @brief Leave this function EMPTY ! Entry point for mods is the 'init' function
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    (void)hModule;
    (void)ul_reason_for_call;
    (void)lpReserved;

    return TRUE;
}

#else // !WIN32
#error "This dll must be compiled for Windows 32bits, x86. The WIN32 environment variable must be set."
#endif // WIN32
