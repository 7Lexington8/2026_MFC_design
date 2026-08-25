cmake_minimum_required(VERSION 3.21)
project(LocalSenseNova LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "LocalSenseNova is an MFC project and must be built with MSVC on Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MFC_FLAG 2)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/CMakeLists.txt")
    message(FATAL_ERROR "llama.cpp not found. Run setup_llama.bat first.")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/llama.cpp llama-build EXCLUDE_FROM_ALL)

add_executable(LocalSenseNova WIN32
    src/pch.cpp
    src/LocalSenseNova.cpp
    src/MainDlg.cpp
    src/LLMEngine.cpp
    src/ConversationManager.cpp
    src/Utf8.cpp
    src/LocalSenseNova.rc
)

set_property(TARGET LocalSenseNova PROPERTY MFC_FLAG 2)
set_property(TARGET LocalSenseNova PROPERTY VS_GLOBAL_CharacterSet "Unicode")

target_include_directories(LocalSenseNova PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/include
)

target_precompile_headers(LocalSenseNova PRIVATE src/pch.h)
target_compile_definitions(LocalSenseNova PRIVATE
    UNICODE
    _UNICODE
    _AFXDLL
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
)

target_link_libraries(LocalSenseNova PRIVATE llama)

if(MSVC)
    target_compile_options(LocalSenseNova PRIVATE /W4 /permissive- /utf-8)
endif()
