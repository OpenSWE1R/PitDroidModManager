#include <stdio.h>
#include <windows.h>
#include <winuser.h>
#include <process.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include <string>
#include <cstring>
#include <memory>
#include <bitset>
#include <filesystem>
namespace fs = std::filesystem;

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>
#include <SDL_opengl.h>

#include "config.h"

// clang-format off
/*

g++ src/dllmain.cpp src/config.c imgui-1.90.5/imgui.cpp imgui-1.90.5/imgui_demo.cpp imgui-1.90.5/imgui_draw.cpp imgui-1.90.5/imgui_tables.cpp imgui-1.90.5/imgui_widgets.cpp imgui-1.90.5/backends/imgui_impl_sdl2.cpp imgui-1.90.5/backends/imgui_impl_opengl3.cpp -o core.dll -std=c++20 -g -Wall -Wformat -s -shared -Isrc/ -ISDL2/include/ -Iimgui-1.90.5 -Iimgui-1.90.5/backends -lgdi32 -lopengl32 -lmingw32 -mwindows -LSDL2/ -lSDL2main -lSDL2 -luser32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldinput8 -Dmain=SDL_main

*/
// clang-format on

#ifdef WIN32

#define SWR_SECTION_TEXT_BEGIN (0x00401000)
#define SWR_SECTION_RSRC_BEGIN (0x00ece000)

typedef struct ChangeItem
{
    std::string modName;
    unsigned char* position;
    std::unique_ptr<unsigned char[]> oldBytes;
} ChangeItem;

std::vector<ChangeItem> g_changes;
std::bitset<SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN> collisionsMask;
const std::string modName("core");

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

void PrintMemory(unsigned char* at, size_t nbBytes)
{
    for (size_t i = 0; i < nbBytes; i++)
    {
        printf("%02x ", *at);
        at += 1;
    }
    printf("\n");
}

void WriteBytes(unsigned char* at, unsigned char* code, unsigned char* oldBytes, size_t nbBytes)
{
    if (g_config.developperMode)
    {
        printf("Writting...\n <<<<\n");
        PrintMemory(at, nbBytes);
    }
    for (size_t i = 0; i < nbBytes; i++)
    {
        if (oldBytes != nullptr)
        {
            oldBytes[i] = *at;
        }

        at[i] = code[i];
    }

    if (g_config.developperMode)
    {
        printf(">>>>\n");
        PrintMemory(at - nbBytes, nbBytes);
        printf("Written %u code bytes from %p to %p\n", nbBytes, at - nbBytes, at);
    }
}

bool WriteBytesChecked(std::string modName, unsigned char* at, unsigned char* code, size_t nbBytes)
{
    for (std::size_t i = reinterpret_cast<uintptr_t>(at); i < reinterpret_cast<uintptr_t>(at) + nbBytes; i++)
    {
        if (collisionsMask.test(i))
        {
            printf("WARNING: Detected Write Collision at 0x%08X! Mod %s cannot write at %p!\n", i, modName.c_str(), at);
            return false;
        }
    }

    auto oldBytes = std::make_unique<unsigned char[]>(nbBytes);

    WriteBytes(at, code, oldBytes.get(), nbBytes);
    for (std::size_t i = reinterpret_cast<uintptr_t>(at); i < reinterpret_cast<uintptr_t>(at) + nbBytes; i++)
    {
        collisionsMask.set(i);
    }

    g_changes.emplace_back(ChangeItem{
        modName,
        at,
        std::move(oldBytes),
    });

    return true;
}

void UndoAllWritesForMod(std::string modName)
{
    // TODO
}

#define NOP (0x90)

void patchAssetBuffer()
{
    unsigned char* ASSETBUFFERMALLOCSIZE_ADDR = (unsigned char*)0x00449042;
    unsigned char* ASSETBUFFERENDOFFSET_ADDR = (unsigned char*)0x0044904d;
    WriteBytesChecked(modName, ASSETBUFFERMALLOCSIZE_ADDR, (unsigned char*)(&g_config.assetBufferByteSize), 4);
    WriteBytesChecked(modName, ASSETBUFFERENDOFFSET_ADDR, (unsigned char*)(&g_config.assetBufferByteSize), 4);
}

