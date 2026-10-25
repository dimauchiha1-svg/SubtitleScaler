#pragma once

#include "Settings.h"

#include <memory>

// Applies the scale to interface elements (subtitles, response options).
namespace Subtitles
{
	// Queues applying the scale to one target on the UI thread. Can be called from any thread.
	// a_withRetry = true: if the menu has not loaded yet, keep retrying for ~10 seconds.
	void Apply(const std::shared_ptr<Settings::Entry>& a_entry, bool a_withRetry);

	// Applies the settings to all targets. Waiting (retry) is used only for HUD targets,
	// other menus are handled when they open.
	void ApplyAll(bool a_withRetry);

	// Subscribes to menu open/close events (call after kDataLoaded).
	void RegisterMenuSink();

	// Installs per-frame hooks on the HUD and dialogue menus that keep the custom text color
	// (the game may reset the color when it shows a new line). Call after kDataLoaded.
	void InstallColorHooks();
}
