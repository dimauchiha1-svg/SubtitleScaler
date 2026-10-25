#include "UI.h"

#include "Settings.h"
#include "Subtitles.h"

#include <spdlog/spdlog.h>

#ifdef SUBTITLESCALER_MENU

#include <Windows.h>

#include <atomic>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <filesystem>

// SKSE Menu Framework 3 API header. ImGui is called through the framework's own functions
// (ImGuiMCP namespace), so imgui.dll and ImGui libraries are not needed.
#include "SKSEMenuFramework.h"

namespace
{
	constexpr auto kFrameworkModule = L"SKSEMenuFramework";

	bool IsFrameworkInstalled()
	{
		return ::GetModuleHandleW(kFrameworkModule) != nullptr;
	}

	// The header calls framework functions via GetProcAddress without checking the result, so
	// before registering the page we make sure the needed functions exist in the installed framework version.
	bool HasRequiredExports()
	{
		static constexpr const char* kNames[] = {
			"AddSectionItem", "igGetVersion", "igGetCurrentContext", "igCheckbox", "igSliderFloat",
			"igDragFloat2", "igColorEdit3", "igButton", "igSameLine", "igSeparator", "igPushID_Str", "igPopID",
			"igTextV", "igTextWrappedV", "igTextUnformatted"
		};

		const auto module = ::GetModuleHandleW(kFrameworkModule);
		if (!module) {
			return false;
		}

		bool ok = true;
		for (const auto* name : kNames) {
			if (!::GetProcAddress(module, name)) {
				spdlog::error("Menu: SKSEMenuFramework.dll has no function {}", name);
				ok = false;
			}
		}
		return ok;
	}

	// Message about the result of the last action (saving / loading the INI).
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
		if (a_name == "HUDName") {
			return "Speaker name in world subtitles";
		}
		if (a_name == "DialogueName") {
			return "Speaker name in the dialogue window";
		}
		return a_name.c_str();
	}

	void RenderBody()
	{
		// Diagnostics: the first call is written to the log BEFORE touching ImGui.
		static bool s_logged = false;
		if (!s_logged) {
			s_logged = true;
			spdlog::info("Menu: first Render call");
			spdlog::info("Menu: ImGui {}, context {}", ImGuiMCP::GetVersion(), static_cast<const void*>(ImGuiMCP::GetCurrentContext()));
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

			float maxWidth = e->maxWidth.load();
			if (ImGuiMCP::SliderFloat("Max width (0 = off)", &maxWidth, 0.0f, 1280.0f, "%.0f")) {
				e->maxWidth = maxWidth;
				changed = true;
			}

			// Custom text color (only for targets that have a text field)
			if (e->name != "PlayerChoices") {
				bool useColor = e->color.load() >= 0;
				if (ImGuiMCP::Checkbox("Custom text color", &useColor)) {
					e->color = useColor ? 0xFFFFFF : -1;
					changed = true;
				}
				if (useColor) {
					const int c = e->color.load();
					float     rgb[3] = { static_cast<float>((c >> 16) & 0xFF) / 255.0f, static_cast<float>((c >> 8) & 0xFF) / 255.0f,
						static_cast<float>(c & 0xFF) / 255.0f };
					if (ImGuiMCP::ColorEdit3("Text color", rgb)) {
						const auto to8 = [](float f) { return std::clamp(static_cast<int>(f * 255.0f + 0.5f), 0, 255); };
						e->color = (to8(rgb[0]) << 16) | (to8(rgb[1]) << 8) | to8(rgb[2]);
						changed = true;
					}
				}
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

	// If an ImGui call still crashes, Windows raises an SEH exception. We catch it so as not to
	// take the game down: the page is disabled until restart.
	bool g_menuBroken = false;

	void LogMenuBroken()
	{
		spdlog::error("Menu: ImGui call crashed. The menu page is disabled until the game restarts, "
					  "settings are available in the INI.");
	}

	void __stdcall Render()
	{
		if (g_menuBroken) {
			return;
		}

		// This function has no objects with destructors, so __try can be used here.
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
			spdlog::info("SKSE Menu Framework is not installed, settings menu unavailable");
			return;
		}

		if (!HasRequiredExports()) {
			spdlog::error("Menu: the installed SKSE Menu Framework version is incompatible, settings menu disabled");
			return;
		}

		SKSEMenuFramework::SetSection("SubtitleScaler");
		SKSEMenuFramework::AddSectionItem("Subtitles", Render);
		spdlog::info("SubtitleScaler menu registered in SKSE Menu Framework");
	}
}

#else

namespace UI
{
	void Register()
	{
		spdlog::info("Plugin was built without SKSE Menu Framework support");
	}
}

#endif
