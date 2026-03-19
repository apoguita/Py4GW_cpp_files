# Py4GW C++ Source

This directory contains the C++ source code for `Py4GW.dll`, which provides Python bindings for Guild Wars via pybind11.

## Prerequisites

- **Visual Studio 2022** (with C++ desktop development workload)
- **Python 3.13.0 32-bit** (GWCA requires 32-bit)
  - Install from: https://www.python.org/downloads/
  - Use the Windows installer and select "32-bit" version
  - Verify with: `py -3.13-32 --version`
- **CMake** (included with Visual Studio or install separately)

## Build Instructions

### 1. Install D3DX9 Libraries

The project requires legacy DirectX SDK libraries. Download them via NuGet:

```powershell
# Download the NuGet package
Invoke-WebRequest -Uri 'https://www.nuget.org/api/v2/package/Microsoft.DXSDK.D3DX' -OutFile 'd3dx_nuget.zip'

# Extract it
Expand-Archive -Path 'd3dx_nuget.zip' -DestinationPath 'd3dx_nuget'

# Copy the x86 libs to the vendor directory
mkdir -p vendor/gwca/Dependencies/DirectX/Lib/x86
copy d3dx_nuget/build/native/release/lib/x86/*.lib vendor/gwca/Dependencies/DirectX/Lib/x86/

# Copy runtime DLLs to bin directory (needed at runtime)
copy d3dx_nuget/build/native/release/bin/x86/*.dll bin/
```

### 2. Configure CMake

```powershell
# Create build directory
mkdir build
cd build

# Configure for 32-bit build with Python 3.13-32
# Adjust the Python path if your installation differs
cmake -G "Visual Studio 17 2022" -A Win32 `
    -DPython3_ROOT_DIR="C:\Users\<USERNAME>\AppData\Local\Programs\Python\Python313-32" `
    -DPython3_EXECUTABLE="C:\Users\<USERNAME>\AppData\Local\Programs\Python\Python313-32\python.exe" `
    ..
```

### 3. Build

```powershell
# Build in RelWithDebInfo mode
cmake --build . --config RelWithDebInfo

# The DLL will be output to: bin/RelWithDebInfo/Py4GW.dll
```

### 4. Deploy

Copy the built DLL to the Py4GW Python project:

```powershell
copy bin/Release/Py4GW.dll ../Py4GW/
```

## Project Structure

```
Py4GW_cpp_files/
├── src/                    # C++ source files
│   ├── dllmain.cpp         # DLL entry point
│   ├── Py4GW.cpp           # Main pybind11 module
│   └── py_*.cpp            # Individual Python module bindings
├── include/                # Header files
├── vendor/
│   └── gwca/               # Guild Wars Client API
├── pybind11/               # Python/C++ binding library
├── bin/                    # Output directory
└── CMakeLists.txt          # Build configuration
```

## Exposed Python Modules

The DLL exposes the following embedded Python modules:

- `Py4GW` - Main module with Console and Game submodules
- `PyAgent` - Agent/NPC management
- `PyPlayer` - Player character control
- `PyParty` - Party management
- `PyMap` - World map access
- `PySkill` / `PySkillbar` - Skill system
- `PyInventory` / `PyItem` - Item management
- `PyImGui` - ImGui UI integration
- `PyOverlay` / `Py2DRenderer` - Overlay rendering
- `PyCamera` - Camera control
- `PyScanner` - Memory scanning
- `PyKeystroke` / `PyMouse` - Input simulation

## Troubleshooting

### "Python is 64-bit, chosen compiler is 32-bit"
Make sure you're using Python 32-bit. Check with:
```
py -3.13-32 -c "import struct; print(struct.calcsize('P') * 8)"
```
Should output `32`.

### Missing d3dx9.lib
Follow step 1 above to download and install the D3DX9 libraries from the NuGet package.

### Linker errors for D3DX functions
Ensure the D3DX9 libs are in `vendor/gwca/Dependencies/DirectX/Lib/x86/` and that `D3DX9_43.dll` is in the `bin/` directory.
