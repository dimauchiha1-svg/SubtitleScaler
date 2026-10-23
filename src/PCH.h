#pragma once

// Предкомпилированный заголовок. CommonLibSSE требует, чтобы RE/Skyrim.h и SKSE/SKSE.h
// были включены раньше остальных своих заголовков (иначе REL/Relocation.h не компилируется).
// CMake подключает этот файл ко всем исходникам цели, включая сгенерированный add_commonlibsse_plugin.

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <spdlog/spdlog.h>
