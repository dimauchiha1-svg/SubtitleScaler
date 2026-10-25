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
		a_vm->RegisterFunction("Reapply", kScriptName, Reapply);
		a_vm->RegisterFunction("ReloadConfig", kScriptName, ReloadConfig);
		return true;
	}
}