void patchWindowFlag()
{
    unsigned char* CHANGEWINDOWFLAG_ADDR = (unsigned char*)0x0049cf7e;
    if (g_config.changeWindowFlags)
    {
        unsigned char pushNewFlags[] = { 0x68, 0x00, 0x00, 0x04, 0x90 }; // PUSH imm32 WS_SIZEBOX | WS_VISIBLE | WS_POPUP
        WriteBytesChecked(modName, CHANGEWINDOWFLAG_ADDR, pushNewFlags, sizeof(pushNewFlags));
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
    WriteBytesChecked(modName, CAMERAFOVCHANGE_ADDR, cameraFOVPatch, sizeof(cameraFOVPatch));

    // Need to patch the two rel16 function call. These two writes both do -8 on the relative call
    unsigned char cameraFOVF1 = 0x3e;
    WriteBytesChecked(modName, CAMERAFOVCHANGEF1_ADDR, &cameraFOVF1, 1);
    unsigned char cameraFOVF2 = 0x65;
    WriteBytesChecked(modName, CAMERAFOVCHANGEF2_ADDR, &cameraFOVF2, 1);
}

void patchSkipRaceCutscene()
{
    unsigned char* SKIPRACECUTSCENE_ADDR = (unsigned char*)0x0045753d;
    if (g_config.skipRaceCutscene == false)
        return;
    unsigned char nops[5] = { NOP, NOP, NOP, NOP, NOP };
    WriteBytesChecked(modName, SKIPRACECUTSCENE_ADDR, nops, sizeof(nops));
}

void patchSkipIntroCamera()
{
    if (g_config.skipIntroCamera == false)
        return;
    unsigned char* SKIPINTROCAMERA_ADDR = (unsigned char*)0x0045e2d5;
    float f = 0.0;
    WriteBytesChecked(modName, SKIPINTROCAMERA_ADDR, (unsigned char*)&f, sizeof(float));
}

void patchUseHighestLOD()
{
    if (g_config.useHighestLOD == false)
        return;
    unsigned char* USEHIGHESTLOD_ADDR = (unsigned char*)0x00431748;
    unsigned char nops[3] = { NOP, NOP, NOP };
    WriteBytesChecked(modName, USEHIGHESTLOD_ADDR, nops, sizeof(nops));
}

void patchTrimCountdown()
{
    if (g_config.trimCountdown == false)
        return;

    unsigned char* TRIMCOUNTDOWN_ADDR = (unsigned char*)0x0045e065;
    float f = 1.1;
    WriteBytesChecked(modName, TRIMCOUNTDOWN_ADDR, (unsigned char*)(&f), sizeof(float));
}

void patchSkipCantinaScene()
{
    if (g_config.skipCantinaScene == false)
        return;
    unsigned char* SKIPCANTINASCENE_ADDR = (unsigned char*)0x004352ab;
    unsigned char newScene = 0x9;
    WriteBytesChecked(modName, SKIPCANTINASCENE_ADDR, &newScene, sizeof(unsigned char));
}

void patchFasterLoad()
{
    if (g_config.fasterLoad == false)
        return;
    unsigned char* FASTERLOAD_ADDR1 = (unsigned char*)0x0045d0db;
    unsigned char* FASTERLOAD_ADDR2 = (unsigned char*)0x00463b87;
    float f = 0.0;
    WriteBytesChecked(modName, FASTERLOAD_ADDR1, (unsigned char*)&f, sizeof(float));
    WriteBytesChecked(modName, FASTERLOAD_ADDR2, (unsigned char*)&f, sizeof(float));
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

    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, old, nullptr);

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

    WriteBytes(CORECSTR_ADDR, core, NULL, 5);
    WriteBytes(INITCSTR_ADDR, init, NULL, 5);

    // Improved for init(HINSTANCE hInstance, PSTR pCmdLine, int nCmdShow);
    unsigned char init_hook_code[] = {
        0x68, 0x44, 0x7e, 0x4b, 0x00, // push CORECSTR_ADDR
        0xff, 0x15, 0x8c, 0xc0, 0x4a, 0x00, // call NEAR absolute GetModuleHandleA(CORECTSTR_ADDR);
        0x68, 0xcf, 0x7e, 0x4b, 0x00, // push INITCSTR_ADDR
        0x50, // push eax
        0xff, 0x15, 0x78, 0xc1, 0x4a, 0x00, // call GetProcAddress(Handle, INITCSTR_ADDR);
        0xff, 0xe0, // jmp eax init(hInstance, hPrevInstance, pCmdLine, nCmdShow);
        0x83, 0xc4, 0x10, // add ESP, 0x10 // is this cleanup necessary ? OpenJKDF2 doesn't do it. Is it 0x10 and not 0xC ?
        0x33, 0xc0, // xor eax, eax
        0xc2, 0x10, 0x00 // ret 0x10
    };
    WriteBytes(WINMAIN_ADDR, init_hook_code, NULL, sizeof(init_hook_code));

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
    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, old, nullptr);
}

