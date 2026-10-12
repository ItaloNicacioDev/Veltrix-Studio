# scripts/README.md — helper scripts documentation

## build-windows.bat
Configures, builds and tests the Veltrix core on Windows with MSVC + Ninja
through CMake presets.

```
scripts\build-windows.bat            (release + tests)
scripts\build-windows.bat debug     (debug + tests)
```

Requirements: Visual Studio 2022 (or Build Tools) with the "Desktop
development with C++" workload, including the "C++ CMake tools for
Windows" component (CMake + Ninja).

The script locates Visual Studio via `vswhere` (standard installer path).
If your VS lives elsewhere without vswhere, set `VELTRIX_VS_PATH` to the
installation directory:

```
set VELTRIX_VS_PATH=E:\VS_TOOLS
scripts\build-windows.bat
```

## build-linux.sh
Same for Linux (GCC >= 11 or Clang >= 14, CMake >= 3.24, Ninja):

```
./scripts/build-linux.sh
./scripts/build-linux.sh debug
```

Neither script hardcodes machine-specific paths; compilers/paths are
discovered or overridden through standard environment variables.
