#include "Papyrus.h"

#include "Settings.h"
#include "Subtitles.h"

#include <algorithm>

namespace
{
	constexpr auto kScriptName = "SubtitleScaler";

	// asName: имя секции INI: "HUD", "Dialogue" или "PlayerChoices".

	float GetScale(RE::StaticFunctionTag*, std::string_view a_name)
	{
		const auto e = Settings::Find(a_name);
		return e ? e->scale.load() : 0.0f;
	}

	// Устанавливает масштаб для всех целей сразу.
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
		a_vm->RegisterFunction("Reapply", kScriptName, Reapply);
		a_vm->RegisterFunction("ReloadConfig", kScriptName, ReloadConfig);
		return true;
	}
}
