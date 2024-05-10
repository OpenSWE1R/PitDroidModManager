#include <stdio.h>
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#include "types.h"
#include "config.h"
#include "memoryReadWrite.h"
#include "gui.h"

// clang-format off
/*

g++ src/dllmain.cpp src/config.c imgui-1.90.5/imgui.cpp imgui-1.90.5/imgui_demo.cpp imgui-1.90.5/imgui_draw.cpp imgui-1.90.5/imgui_tables.cpp imgui-1.90.5/imgui_widgets.cpp imgui-1.90.5/backends/imgui_impl_sdl2.cpp imgui-1.90.5/backends/imgui_impl_opengl3.cpp -o core.dll -std=c++20 -g -Wall -Wformat -s -shared -Isrc/ -ISDL2/include/ -Iimgui-1.90.5 -Iimgui-1.90.5/backends -lgdi32 -lopengl32 -lmingw32 -mwindows -LSDL2/ -lSDL2main -lSDL2 -luser32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldinput8 -Dmain=SDL_main

*/
// clang-format on

#ifdef WIN32

bool CreateConsoleWindow()
{
    if (!AllocConsole())
        return false;

    HWND ConsoleWindow = GetConsoleWindow();
    if (ConsoleWindow == nullptr)
        return false;

    freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

    SetConsoleTitleA("SWR CE Debug Console");

    char NPath[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, NPath);

    printf("Core DLL Loaded at %s\n", NPath);
    printf("PID is %ld\n", GetCurrentProcessId());
    if (g_config.developperMode)
    {
        printf("Waiting for input...\n");
        getchar();
    }

    return true;
}

extern "C"
{
    __declspec(dllexport) void init(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
    {
        printf("Init called inside core dll ! Hooked successfuly.\n");
        g_hInstance = hInstance;
        g_hPrevInstance = hPrevInstance;
        g_pCmdLine = pCmdLine;
        g_nCmdShow = nCmdShow;

        int result = runGui();
        if (result == 1)
        {
            // Call original main
            int (*Window_Main)(HINSTANCE, HINSTANCE, PSTR, int, const char*) = (int (*)(HINSTANCE, HINSTANCE, PSTR, int, const char*))0x0049cd40;
            Window_Main(hInstance, nullptr, pCmdLine, nCmdShow, "Modded game window");
        }
        else if (result != 0)
        {
            // Error of some kind
        };
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    (void)hModule;
    (void)lpReserved;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        parseConfig();
        if (!CreateConsoleWindow())
        {
            printf("PitDroidModManager dll console exists\n");
        }
        printConfig();

        hookInit();
        break;
    }

    case DLL_PROCESS_DETACH:
        printf("PitDroidModManager unloading\n");
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}

#else // !WIN32
#error "This dll must be compiled for Windows 32bits, x86. The WIN32 environment variable must be set."
#endif // WIN32
