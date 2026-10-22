#include "UI.h"

#include "Settings.h"
#include "Subtitles.h"

#include <spdlog/spdlog.h>

#ifdef SUBTITLESCALER_MENU

#include "SKSEMenuFramework.h"

#if __has_include(<imgui/imgui.h>)
#	include <imgui/imgui.h>
#else
#	include <imgui.h>
#endif

namespace
{
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

	void __stdcall Render()
	{
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
}

namespace UI
{
	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
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
