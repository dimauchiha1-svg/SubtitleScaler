cmake_minimum_required(VERSION 3.21)

project(SubtitleScaler
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Совпадает с триплетом x64-windows-static-md (статические библиотеки, динамическая CRT)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

option(SUBTITLESCALER_DUMP "Писать в лог структуру меню, если элемент субтитров не найден" OFF)
option(SUBTITLESCALER_MENU "Меню настроек для SKSE Menu Framework" ON)
set(MENU_SDK_DIR "" CACHE PATH "Папка с клоном https://github.com/Thiago099/SKSE-Menu-Framework-SDK")

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

if(SUBTITLESCALER_DUMP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SUBTITLESCALER_DUMP)
endif()

# ---------------------------------------------------------------------------
# SKSE Menu Framework (необязательно). Нужен SDK: https://github.com/Thiago099/SKSE-Menu-Framework-SDK
# Ищем в нём SKSEMenuFramework.h/.lib и сборку ImGui. Если что-то не найдено,
# плагин собирается без меню (предупреждение в логе CMake).
# ---------------------------------------------------------------------------
if(SUBTITLESCALER_MENU)
    set(_menu_ok FALSE)

    if(MENU_SDK_DIR AND IS_DIRECTORY "${MENU_SDK_DIR}")
        file(GLOB_RECURSE _mf_libs "${MENU_SDK_DIR}/Mods/*SKSEMenuFramework.lib")
        file(GLOB_RECURSE _mf_hdrs "${MENU_SDK_DIR}/Mods/*SKSEMenuFramework.h")

        # Для Release берём сборку без суффикса " DEV"; если такой нет, подойдёт любая.
        set(_mf_lib "")
        foreach(_f IN LISTS _mf_libs)
            if(NOT "${_f}" MATCHES " DEV")
                set(_mf_lib "${_f}")
            endif()
        endforeach()
        if(NOT _mf_lib AND _mf_libs)
            list(GET _mf_libs 0 _mf_lib)
        endif()

        set(_mf_hdr "")
        foreach(_f IN LISTS _mf_hdrs)
            if(NOT "${_f}" MATCHES " DEV")
                set(_mf_hdr "${_f}")
            endif()
        endforeach()
        if(NOT _mf_hdr AND _mf_hdrs)
            list(GET _mf_hdrs 0 _mf_hdr)
        endif()

        if(_mf_lib AND _mf_hdr)
            get_filename_component(_mf_hdr_dir "${_mf_hdr}" DIRECTORY)
            string(REGEX REPLACE "\\.lib$" ".dll" _mf_dll "${_mf_lib}")

            list(APPEND CMAKE_PREFIX_PATH
                "${MENU_SDK_DIR}/ImGui/release/lib/cmake"
                "${MENU_SDK_DIR}/ImGui/release")
            find_package(ImGui CONFIG QUIET)

            if(ImGui_FOUND)
                add_library(SKSEMenuFramework SHARED IMPORTED)
                set_target_properties(SKSEMenuFramework PROPERTIES
                    IMPORTED_IMPLIB "${_mf_lib}"
                    IMPORTED_LOCATION "${_mf_dll}"
                    INTERFACE_INCLUDE_DIRECTORIES "${_mf_hdr_dir}")
                target_link_libraries(${PROJECT_NAME} PRIVATE SKSEMenuFramework ImGui::imgui)
                target_compile_definitions(${PROJECT_NAME} PRIVATE SUBTITLESCALER_MENU)
                set(_menu_ok TRUE)
                message(STATUS "SKSE Menu Framework: подключён (${_mf_lib})")
            else()
                message(WARNING "SKSE Menu Framework: ImGui не найден в '${MENU_SDK_DIR}/ImGui/release'")
            endif()
        else()
            message(WARNING "SKSE Menu Framework: SKSEMenuFramework.h/.lib не найдены в '${MENU_SDK_DIR}/Mods'")
        endif()
    else()
        message(WARNING "SKSE Menu Framework: не задан MENU_SDK_DIR (-DMENU_SDK_DIR=<путь к SDK>)")
    endif()

    if(NOT _menu_ok)
        message(WARNING "Плагин будет собран БЕЗ меню SKSE Menu Framework")
    endif()
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /permissive- /Zc:preprocessor)
endif()
