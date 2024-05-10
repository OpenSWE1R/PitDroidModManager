#include "memoryReadWrite.h"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <memory>
#include <vector>
#include <bitset>
#include <windows.h>

#include "config.h"
#include "types.h"

std::vector<ChangeItem> g_changes;
std::bitset<SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN> g_collisionsMask;
const char* g_modName = nullptr; // used for logging by writeBytesChecked
int g_modHadCollision = false; // set if transaction failed, to rollback

HINSTANCE g_hInstance;
HINSTANCE g_hPrevInstance;
PSTR g_pCmdLine;
int g_nCmdShow;

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
        printf("%s Writting...\n <<<<\n", g_modName);
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

    for (std::size_t i = reinterpret_cast<uintptr_t>(WINMAIN_ADDR); i < reinterpret_cast<uintptr_t>(WINMAIN_ADDR) + sizeof(init_hook_code); i++)
    {
        g_collisionsMask.set(i);
    }

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

extern "C"
{
    __declspec(dllexport) bool WriteBytesChecked(unsigned char* at, unsigned char* code, size_t nbBytes)
    {
        for (std::size_t i = reinterpret_cast<uintptr_t>(at); i < reinterpret_cast<uintptr_t>(at) + nbBytes; i++)
        {
            if (g_collisionsMask.test(i))
            {
                printf("WARNING: Detected Write Collision at 0x%08X! Mod %s cannot write at %p!\n", i, g_modName, at);

                g_modHadCollision = true;
                return false;
            }
        }

        auto oldBytes = std::make_unique<unsigned char[]>(nbBytes);

        WriteBytes(at, code, oldBytes.get(), nbBytes);
        for (std::size_t i = reinterpret_cast<uintptr_t>(at); i < reinterpret_cast<uintptr_t>(at) + nbBytes; i++)
        {
            g_collisionsMask.set(i);
        }

        g_changes.emplace_back(ChangeItem{
            g_modName,
            at,
            nbBytes,
            std::move(oldBytes),
        });

        return true;
    }
}

void UndoAllWritesForMod(const char* modName)
{
    DWORD old;
    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, PAGE_EXECUTE_READWRITE, &old);

    size_t i = 0;
    while (g_changes.size() < i)
    {
        if (std::strcmp(modName, g_changes[i].modName.c_str()) == 0)
        {
            unsigned char* at = g_changes[i].position;
            unsigned char* oldBytes = g_changes[i].oldBytes.get();
            size_t nbBytes = g_changes[i].nbBytes;

            WriteBytes(at, oldBytes, nullptr, nbBytes);

            for (std::size_t j = reinterpret_cast<uintptr_t>(at); j < reinterpret_cast<uintptr_t>(at) + nbBytes; j++)
            {
                g_collisionsMask.reset(j);
            }

            g_changes.erase(g_changes.begin() + i);
        }
        else
        {
            i += 1;
        }
    }

    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, old, nullptr);
}

void UndoAllWrites()
{
    DWORD old;
    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, PAGE_EXECUTE_READWRITE, &old);

    while (0 < g_changes.size())
    {
        size_t last = g_changes.size() - 1;
        unsigned char* at = g_changes[last].position;
        unsigned char* oldBytes = g_changes[last].oldBytes.get();
        size_t nbBytes = g_changes[last].nbBytes;

        WriteBytes(at, oldBytes, nullptr, nbBytes);

        for (std::size_t j = reinterpret_cast<uintptr_t>(at); j < reinterpret_cast<uintptr_t>(at) + nbBytes; j++)
        {
            g_collisionsMask.reset(j);
        }

        g_changes.erase(g_changes.end() - 1);
    }

    VirtualProtect((void*)SWR_SECTION_TEXT_BEGIN, SWR_SECTION_RSRC_BEGIN - SWR_SECTION_TEXT_BEGIN, old, nullptr);
}

typedef void (*modInitf)(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow);

/**
 * @return true on error
 */
bool runPatching(std::vector<ModItem>& mods_items)
{
    printf("Running Patching...\n");

    // Clear all previous patches in order not to collide with self
    UndoAllWrites();

    int hasError = false;
    for (const ModItem& item : mods_items)
    {
        if (item.activated)
        {
            g_modName = item.filename.c_str();
            g_modHadCollision = false;

            HINSTANCE handle = LoadLibraryA(item.path.string().c_str());
            if (!handle)
            {
                printf("Error: Cannot load %s. Refresh the dll list before patching.\n", g_modName);
                hasError = true;
                continue;
            }

            modInitf modInit = (modInitf)GetProcAddress(handle, "init");
            if (!modInit)
            {
                printf("Error: Cannot find \"init\" function in the mod %s. Make sure the function symbol is properly exported.\n", g_modName);
                // if (g_config.developperMode)
                // {
                LPSTR messageBuffer = nullptr;
                size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                                             GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
                printf("\t%s\n", messageBuffer);
                // }
                hasError = true;
                continue;
            }

            modInit(g_hInstance, g_hPrevInstance, g_pCmdLine, g_nCmdShow);

            // abort transaction for mod that collided
            if (g_modHadCollision)
            {
                UndoAllWritesForMod(g_modName);
                printf("Error: Collision occured when trying to patch \"%s\"\n", item.filename.c_str());
                hasError = true;
                continue;
            }
        }
    }

    if (hasError)
    {
        printf("Patching contains some Errors !\n");
        return true;
    }

    printf("Patching completed successfuly !\n");
    return false;
}
