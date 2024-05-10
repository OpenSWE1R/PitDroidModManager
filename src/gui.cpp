#include "gui.h"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>
#include <SDL_opengl.h>

#include <vector>
#include <cstring>
#include <filesystem>
namespace fs = std::filesystem;

#include "types.h"
#include "config.h"
#include "memoryReadWrite.h"

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

void getMods(std::vector<ModItem>& outmods)
{
    printf("Getting all mods in the mod folder...\n");
    std::string mod_path{ "./mods" };
    for (const auto& entry : fs::directory_iterator(mod_path))
    {
        std::filesystem::path fullnamePath = entry.path();
        std::string filename = fullnamePath.filename().string();
        const char* extension = fullnamePath.extension().string().c_str();
        if (std::strcmp(extension, ".dll") == 0)
        {
            outmods.push_back(ModItem{ fullnamePath, filename, true });
        }
        else
        {
            printf("\tIgnoring %s when searching for mods\n", filename.c_str());
        }
    }
}

/**
 * @return 1 if want to run game, 0 if not, -1 on error
 */
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
    SDL_Window* window = SDL_CreateWindow("PitDroid Mod Manager Gui", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
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
    std::vector<ModItem> mods_items{};
    getMods(mods_items);
    bool patchError = false;
    bool runGame = false;

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
            {
                runGame = false;
                done = true;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
            {
                runGame = false;
                done = true;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (g_config.developperMode && show_demo_window)
        {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Mod Selection");

            if (g_config.developperMode && show_demo_window)
            {
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
            }

            ImGui::Text("Use your mouse to drag and drop to re-order mods.");

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
                    ImGui::Checkbox("Activated", &mods_items[n].activated);
                    ImGui::SameLine();
                    ImGui::Button(mods_items[n].filename.c_str());

                    // Our buttons are both drag sources and drag targets here!
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                    {
                        ImGui::SetDragDropPayload("MOD_ITEM_CELL", &n, sizeof(int));
                        ImGui::Text("Swap %s with another mod", mods_items[n].filename.c_str());
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

            if (ImGui::Button("Patch and Run game"))
            {
                runGame = true;

                patchError = runPatching(mods_items);
                if (!patchError)
                {
                    done = true;
                }
            }

            if (patchError)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 0.0, 0.0, 1.0));
                ImGui::Text("Some patch error occured. Check the debug console for more informations.");
                if (ImGui::Button("Run game anyway"))
                {
                    runGame = true;
                }
                ImGui::PopStyleColor();
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

    if (runGame)
    {
        return 1;
    }

    return 0;
}
