#include <stdio.h>

#include <windows.h>

/*

g++ dllmain.cpp -o patching_example.dll -std=c++20 -g -Wall -Wformat -s -shared

*/

#ifdef WIN32

#define SWR_SECTION_TEXT_BEGIN (0x00401000)
#define SWR_SECTION_RSRC_BEGIN (0x00ece000)

HMODULE coreHandle;

typedef bool (*WriteBytesChecked_f)(unsigned char* at, unsigned char* code, size_t nbBytes);
WriteBytesChecked_f WriteBytesChecked = nullptr;

#define NOP (0x90)

// void patchAssetBuffer()
// {
//     unsigned char* ASSETBUFFERMALLOCSIZE_ADDR = (unsigned char*)0x00449042;
//     unsigned char* ASSETBUFFERENDOFFSET_ADDR = (unsigned char*)0x0044904d;
//     WriteBytesChecked(ASSETBUFFERMALLOCSIZE_ADDR, (unsigned char*)(&g_config.assetBufferByteSize), 4);
//     WriteBytesChecked(ASSETBUFFERENDOFFSET_ADDR, (unsigned char*)(&g_config.assetBufferByteSize), 4);
// }

// void patchWindowFlag()
// {
//     unsigned char* CHANGEWINDOWFLAG_ADDR = (unsigned char*)0x0049cf7e;
//     if (g_config.changeWindowFlags)
//     {
//         unsigned char pushNewFlags[] = { 0x68, 0x00, 0x00, 0x04, 0x90 }; // PUSH imm32 WS_SIZEBOX | WS_VISIBLE | WS_POPUP
//         WriteBytesChecked(CHANGEWINDOWFLAG_ADDR, pushNewFlags, sizeof(pushNewFlags));
//     }
// }

// void patchFOV()
// {
//     unsigned char* CAMERAFOVCHANGE_ADDR = (unsigned char*)0x004832ee;
//     unsigned char* CAMERAFOVCHANGEF1_ADDR = (unsigned char*)0x0048349e;
//     unsigned char* CAMERAFOVCHANGEF2_ADDR = (unsigned char*)0x004834a7;

//     // make room for FOV change. + 8 bytes, hitting into nops without issues
//     memmove((void*)(CAMERAFOVCHANGE_ADDR + 8), (void*)CAMERAFOVCHANGE_ADDR, 0x1ba);
//     memset((void*)CAMERAFOVCHANGE_ADDR, NOP, 14); // we remove the original instruction as well and put it in our shellcode instead

//     // MOV ECX, config->cameraFOV
//     // MOV [ESI + 0x44], ECX
//     // MOV ECX, 0x3f800000 // Put back the original value for the rest of the function
//     unsigned char cameraFOVPatch[13] = { 0xB9, NOP, NOP, NOP, NOP, 0x89, 0x4E, 0x44, 0xB9, 0x00, 0x00, 0x80, 0x3F };
//     memmove(&(cameraFOVPatch[1]), (void*)(&g_config.cameraFOV), 4);
//     WriteBytesChecked(CAMERAFOVCHANGE_ADDR, cameraFOVPatch, sizeof(cameraFOVPatch));

//     // Need to patch the two rel16 function call. These two writes both do -8 on the relative call
//     unsigned char cameraFOVF1 = 0x3e;
//     WriteBytesChecked(CAMERAFOVCHANGEF1_ADDR, &cameraFOVF1, 1);
//     unsigned char cameraFOVF2 = 0x65;
//     WriteBytesChecked(CAMERAFOVCHANGEF2_ADDR, &cameraFOVF2, 1);
// }

void patchSkipRaceCutscene()
{
    unsigned char* SKIPRACECUTSCENE_ADDR = (unsigned char*)0x0045753d;
    unsigned char nops[5] = { NOP, NOP, NOP, NOP, NOP };
    WriteBytesChecked(SKIPRACECUTSCENE_ADDR, nops, sizeof(nops));
}

void patchSkipIntroCamera()
{
    unsigned char* SKIPINTROCAMERA_ADDR = (unsigned char*)0x0045e2d5;
    float f = 0.0;
    WriteBytesChecked(SKIPINTROCAMERA_ADDR, (unsigned char*)&f, sizeof(float));
}

void patchUseHighestLOD()
{
    unsigned char* USEHIGHESTLOD_ADDR = (unsigned char*)0x00431748;
    unsigned char nops[3] = { NOP, NOP, NOP };
    WriteBytesChecked(USEHIGHESTLOD_ADDR, nops, sizeof(nops));
}

void patchTrimCountdown()
{
    unsigned char* TRIMCOUNTDOWN_ADDR = (unsigned char*)0x0045e065;
    float f = 1.1;
    WriteBytesChecked(TRIMCOUNTDOWN_ADDR, (unsigned char*)(&f), sizeof(float));
}

void patchSkipCantinaScene()
{
    unsigned char* SKIPCANTINASCENE_ADDR = (unsigned char*)0x004352ab;
    unsigned char newScene = 0x9;
    WriteBytesChecked(SKIPCANTINASCENE_ADDR, &newScene, sizeof(unsigned char));
}

void patchFasterLoad()
{
    unsigned char* FASTERLOAD_ADDR1 = (unsigned char*)0x0045d0db;
    unsigned char* FASTERLOAD_ADDR2 = (unsigned char*)0x00463b87;
    float f = 0.0;
    WriteBytesChecked(FASTERLOAD_ADDR1, (unsigned char*)&f, sizeof(float));
    WriteBytesChecked(FASTERLOAD_ADDR2, (unsigned char*)&f, sizeof(float));
}

int applyPatches()
{
    DWORD old;
    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, PAGE_EXECUTE_READWRITE, &old);

    // patchAssetBuffer();
    // patchWindowFlag(); // TODO: investigate
    // patchFOV();
    patchSkipRaceCutscene();
    patchSkipIntroCamera();
    patchUseHighestLOD();
    patchTrimCountdown();
    patchSkipCantinaScene();
    patchFasterLoad();

    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, old, nullptr);

    return 0;
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

        applyPatches();
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
