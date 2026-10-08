import os
import subprocess
import sys

# Ensure w64devkit is first on PATH and MinGW is removed
path_dirs = os.environ.get("PATH", "").split(os.pathsep)
filtered = [p for p in path_dirs if "MinGW" not in p and "mingw" not in p]
os.environ["PATH"] = os.pathsep.join(["C:\\Tools\\w64devkit\\bin"] + filtered)

# Run CMake configure
print("Configuring with CMake...")
res = subprocess.run([
    sys.executable, "-m", "cmake",
    "-S", ".",
    "-B", "build",
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_CXX_COMPILER=C:/Tools/w64devkit/bin/g++.exe",
    "-DCMAKE_C_COMPILER=C:/Tools/w64devkit/bin/gcc.exe",
    "-DCMAKE_MAKE_PROGRAM=C:/Tools/w64devkit/bin/ninja.exe",
], check=False)
if res.returncode != 0:
    sys.exit(res.returncode)

# Run CMake build
print("Building with Ninja...")
res = subprocess.run([
    sys.executable, "-m", "cmake",
    "--build", "build",
], check=False)
sys.exit(res.returncode)
