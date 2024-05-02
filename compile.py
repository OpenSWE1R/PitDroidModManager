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
FLAGS_LOADER = ["-g", "-Wall", "-Wextra", "-pedantic"]

INCLUDES = ["-I.", "-Isrc", "-Iraylib\include"]
# -DINCLUDE_DX_HEADERS=1 doesnt include the correct headers for some reasons. Missing -I ?
FLAGS = ["-std=c++20", "-s", "-shared", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-unused-variable", "-g"]
LIBS = ["-Lraylib\lib", "-lraylib", "-lgdi32", "-lcomctl32", "-lole32", "-lwinmm"]

IGNORED_SOURCES = [os.path.join(THISDIR, "src", "modloader", "modloader.cpp")]
SOURCES = glob.glob(os.path.join(THISDIR, "src", "**", "*.c"), recursive=True) + glob.glob(os.path.join(THISDIR, "src", "**", "*.cpp"), recursive=True)
for ignored in IGNORED_SOURCES:
    SOURCES.remove(ignored)

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
        print(out)

    return status

def compileSource(sourceFile, objdir, force):
    status = 0
    name = os.path.splitext(os.path.basename(sourceFile))[0]
    objpath = os.path.join(objdir, name + ".o")
    if (not force and os.path.isfile(objpath) and os.path.getmtime(sourceFile) < os.path.getmtime(objpath)):
        return status, objpath

    status = runLogged(["g++"] + [ "-c", sourceFile, "-o", objpath] + FLAGS + INCLUDES + LIBS)
    return status, objpath

def main(args):
    print(args)

    if not cmdExists("g++"):
        print(f"{colors.FAIL}Missing g++ in PATH ! Please install g++ from https://github.com/brechtsanders/winlibs_mingw/releases/tag/13.2.0posix-17.0.6-11.0.1-ucrt-r5{colors.ENDC}")
        sys.exit(1)

    if args.modloader:
        _, err, status = run(["g++"] + SOURCES_LOADER + ["-Isrc", "-o", os.path.join("build", "PitDroidModManager")] + FLAGS_LOADER)
        if (status != 0):
            printerr(err)

    # DLL compilation
    OBJS = []
    failed = False

    objdir = os.path.join(THISDIR, "build", "PitDroidModManager_core")

    if not os.path.isdir(objdir):
        os.mkdir(objdir)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as executor:
        futures = [executor.submit(compileSource, SOURCES[i], objdir, args.force) for i in range(len(SOURCES))]
        results = [future.result() for future in concurrent.futures.as_completed(futures)]

    for status, result_objdir in results:
        OBJS.append(result_objdir)
        if status != 0:
            failed = True
            break

    if (failed):
        printerr("Some object file compilation failed. Aborting now")
        sys.exit(1)
    elif (runLogged(["g++"] + OBJS + ["-o", os.path.join(THISDIR, "build", "core.dll")] + FLAGS + INCLUDES + LIBS) != 0):
        sys.exit(1)

    shutil.copyfile(os.path.join(THISDIR, "config", "coreConfig.txt"), os.path.join(THISDIR, "build", "coreConfig.txt"))

    print(f"{colors.OKGREEN}Compilation successful ! Outputs are in build/{colors.ENDC}")
    return

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='compile.py', description='Compile the core dll')
    parser.add_argument('-B', '--force', action='store_true', default=False, help='Force rebuild every file')
    parser.add_argument('-j', '--jobs', action='store', default=None, help='Number of parallel jobs', type=int)
    parser.add_argument('--modloader', action='store_true', default=False, help='Build the mod loader as well')
    args = parser.parse_args()
    if platform.system() == "Windows" and args.jobs is not None and args.jobs > 61:
        args.jobs = 61
    main(args)
