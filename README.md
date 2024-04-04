# PitDroidModManager
Official Mod Loader for Star Wars Episode 1 Racer

# Developpement
Compile with `python compile.py`. The loader and the core mod loading DLL should be in `build/`

# Example Usage
// TODO
## Here is what a user / mod dev usage would look like ideally:

Compile the mod loader and the core dll using `python compile.py --modloader`
Move the `build/PitDroidModManager.exe`, `build/PitDroidModManager_coreConfig.txt` and the `build/PitDroidModManager_core.dll` into the game folder where `SWEP1RCR.EXE` is located
Add mods into the `mods/` folder in the same directory
Run `PitDroidModManager.exe`
Toggle and sort mods you want to run using the GUI. Click on `Patch`. Click on `Run`

# TODO
/!\ Merge All modloaders into a single one (to rule them all)
- Anti-collision wrapper on write of the original ROM
- Hooking utils
- Hot reloading in dev mode
- Any mod to be sortable and toggle-able
- Mod API for mod devs and users
- Speedrunning API
- GUI for mod devs and users

# Internal Workings Overview
For the ideal usage to work here is what is needed:
- GUI from modloader
    - auto-detect mods, sort them and run them sequentially + detect potential collisions
    - run the patched game
    - 'smart' remembering of user options (sorting and toggle state)

# Open questions
CMake ?
    - Pros: Include libraries easier
    - Cons: +1 Dependency
Do we need Detour and additional libraries for the mod loader and the core dll ?

# Technical Questions
Is DLL hot-reloading possible from the loader instead of the core dll ?
How to expose core dll functions as an API in the loader ?
- Loader and core dll do not live in same address space. Dll is required to be in original game address space with injection in order for hooking to work. How to do it then ?
