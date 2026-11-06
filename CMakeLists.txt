cmake_minimum_required(VERSION 3.20)
project(SamAteMyRAM VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Altman WIN32
    src/main.cpp
    src/memory_manager.cpp
    src/mascot.cpp
    src/audio.cpp
    src/ui_renderer.cpp
    res/app.rc
)

target_include_directories(Altman PRIVATE src res)
target_compile_definitions(Altman PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)

target_link_libraries(Altman PRIVATE
    d2d1
    dwrite
    winmm
    psapi
    ole32
    uuid
    dwmapi
    user32
    gdi32
    kernel32
)

if(MINGW)
    target_compile_options(Altman PRIVATE -municode -Wall -Wextra -Wno-unused-parameter)
    target_link_options(Altman PRIVATE -municode -static -static-libgcc -static-libstdc++ -mwindows)
endif()

if(MSVC)
    target_compile_options(Altman PRIVATE /W4 /EHsc /utf-8)
    set_property(TARGET Altman PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_target_properties(Altman PROPERTIES OUTPUT_NAME "Altman")
