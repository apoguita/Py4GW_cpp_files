cmake_minimum_required(VERSION 3.16)
project(Py4GW)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/")

# Automatically find all source files
file(GLOB_RECURSE PROJECT_SOURCES 
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.c"
)

# Automatically find all header files
file(GLOB_RECURSE PROJECT_HEADERS
    "${PROJECT_SOURCE_DIR}/include/*.h"
    "${PROJECT_SOURCE_DIR}/include/*.hpp"
)

# Create the DLL
add_library(Py4GW SHARED 
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
)

# Compiler options
add_compile_options(/MP /permissive-)
target_compile_definitions(Py4GW PRIVATE 
    NOMINMAX 
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

# Configure precompiled header
target_precompile_headers(Py4GW PRIVATE 
    ${PROJECT_SOURCE_DIR}/include/Headers.h
)
set_target_properties(Py4GW PROPERTIES
    PRECOMPILE_HEADERS_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/PCH"
)



# Add the pybind11 directory
add_subdirectory(pybind11)

# Find Python 3 interpreter and development files
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

# Include and link Python and Pybind11
target_include_directories(Py4GW PRIVATE
    ${Python3_INCLUDE_DIRS}  # Include Python headers
)

# Include GWCA (replacing FetchContent with local integration)
add_subdirectory(${PROJECT_SOURCE_DIR}/vendor/gwca)
target_include_directories(Py4GW PRIVATE ${PROJECT_SOURCE_DIR}/vendor/gwca/include)
target_link_libraries(Py4GW PRIVATE gwca)

include(FetchContent)
# Import ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.89.9
)
FetchContent_MakeAvailable(imgui)

# Create ImGui library
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)


# Set include directories for Py4GW
target_include_directories(Py4GW PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Import DirectXTex
FetchContent_Declare(
    DirectXTex
    GIT_REPOSITORY https://github.com/microsoft/DirectXTex
    GIT_TAG oct2024
)
FetchContent_GetProperties(directxtex)
if (NOT directxtex_POPULATED)
    FetchContent_Populate(directxtex)
    
    add_library(directxtex STATIC)
    set(DIRECTXTEX_SOURCES
        "${directxtex_SOURCE_DIR}/DDSTextureLoader/DDSTextureLoader9.h"
        "${directxtex_SOURCE_DIR}/DDSTextureLoader/DDSTextureLoader9.cpp"
        "${directxtex_SOURCE_DIR}/WICTextureLoader/WICTextureLoader9.h"
        "${directxtex_SOURCE_DIR}/WICTextureLoader/WICTextureLoader9.cpp"
    )
    target_sources(directxtex PRIVATE ${DIRECTXTEX_SOURCES})
    target_include_directories(directxtex PUBLIC "${directxtex_SOURCE_DIR}")
endif()

# Import nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(json)

# Include nlohmann/json in Py4GW
target_include_directories(Py4GW PRIVATE ${json_SOURCE_DIR}/include)


# Link everything
target_link_libraries(Py4GW PRIVATE 
    gwca
    imgui
    directxtex
    d3d9.lib
	pybind11::module           # Link against Pybind11
    ${Python3_LIBRARIES}       # Link Python libraries
)