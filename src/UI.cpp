#include "UI.h"

#include "Settings.h"
#include "Subtitles.h"

#include <spdlog/spdlog.h>

#ifdef SUBTITLESCALER_MENU

#include <Windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <filesystem>

// Заголовок API SKSE Menu Framework 3. ImGui в нём вызывается через функции самого фреймворка
// (пространство имён ImGuiMCP), поэтому imgui.dll и библиотеки ImGui не нужны.
#include "SKSEMenuFramework.h"

namespace
{
	constexpr auto kFrameworkModule = L"SKSEMenuFramework";

	bool IsFrameworkInstalled()
	{
		return ::GetModuleHandleW(kFrameworkModule) != nullptr;
	}

	// Заголовок вызывает функции фреймворка через GetProcAddress без проверки результата, поэтому
	// перед регистрацией страницы убеждаемся, что нужные функции есть в установленной версии фреймворка.
	bool HasRequiredExports()
	{
		static constexpr const char* kNames[] = {
			"AddSectionItem", "igGetVersion", "igGetCurrentContext", "igCheckbox", "igSliderFloat",
			"igDragFloat2", "igButton", "igSameLine", "igSeparator", "igPushID_Str", "igPopID",
			"igTextV", "igTextWrappedV", "igTextUnformatted"
		};

		const auto module = ::GetModuleHandleW(kFrameworkModule);
		if (!module) {
			return false;
		}

		bool ok = true;
		for (const auto* name : kNames) {
			if (!::GetProcAddress(module, name)) {
				spdlog::error("Меню: в SKSEMenuFramework.dll нет функции {}", name);
				ok = false;
			}
		}
		return ok;
	}

	// Сообщение о результате последнего действия (сохранение / загрузка INI).
	const char* g_status = "";

	const char* GetTitle(const std::string& a_name)
	{
		if (a_name == "HUD") {
			return "World subtitles (NPC to NPC, NPC to player outside the dialogue window)";
		}
		if (a_name == "Dialogue") {
			return "NPC lines in the dialogue window";
		}
		if (a_name == "PlayerChoices") {
			return "Player response options";
		}
		return a_name.c_str();
	}

	void RenderBody()
	{
		// Диагностика: первый вызов пишется в лог ДО обращения к ImGui.
		static bool s_logged = false;
		if (!s_logged) {
			s_logged = true;
			spdlog::info("Меню: первый вызов Render");
			spdlog::info("Меню: ImGui {}, контекст {}", ImGuiMCP::GetVersion(), static_cast<const void*>(ImGuiMCP::GetCurrentContext()));
		}

		ImGuiMCP::TextWrapped("Changes are applied immediately. World subtitles update on the next spoken line, "
							  "dialogue window elements the next time the window is opened.");

		for (const auto& e : Settings::GetEntries()) {
			ImGuiMCP::PushID(e->name.c_str());
			ImGuiMCP::Separator();
			ImGuiMCP::Text("%s", GetTitle(e->name));

			bool changed = false;

			bool on = e->enabled.load();
			if (ImGuiMCP::Checkbox("Enabled", &on)) {
				e->enabled = on;
				changed = true;
			}

			float scale = e->scale.load();
			if (ImGuiMCP::SliderFloat("Scale (%)", &scale, Settings::kMinScale, Settings::kMaxScale, "%.0f")) {
				e->scale = scale;
				changed = true;
			}

			float offset[2] = { e->offsetX.load(), e->offsetY.load() };
			if (ImGuiMCP::DragFloat2("Offset X / Y", offset, 1.0f, -500.0f, 500.0f, "%.0f")) {
				e->offsetX = offset[0];
				e->offsetY = offset[1];
				changed = true;
			}

			if (ImGuiMCP::Button("Original size")) {
				e->scale = 100.0f;
				e->offsetX = 0.0f;
				e->offsetY = 0.0f;
				changed = true;
			}

			if (changed) {
				Subtitles::Apply(e, false);
			}
			ImGuiMCP::PopID();
		}

		ImGuiMCP::Separator();
		if (ImGuiMCP::Button("Save to INI")) {
			g_status = Settings::Save() ? "Saved to SubtitleScaler.ini" : "Could not write SubtitleScaler.ini (see log)";
		}
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button("Reload from INI")) {
			Settings::Load();
			Subtitles::ApplyAll(false);
			g_status = "Reloaded from SubtitleScaler.ini";
		}
		if (g_status[0] != '\0') {
			ImGuiMCP::TextUnformatted(g_status);
		}
	}

	// Если вызов ImGui всё же приведёт к аварии, Windows бросает SEH-исключение. Ловим его, чтобы не
	// уронить игру: страница отключается до перезапуска.
	bool g_menuBroken = false;

	void LogMenuBroken()
	{
		spdlog::error("Меню: вызов ImGui привёл к аварии. Страница меню отключена до перезапуска игры, "
					  "настройки доступны в INI.");
	}

	void __stdcall Render()
	{
		if (g_menuBroken) {
			return;
		}

		// В этой функции нет объектов с деструкторами, поэтому здесь можно использовать __try.
		__try {
			RenderBody();
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			g_menuBroken = true;
		}

		if (g_menuBroken) {
			static bool s_logged = false;
			if (!s_logged) {
				s_logged = true;
				LogMenuBroken();
			}
		}
	}
}

namespace UI
{
	void Register()
	{
		if (!IsFrameworkInstalled()) {
			spdlog::info("SKSE Menu Framework не установлен, меню настроек недоступно");
			return;
		}

		if (!HasRequiredExports()) {
			spdlog::error("Меню: установленная версия SKSE Menu Framework несовместима, меню настроек отключено");
			return;
		}

		SKSEMenuFramework::SetSection("SubtitleScaler");
		SKSEMenuFramework::AddSectionItem("Subtitles", Render);
		spdlog::info("Меню SubtitleScaler зарегистрировано в SKSE Menu Framework");
	}
}

#else

namespace UI
{
	void Register()
	{
		spdlog::info("Плагин собран без поддержки SKSE Menu Framework");
	}
}

#endif
