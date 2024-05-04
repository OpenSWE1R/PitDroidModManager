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

#include "imgui.h"
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

// #undef DrawText

int runGui()
{
    // Setup SDL
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

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with graphics context
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

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select
    // them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or
    // display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling
    // ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for
    // details.
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
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

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear
        // ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window); // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window); // Pass a pointer to our bool variable (the window will have a closing button that
                                                                  // will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
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

    // Cleanup
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
        int i = 0;
        Sleep(2000);
        printf("Waiting in init %d\n", i);

        // TODO: Real hooks here
        // applyPatches();
        runGui();

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