inline void SetupImGuiStyle(bool bStyleDark_, float alpha_)
{
    ImGuiStyle& style = ImGui::GetStyle();

    /*
     Text color rgb(183, 245, 255)
     Selected Text color rgb(255, 255, 255)
     yellow text: #FBDD01 rgb(251, 221, 1)
    yellow ui outline #EEFF00 rgb(238, 255, 0) + blue ui inline #00DDFF rgb(0, 221, 255)
    yellow hover #616800FF
    yellow active #A2AD00FF
    selected item outline #999896 rgb(153, 152, 150)

    orange text #FF7D00 rgb(255, 125, 0)
    red text #FF0000 rgb(255, 0, 0)
    yellow "disabled" / background #757D00 rgb(117, 125, 0)

    */
    style.Alpha = 1.0f;
    style.WindowRounding = 12.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 12.0f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 12.0f;
    style.TabRounding = 12.0f;
    style.Colors[ImGuiCol_Text] = ImVec4(0.73f, 0.96f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.93f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.93f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.38f, 0.41f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.63f, 0.68f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.93f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.93f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.93f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.93f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    if (bStyleDark_)
    {
        for (int i = 0; i <= ImGuiCol_COUNT; i++)
        {
            ImVec4& col = style.Colors[i];
            float H, S, V;
            ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);

            if (S < 0.1f)
            {
                V = 1.0f - V;
            }
            ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);
            if (col.w < 1.00f)
            {
                col.w *= alpha_;
            }
        }
    }
    else
    {
        for (int i = 0; i <= ImGuiCol_COUNT; i++)
        {
            ImVec4& col = style.Colors[i];
            if (col.w < 1.00f)
            {
                col.x *= alpha_;
                col.y *= alpha_;
                col.z *= alpha_;
                col.w *= alpha_;
            }
        }
    }
}

void getMods(std::vector<std::string>& outmods)
{
    std::string mod_path{ "./mods" };
    for (const auto& entry : fs::directory_iterator(mod_path))
    {
        std::filesystem::path fullnamePath = entry.path().filename();
        std::string fullname = fullnamePath.string();
        const char* extension = fullnamePath.extension().string().c_str();
        if (std::strcmp(extension, ".dll") == 0)
        {
            outmods.push_back(fullname);
        }
        else
        {
            printf("Ignoring %s when searching for mods\n", fullname.c_str());
        }
    }
}

int runGui()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();
    SetupImGuiStyle(false, 1.0);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImFont* default_font = io.Fonts->AddFontFromFileTTF("./mods/DroidSans.ttf", 18.0f);
    IM_ASSERT(default_font != nullptr);
    ImFont* swe1r_font = io.Fonts->AddFontFromFileTTF("./mods/swe1r-font.ttf", 36.0f, nullptr, io.Fonts->GetGlyphRangesDefault());
    IM_ASSERT(swe1r_font != nullptr);

    // Our state
    bool show_demo_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    std::vector<std::string> mods_items{};
    getMods(mods_items);

    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse
        // data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the
        // keyboard data. Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("HELLO, WORLD!");

            float old_scale = swe1r_font->Scale;
            swe1r_font->Scale *= 2;
            ImGui::PushFont(swe1r_font);

            ImGui::Text("THIS IS SOME USEFUL TEXT.");

            swe1r_font->Scale = old_scale;
            ImGui::PopFont();

            ImGui::PushFont(swe1r_font);

            ImGui::Text("now this is podracing.");
            ImGui::PopFont();

            ImGui::Checkbox("DEMO WINDOW", &show_demo_window);

            ImGui::Separator();
            if (mods_items.size() == 0)
            {
                ImGui::Text("No mods detected in the ./mods folder. Check that the mods are dll files");
            }
            else
            {
                for (size_t n = 0; n < mods_items.size(); n++)
                {
                    ImGui::PushID(n);
                    ImGui::Button(mods_items[n].c_str());

                    // Our buttons are both drag sources and drag targets here!
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                    {
                        ImGui::SetDragDropPayload("MOD_ITEM_CELL", &n, sizeof(int));
                        ImGui::Text("Swap %s with another mod", mods_items[n].c_str());
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOD_ITEM_CELL"))
                        {
                            IM_ASSERT(payload->DataSize == sizeof(int));
                            int payload_n = *(const int*)payload->Data;

                            std::swap(mods_items[n], mods_items[payload_n]);
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::PopID();
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Refresh folder"))
            {
                mods_items.clear();
                getMods(mods_items);
            }

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

extern "C"
{
    __declspec(dllexport) void init(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
    {
        printf("Init called inside core dll ! Hooked successfuly.\n");
        // int i = 0;
        // Sleep(2000);
        // printf("Waiting in init %d\n", i);

        // TODO: Real hooks here
        // applyPatches();
        runGui();
        // TODO
        std::exit(1);

        // Call original main
        int (*Window_Main)(HINSTANCE, HINSTANCE, PSTR, int, const char*) = (int (*)(HINSTANCE, HINSTANCE, PSTR, int, const char*))0x0049cd40;
        Window_Main(hInstance, nullptr, pCmdLine, nCmdShow, "Modded game window");
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
