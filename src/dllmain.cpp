#include <stdio.h>

#include <windows.h>
#include <process.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"

#ifdef WIN32

HWND g_ConsoleWindow = NULL;

bool CreateConsoleWindow()
{
    if (!AllocConsole())
        return false;

    g_ConsoleWindow = GetConsoleWindow();
    if (g_ConsoleWindow == NULL)
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

void PrintMemory(unsigned char* at, size_t nbBytes)
{
    for (size_t i = 0; i < nbBytes; i++)
    {
        printf("%02x ", *at);
        at += 1;
    }
    printf("\n");
}

void WriteBytes(unsigned char* at, unsigned char* code, size_t nbBytes)
{
    if (g_config.developperMode)
    {
        printf("Writting...\n <<<<\n");
        PrintMemory(at, nbBytes);
    }
    for (size_t i = 0; i < nbBytes; i++)
    {
        *at = *code;
        at += 1;
        code += 1;
    }
    if (g_config.developperMode)
    {
        printf(">>>>\n");
        PrintMemory(at - nbBytes, nbBytes);
        printf("Written %u code bytes from %p to %p\n", nbBytes, at - nbBytes, at);
    }
}

#define SWR_SECTION_TEXT_BEGIN (0x00401000)
#define SWR_SECTION_RSRC_BEGIN (0x00ece000)

#define NOP (0x90)

void patchAssetBuffer()
{
    unsigned char* ASSETBUFFERMALLOCSIZE_ADDR = (unsigned char*)0x00449042;
    unsigned char* ASSETBUFFERENDOFFSET_ADDR = (unsigned char*)0x0044904d;
    WriteBytes(ASSETBUFFERMALLOCSIZE_ADDR, (unsigned char*)(&g_config.assetBufferByteSize), 4);
    WriteBytes(ASSETBUFFERENDOFFSET_ADDR, (unsigned char*)(&g_config.assetBufferByteSize), 4);
}

void patchWindowFlag()
{
    unsigned char* CHANGEWINDOWFLAG_ADDR = (unsigned char*)0x0049cf7e;
    if (g_config.changeWindowFlags)
    {
        unsigned char pushNewFlags[] = { 0x68, 0x00, 0x00, 0x04, 0x90 }; // PUSH imm32 WS_SIZEBOX | WS_VISIBLE | WS_POPUP
        WriteBytes(CHANGEWINDOWFLAG_ADDR, pushNewFlags, sizeof(pushNewFlags));
    }
}

void patchFOV()
{
    unsigned char* CAMERAFOVCHANGE_ADDR = (unsigned char*)0x004832ee;
    unsigned char* CAMERAFOVCHANGEF1_ADDR = (unsigned char*)0x0048349e;
    unsigned char* CAMERAFOVCHANGEF2_ADDR = (unsigned char*)0x004834a7;

    // make room for FOV change. + 8 bytes, hitting into nops without issues
    memmove((void*)(CAMERAFOVCHANGE_ADDR + 8), (void*)CAMERAFOVCHANGE_ADDR, 0x1ba);
    memset((void*)CAMERAFOVCHANGE_ADDR, NOP, 14); // we remove the original instruction as well and put it in our shellcode instead

    // MOV ECX, config->cameraFOV
    // MOV [ESI + 0x44], ECX
    // MOV ECX, 0x3f800000 // Put back the original value for the rest of the function
    unsigned char cameraFOVPatch[13] = { 0xB9, NOP, NOP, NOP, NOP, 0x89, 0x4E, 0x44, 0xB9, 0x00, 0x00, 0x80, 0x3F };
    memmove(&(cameraFOVPatch[1]), (void*)(&g_config.cameraFOV), 4);
    WriteBytes(CAMERAFOVCHANGE_ADDR, cameraFOVPatch, sizeof(cameraFOVPatch));

    // Need to patch the two rel16 function call. These two writes both do -8 on the relative call
    unsigned char cameraFOVF1 = 0x3e;
    WriteBytes(CAMERAFOVCHANGEF1_ADDR, &cameraFOVF1, 1);
    unsigned char cameraFOVF2 = 0x65;
    WriteBytes(CAMERAFOVCHANGEF2_ADDR, &cameraFOVF2, 1);
}

void patchSkipRaceCutscene()
{
    unsigned char* SKIPRACECUTSCENE_ADDR = (unsigned char*)0x0045753d;
    if (g_config.skipRaceCutscene == false)
        return;
    unsigned char nops[5] = { NOP, NOP, NOP, NOP, NOP };
    WriteBytes(SKIPRACECUTSCENE_ADDR, nops, sizeof(nops));
}

void patchSkipIntroCamera()
{
    if (g_config.skipIntroCamera == false)
        return;
    unsigned char* SKIPINTROCAMERA_ADDR = (unsigned char*)0x0045e2d5;
    float f = 0.0;
    WriteBytes(SKIPINTROCAMERA_ADDR, (unsigned char*)&f, sizeof(float));
}

void patchUseHighestLOD()
{
    if (g_config.useHighestLOD == false)
        return;
    unsigned char* USEHIGHESTLOD_ADDR = (unsigned char*)0x00431748;
    unsigned char nops[3] = { NOP, NOP, NOP };
    WriteBytes(USEHIGHESTLOD_ADDR, nops, sizeof(nops));
}

void patchTrimCountdown()
{
    if (g_config.trimCountdown == false)
        return;

    unsigned char* TRIMCOUNTDOWN_ADDR = (unsigned char*)0x0045e065;
    float f = 1.1;
    WriteBytes(TRIMCOUNTDOWN_ADDR, (unsigned char*)(&f), sizeof(float));
}

void patchSkipCantinaScene()
{
    if (g_config.skipCantinaScene == false)
        return;
    unsigned char* SKIPCANTINASCENE_ADDR = (unsigned char*)0x004352ab;
    unsigned char newScene = 0x9;
    WriteBytes(SKIPCANTINASCENE_ADDR, &newScene, sizeof(unsigned char));
}

void patchFasterLoad()
{
    if (g_config.fasterLoad == false)
        return;
    unsigned char* FASTERLOAD_ADDR1 = (unsigned char*)0x0045d0db;
    unsigned char* FASTERLOAD_ADDR2 = (unsigned char*)0x00463b87;
    float f = 0.0;
    WriteBytes(FASTERLOAD_ADDR1, (unsigned char*)&f, sizeof(float));
    WriteBytes(FASTERLOAD_ADDR2, (unsigned char*)&f, sizeof(float));
}

int applyPatches()
{
    if (g_config.developperMode)
        printf("Applying Patches...\n");

    DWORD old;
    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, PAGE_EXECUTE_READWRITE, &old);

    patchAssetBuffer();
    // patchWindowFlag(); // TODO: investigate
    patchFOV();
    patchSkipRaceCutscene();
    patchSkipIntroCamera();
    patchUseHighestLOD();
    patchTrimCountdown();
    patchSkipCantinaScene();
    patchFasterLoad();

    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, old, NULL);

    if (g_config.developperMode)
    {
        printf("Patching done. Press any key to continue to the game\n");
        getchar();
    }
    return 0;
}

