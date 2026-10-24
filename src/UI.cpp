#include "UI.h"

#include "Settings.h"
#include "Subtitles.h"

#include <spdlog/spdlog.h>

#ifdef SUBTITLESCALER_MENU

#include <Windows.h>

#include "SKSEMenuFramework.h"

#if __has_include(<imgui/imgui.h>)
#	include <imgui/imgui.h>
#else
#	include <imgui.h>
#endif

namespace
{
	// Заголовок текущей версии фреймворка не содержит IsInstalled(), поэтому проверяем сами:
	// DLL фреймворка загружается SKSE до нашего kPostLoad, если мод установлен.
	bool IsFrameworkInstalled()
	{
		return ::GetModuleHandleA("SKSEMenuFramework.dll") != nullptr;
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
		// Диагностика: первый вызов пишется в лог ДО обращения к ImGui. Если игра вылетает при открытии
		// страницы, по логу видно, дошло ли дело до ImGui и что вернул GetCurrentContext().
		static bool s_logged = false;
		if (!s_logged) {
			s_logged = true;
			spdlog::info("Меню: первый вызов Render");
			spdlog::info("Меню: ImGui {}, контекст {}", ImGui::GetVersion(), static_cast<const void*>(ImGui::GetCurrentContext()));
		}

		// Нет контекста ImGui значит, наша копия ImGui не та, которую использует фреймворк.
		// Рисовать нельзя (вылет), поэтому показываем ничего и пишем причину в лог.
		if (!ImGui::GetCurrentContext()) {
			static bool s_warned = false;
			if (!s_warned) {
				s_warned = true;
				spdlog::error("Меню: у ImGui нет текущего контекста. Версия ImGui в плагине не совпадает с фреймворком.");
			}
			return;
		}

		ImGui::TextWrapped("Changes are applied immediately. World subtitles update on the next spoken line, "
						   "dialogue window elements the next time the window is opened.");

		for (const auto& e : Settings::GetEntries()) {
			ImGui::PushID(e->name.c_str());
			ImGui::Separator();
			ImGui::Text("%s", GetTitle(e->name));

			bool changed = false;

			bool on = e->enabled.load();
			if (ImGui::Checkbox("Enabled", &on)) {
				e->enabled = on;
				changed = true;
			}

			float scale = e->scale.load();
			if (ImGui::SliderFloat("Scale (%)", &scale, Settings::kMinScale, Settings::kMaxScale, "%.0f")) {
				e->scale = scale;
				changed = true;
			}

			float offset[2] = { e->offsetX.load(), e->offsetY.load() };
			if (ImGui::DragFloat2("Offset X / Y", offset, 1.0f, -500.0f, 500.0f, "%.0f")) {
				e->offsetX = offset[0];
				e->offsetY = offset[1];
				changed = true;
			}

			if (ImGui::Button("Original size")) {
				e->scale = 100.0f;
				e->offsetX = 0.0f;
				e->offsetY = 0.0f;
				changed = true;
			}

			if (changed) {
				Subtitles::Apply(e, false);
			}
			ImGui::PopID();
		}

		ImGui::Separator();
		if (ImGui::Button("Save to INI")) {
			g_status = Settings::Save() ? "Saved to SubtitleScaler.ini" : "Could not write SubtitleScaler.ini (see log)";
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload from INI")) {
			Settings::Load();
			Subtitles::ApplyAll(false);
			g_status = "Reloaded from SubtitleScaler.ini";
		}
		if (g_status[0] != '\0') {
			ImGui::TextUnformatted(g_status);
		}
	}

	// Если ImGui недоступен (например, не найден imgui.dll или в нём нет нужной функции),
	// Windows бросает SEH-исключение. Ловим его, чтобы не уронить игру: меню отключается до перезапуска.
	bool g_menuBroken = false;

	void LogMenuBroken()
	{
		spdlog::error("Меню: вызов ImGui привёл к аварии (вероятно, нет imgui.dll или версия ImGui не совпадает "
					  "с фреймворком). Страница меню отключена до перезапуска игры, настройки доступны в INI.");
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
