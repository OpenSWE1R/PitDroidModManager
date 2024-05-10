import sys
import os
import subprocess
import platform
import glob
import concurrent.futures
import argparse
import shutil

class colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

THISDIR = os.getcwd()
THISPYTHON = "python" + sys.version[0:4]

SOURCES_LOADER = [os.path.join(THISDIR, "src", "modloader", "modloader.cpp"), os.path.join(THISDIR, "src", "modloader", "md5.c")]
FLAGS_LOADER = ["-g", "-O0" "-Wall", "-Wextra", "-pedantic"]

SOURCES_IMGUI = [
    "imgui-1.90.5/imgui.cpp",
    "imgui-1.90.5/imgui_demo.cpp",
    "imgui-1.90.5/imgui_draw.cpp",
    "imgui-1.90.5/imgui_tables.cpp",
    "imgui-1.90.5/imgui_widgets.cpp",
    "imgui-1.90.5/imgui_stdlib.cpp",
    "imgui-1.90.5/backends/imgui_impl_sdl2.cpp",
    "imgui-1.90.5/backends/imgui_impl_opengl3.cpp"
]

INCLUDES = [
    "-Isrc/",
    "-ISDL2/include",
    "-Iimgui-1.90.5",
    "-Iimgui-1.90.5",
    "-Iimgui-1.90.5/backends"
]
INCLUDES_LIBS = ["-LSDL2/"]

# -DINCLUDE_DX_HEADERS=1 doesnt include the correct headers for some reasons. Missing -I ?
FLAGS = [
    "-std=c++20",
    "-s",
    "-shared",
    "-Wall",
    "-Wextra",
    "-Wno-unused-parameter",
    "-Wno-unused-variable",
    "-g",
    "-O0"
]
LIBS = [
    "-lgdi32",
    "-lopengl32",
    "-lmingw32",
    "-mwindows",
    "-lSDL2main",
    "-lSDL2",
    "-luser32",
    "-lwinmm",
    "-limm32",
    "-lole32",
    "-loleaut32",
    "-lversion",
    "-luuid",
    "-ladvapi32",
    "-lsetupapi",
    "-lshell32",
    "-ldinput8"
]

IGNORED_SOURCES = [os.path.join(THISDIR, "src", "modloader", "modloader.cpp")]
SOURCES = glob.glob(os.path.join(THISDIR, "src", "**", "*.c"), recursive=True) + glob.glob(os.path.join(THISDIR, "src", "**", "*.cpp"), recursive=True)
for ignored in IGNORED_SOURCES:
    SOURCES.remove(ignored)

SOURCES += SOURCES_IMGUI

def printerr(s):
    print(f"{colors.FAIL}{s}{colors.ENDC}")

def cmdExists(cmd: str):
    if platform.system() == "Windows":
        _, _, status = run(["where", cmd])
    else:
        _, _, status = run(["command", "-v", cmd])
    return status == 0

def run(args: list[str], cwd=THISDIR):
    print(f"{colors.BOLD}{colors.OKCYAN}Running \"{' '.join(args)}\" in {cwd}{colors.ENDC}")
    try:
        if platform.system() == "Windows":
            process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True, cwd=cwd)
        else:
            process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd)

        stdout, stderr = process.communicate()

        stdout = stdout.decode('utf-8')
        stderr = stderr.decode('utf-8')

        return stdout, stderr, process.returncode
    except Exception as e:
        return None, str(e), 1

def runLogged(args, cwd=None):
    out = None
    err = None
    status = 0

    if cwd:
        out, err, status = run(args, cwd)
    else:
        out, err, status = run(args)

    if status != 0:
        printerr(err)
        printerr(f"Returned status code {status}")
    else:
        if (out != ""):
            print(out)

    return status

def compileSource(sourceFile, objdir, flags, force):
    status = 0
    name = os.path.splitext(os.path.basename(sourceFile))[0]
    objpath = os.path.join(objdir, name + ".o")
    if (not force and os.path.isfile(objpath) and os.path.getmtime(sourceFile) < os.path.getmtime(objpath)):
        return status, objpath

    status = runLogged(["g++"] + [ "-c", sourceFile, "-o", objpath] + flags)
    return status, objpath

def compileAllSources(sourceList, objDir, flags):
    if not os.path.isdir(objDir):
        os.mkdir(objDir)

    objList = []
    failed = False

    failed = False

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as executor:
        futures = [executor.submit(compileSource, sourceList[i], objDir, flags, args.force) for i in range(len(sourceList))]
        results = [future.result() for future in concurrent.futures.as_completed(futures)]

    for status, result_objdir in results:
        objList.append(result_objdir)
        if status != 0:
            failed = True
            break

    return failed, objList

def main(args):
    print(args)

    if not cmdExists("g++"):
        print(f"{colors.FAIL}Missing g++ in PATH ! Please install g++ from https://github.com/brechtsanders/winlibs_mingw/releases/tag/13.2.0posix-17.0.6-11.0.1-ucrt-r5{colors.ENDC}")
        sys.exit(1)

    buildDir = os.path.join(THISDIR, "build")
    objDir = os.path.join(THISDIR, "build", "PitDroidModManager_core")
    modsDir = os.path.join(THISDIR, "examples")
    modsOutDir = os.path.join(THISDIR, "build", "mods")
    assetsDir = os.path.join(THISDIR, "assets")

    if not os.path.isdir(buildDir):
        os.mkdir(buildDir)
    if not os.path.isdir(modsOutDir):
        os.mkdir(modsOutDir)

    if args.modloader:
        _, err, status = run(["g++"] + SOURCES_LOADER + ["-Isrc", "-o", os.path.join("build", "PitDroidModManager")] + FLAGS_LOADER)
        if (status != 0):
            printerr(err)

    if args.examples:
        mods_examples = [f.name for f in os.scandir(modsDir) if f.is_dir()]
        for example_mod in mods_examples:
            sources_example = glob.glob(os.path.join(modsDir, example_mod, "**", "*.c"), recursive=True) + glob.glob(os.path.join(modsDir, example_mod, "**", "*.cpp"), recursive=True)
            failed, objs_list = compileAllSources(sources_example, os.path.join(THISDIR, "build", example_mod), FLAGS)
            if (failed == 0):
                runLogged(["g++"] + objs_list + ["-o", os.path.join(modsOutDir, example_mod + ".dll")] + FLAGS)


    failed, objsCore = compileAllSources(SOURCES, objDir, FLAGS + INCLUDES)

    if (failed):
        printerr("Some object file compilation failed. Aborting now")
        sys.exit(1)
    elif (runLogged(["g++"] + objsCore + ["-o", os.path.join(THISDIR, "build", "core.dll")] + FLAGS + LIBS) != 0):
        sys.exit(1)

    for filename in os.listdir(os.path.join(THISDIR, "assets")):
        shutil.copyfile(os.path.join(assetsDir, filename), os.path.join(modsOutDir, filename))

    print(f"{colors.OKGREEN}Compilation successful ! Outputs are in build/{colors.ENDC}")
    return

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='compile.py', description='Compile the core dll')
    parser.add_argument('-B', '--force', action='store_true', default=False, help='Force rebuild every file')
    parser.add_argument('-j', '--jobs', action='store', default=None, help='Number of parallel jobs', type=int)
    parser.add_argument('--modloader', action='store_true', default=False, help='Build the mod loader as well')
    parser.add_argument('--examples', action='store_true', default=False, help='Build the examples mods as well')
    args = parser.parse_args()
    if platform.system() == "Windows" and args.jobs is not None and args.jobs > 61:
        args.jobs = 61
    main(args)
