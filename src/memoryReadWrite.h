#pragma once

#include "types.h"
#include "windows.h"
#include "vector"
#include "cstdint"

extern const char* g_modName;

extern HINSTANCE g_hInstance;
extern HINSTANCE g_hPrevInstance;
extern PSTR g_pCmdLine;
extern int g_nCmdShow;

void PrintMemory(unsigned char* at, size_t nbBytes);
void WriteBytes(unsigned char* at, unsigned char* code, unsigned char* oldBytes, size_t nbBytes);
void hookInit(void);

void UndoAllWritesForMod(const char* modName);
void UndoAllWrites();

bool runPatching(std::vector<ModItem>& mods_items);
