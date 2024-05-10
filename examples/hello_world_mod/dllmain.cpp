#include <stdio.h>

#include <windows.h>

/*

g++ dllmain.cpp -o hello_world.dll -std=c++20 -g -Wall -Wformat -s -shared

*/

#ifdef WIN32

extern "C"
{
    __declspec(dllexport) void init(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
    {
        printf("Hello World mod !\n");
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
