cmake_minimum_required(VERSION 3.21)

project(SubtitleScaler
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Статическая CRT: CommonLibSSE NG сам добавляет /MT, поэтому всё остальное
# (триплет vcpkg x64-windows-static) тоже собирается со статической CRT.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(SUBTITLESCALER_DUMP "Писать в лог структуру меню, если элемент субтитров не найден" OFF)
option(SUBTITLESCALER_MENU "Меню настроек для SKSE Menu Framework" ON)
set(MENU_API_DIR "" CACHE PATH "Папка с клоном https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API")

find_package(CommonLibSSE CONFIG REQUIRED)

# add_commonlibsse_plugin создаёт DLL, подключает CommonLibSSE и генерирует
# SKSEPlugin_Version / SKSEPlugin_Query (Address Library, работает на SE и AE).
add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR "SubtitleScaler"
    SOURCES
        src/main.cpp
        src/Papyrus.cpp
        src/Papyrus.h
        src/Settings.cpp
        src/Settings.h
        src/Subtitles.cpp
        src/Subtitles.h
        src/UI.cpp
        src/UI.h
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

# PCH подключается ко всем файлам цели, в том числе к сгенерированному файлу плагина.
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

if(SUBTITLESCALER_DUMP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SUBTITLESCALER_DUMP)
endif()

# ---------------------------------------------------------------------------
# SKSE Menu Framework 3 (необязательно). Нужен только заголовок SKSEMenuFramework.h из репозитория
# https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API (линковать ничего не нужно: функции
# фреймворка и ImGui вызываются через GetProcAddress). Если заголовок не найден,
# плагин собирается без меню (предупреждение в логе CMake).
# ---------------------------------------------------------------------------
if(SUBTITLESCALER_MENU)
    if(MENU_API_DIR AND EXISTS "${MENU_API_DIR}/SKSEMenuFramework.h")
        target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${MENU_API_DIR}")
        target_compile_definitions(${PROJECT_NAME} PRIVATE SUBTITLESCALER_MENU)
        message(STATUS "SKSE Menu Framework: заголовок найден (${MENU_API_DIR})")
    else()
        message(WARNING "SKSE Menu Framework: SKSEMenuFramework.h не найден в MENU_API_DIR='${MENU_API_DIR}'. "
                        "Плагин будет собран БЕЗ меню.")
    endif()
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /permissive- /Zc:preprocessor)
endif()
