cmake_minimum_required(VERSION 3.15)
project(JigsawMachine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Optimize for size and strip symbols
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -s")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -s")

# Enable Link Time Optimization (LTO)
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT lto_output)
if(lto_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(WARNING "IPO/LTO is not supported: ${lto_output}")
endif()

# Find SDL2 (MSYS2 usually provides SDL2Config.cmake)
# We use SDL2-static for the standalone executable requirement
find_package(SDL2 REQUIRED CONFIG)

# Include directories for third-party headers
include_directories(third_party)

# Source files
set(SOURCES
    src/main.cpp
)

# Executable target (WIN32 hides the console window on Windows)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# Link SDL2 statically. MSYS2 provides SDL2::SDL2-static target.
# If not available, fallback to standard SDL2::SDL2 but it might be dynamic.
if(TARGET SDL2::SDL2-static)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static)
else()
    message(WARNING "SDL2::SDL2-static not found! Falling back to SDL2::SDL2. Executable might require DLLs.")
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
endif()

# Setup UPX compression after build
find_program(UPX_CMD upx)
if(UPX_CMD)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${UPX_CMD} --best --lzma $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Compressing executable with UPX..."
    )
else()
    message(WARNING "UPX not found. Executable will not be compressed.")
endif()