void hookInit(void)
{
    DWORD old;
    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, PAGE_EXECUTE_READWRITE, &old);
    // WinMain @ 0x004238d0

    // String locations are in padding unused space in .data
    unsigned char* CORECSTR_ADDR = (unsigned char*)0x004b7e44;
    unsigned char* INITCSTR_ADDR = (unsigned char*)0x004b7ecf;
    unsigned char* WINMAIN_ADDR = (unsigned char*)0x004238d0;

    unsigned char core[5] = { 'c', 'o', 'r', 'e', 0 };
    unsigned char init[5] = { 'i', 'n', 'i', 't', 0 };

    WriteBytes(CORECSTR_ADDR, core, 5);
    WriteBytes(INITCSTR_ADDR, init, 5);

    // Improved for init(HINSTANCE hInstance, PSTR pCmdLine, int nCmdShow);
    unsigned char init_hook_code[] = {
        0x68, 0x44, 0x7e, 0x4b, 0x00, // push CORECSTR_ADDR
        0xff, 0x15, 0x8c, 0xc0, 0x4a, 0x00, // call NEAR absolute GetModuleHandleA(CORECTSTR_ADDR);
        0x68, 0xcf, 0x7e, 0x4b, 0x00, // push INITCSTR_ADDR
        0x50, // push eax
        0xff, 0x15, 0x78, 0xc1, 0x4a, 0x00, // call GetProcAddress(Handle, INITCSTR_ADDR);
        0xff, 0xe0, // jmp eax init(hInstance, hPrevInstance, pCmdLine, nCmdShow);
        0x83, 0xc4, 0x10, // add ESP, 0x10
        0x33, 0xc0, // xor eax, eax
        0xc2, 0x10, 0x00 // ret 0x10
    };
    WriteBytes(WINMAIN_ADDR, init_hook_code, sizeof(init_hook_code));

    // The shell code without the C syntax:
    /*
        68 44 7e 4b 00
        ff 15 8c c0 4a 00
        68 cf 7e 4b 00
        50
        ff 15 78 c1 4a 00
        ff e0
        83 c4 10
        33 c0
        c2 10 00
    */
    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, old, NULL);
}

extern "C"
{
    __declspec(dllexport) void init(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
    {
        printf("Init called inside core dll ! Hooked successfuly.\n");
        int i = 0;
        Sleep(2000);
        printf("Waiting in init %d\n", i);

        // TODO: Real hooks here
        // applyPatches();

        // Call original main
        int (*Window_Main)(HINSTANCE, HINSTANCE, PSTR, int, char*) = (int (*)(HINSTANCE, HINSTANCE, PSTR, int, char*))0x0049cd40;
        Window_Main(hInstance, NULL, pCmdLine, nCmdShow, "Modded game window");
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
