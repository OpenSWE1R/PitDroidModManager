#pragma once

#define SWR_SECTION_TEXT_BEGIN (0x00401000)
#define SWR_SECTION_RSRC_BEGIN (0x00ece000)

#include <cstdint>
#include <string>
#include <memory>
#include <filesystem>

typedef struct ChangeItem
{
    std::string modName;
    unsigned char* position;
    std::size_t nbBytes;
    std::unique_ptr<unsigned char[]> oldBytes;
} ChangeItem;

typedef struct ModItem
{
    std::filesystem::path path;
    std::string filename;
    bool activated;
} ModItem;
