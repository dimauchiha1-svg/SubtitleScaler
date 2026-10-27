#include "Papyrus.h"

#include "Settings.h"
#include "Subtitles.h"

#include <algorithm>

namespace
{
	constexpr auto kScriptName = "SubtitleScaler";

	// asName: INI section name: "HUD", "Dialogue" or "PlayerChoices".

	float GetScale(RE::StaticFunctionTag*, std::string_view a_name)
	{
		const auto e = Settings::Find(a_name);
		return e ? e->scale.load() : 0.0f;
	}

	// Sets the scale of all targets at once.
	void SetScale(RE::StaticFunctionTag*, float a_percent)
	{
		const auto v = std::clamp(a_percent, Settings::kMinScale, Settings::kMaxScale);
		for (const auto& e : Settings::GetEntries()) {
			e->scale = v;
		}
		Subtitles::ApplyAll(false);
	}

	void SetTargetScale(RE::StaticFunctionTag*, std::string_view a_name, float a_percent)
	{
		if (const auto e = Settings::Find(a_name)) {
			e->scale = std::clamp(a_percent, Settings::kMinScale, Settings::kMaxScale);
			Subtitles::Apply(e, false);
		}
	}

	void SetOffset(RE::StaticFunctionTag*, std::string_view a_name, float a_x, float a_y)
	{
		if (const auto e = Settings::Find(a_name)) {
			e->offsetX = a_x;
			e->offsetY = a_y;
			Subtitles::Apply(e, false);
		}
	}

	void SetEnabled(RE::StaticFunctionTag*, std::string_view a_name, bool a_enabled)
	{
		if (const auto e = Settings::Find(a_name)) {
			e->enabled = a_enabled;
			Subtitles::Apply(e, false);
		}
	}

	bool IsEnabled(RE::StaticFunctionTag*, std::string_view a_name)
	{
		const auto e = Settings::Find(a_name);
		return e && e->enabled.load();
	}

	// afRGB: 0xRRGGBB (for example 16776960 = yellow), a negative value restores the original color.
	void SetTargetColor(RE::StaticFunctionTag*, std::string_view a_name, std::int32_t a_rgb)
	{
		if (const auto e = Settings::Find(a_name)) {
			e->color = a_rgb < 0 ? -1 : (a_rgb & 0xFFFFFF);
			Subtitles::Apply(e, false);
		}
	}

	// Returns 0xRRGGBB, or -1 if the original color is used.
	std::int32_t GetTargetColor(RE::StaticFunctionTag*, std::string_view a_name)
	{
		const auto e = Settings::Find(a_name);
		return e ? e->color.load() : -1;
	}

	// Switches writing to SubtitleScaler.log on or off.
	void SetLogging(RE::StaticFunctionTag*, bool a_enabled)
	{
		Settings::SetLogEnabled(a_enabled);
	}

	// Drop shadow. aiStrength 0-100. Works for "HUD", "Dialogue" and "PlayerChoices".
	void SetTargetShadow(RE::StaticFunctionTag*, std::string_view a_name, bool a_enabled, std::int32_t a_strength)
	{
		if (const auto e = Settings::Find(a_name); e && Settings::SupportsEffects(e->name)) {
			e->shadowEnabled = a_enabled;
			e->shadowStrength = std::clamp(a_strength, 0, 100);
			Subtitles::Apply(e, false);
		}
	}

	// Black outline. aiSize 1-4. Works for "HUD", "Dialogue" and "PlayerChoices".
	void SetTargetOutline(RE::StaticFunctionTag*, std::string_view a_name, bool a_enabled, std::int32_t a_size)
	{
		if (const auto e = Settings::Find(a_name); e && Settings::SupportsEffects(e->name)) {
			e->outlineEnabled = a_enabled;
			e->outlineSize = std::clamp(a_size, 1, 4);
			Subtitles::Apply(e, false);
		}
	}

	// Font name from the game's font configuration, for example "$EverywhereBoldFont". Empty = original font.
	// Works for "HUD", "Dialogue" and "PlayerChoices".
	void SetTargetFont(RE::StaticFunctionTag*, std::string_view a_name, std::string_view a_font)
	{
		if (const auto e = Settings::Find(a_name); e && Settings::SupportsFont(e->name)) {
			e->SetFont(std::string(a_font));
			Subtitles::Apply(e, false);
		}
	}

	void Reapply(RE::StaticFunctionTag*)
	{
		Subtitles::ApplyAll(true);
	}

	void ReloadConfig(RE::StaticFunctionTag*)
	{
		Settings::Load();
		Subtitles::ApplyAll(false);
	}
}

namespace Papyrus
{
	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("GetScale", kScriptName, GetScale);
		a_vm->RegisterFunction("SetScale", kScriptName, SetScale);
		a_vm->RegisterFunction("SetTargetScale", kScriptName, SetTargetScale);
		a_vm->RegisterFunction("SetOffset", kScriptName, SetOffset);
		a_vm->RegisterFunction("SetEnabled", kScriptName, SetEnabled);
		a_vm->RegisterFunction("IsEnabled", kScriptName, IsEnabled);
		a_vm->RegisterFunction("SetTargetColor", kScriptName, SetTargetColor);
		a_vm->RegisterFunction("GetTargetColor", kScriptName, GetTargetColor);
		a_vm->RegisterFunction("SetTargetShadow", kScriptName, SetTargetShadow);
		a_vm->RegisterFunction("SetTargetOutline", kScriptName, SetTargetOutline);
		a_vm->RegisterFunction("SetTargetFont", kScriptName, SetTargetFont);
		a_vm->RegisterFunction("SetLogging", kScriptName, SetLogging);
		a_vm->RegisterFunction("Reapply", kScriptName, Reapply);
		a_vm->RegisterFunction("ReloadConfig", kScriptName, ReloadConfig);
		return true;
	}
}
