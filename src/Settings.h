#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Plugin settings. Values are read from Data/SKSE/Plugins/SubtitleScaler.ini
// and can be changed at runtime from Papyrus.
//
// Each INI section describes one "target" (Entry): a text element inside a menu that
// should be enlarged. There are three targets: HUD (NPC subtitles in the world), Dialogue
// (an NPC line in the dialogue window) and PlayerChoices (the player's response options).
namespace Settings
{
	constexpr float       kMinScale = 50.0f;
	constexpr float       kMaxScale = 400.0f;
	constexpr const char* kHudMenu = "HUD Menu";

	class Entry
	{
	public:
		Entry(std::string a_name, std::string a_menu, std::vector<std::string> a_paths, bool a_enabled) :
			name(std::move(a_name)), enabled(a_enabled), _menu(std::move(a_menu)), _paths(std::move(a_paths))
		{}

		const std::string  name;  // INI section name
		std::atomic<bool>  enabled;
		std::atomic<float> scale{ 150.0f };  // in percent, 100 = original size
		std::atomic<float> offsetX{ 0.0f };  // X shift in Flash stage units
		std::atomic<float> offsetY{ 0.0f };  // Y shift in Flash stage units
		// true: when enlarged, the element stays centered in place instead of growing to the right from its left edge.
		std::atomic<bool> centerAnchor{ false };
		// Maximum visible width of the element in Flash stage units (0 = no limit). If the enlarged
		// element is wider, the text wraps onto new lines instead of running off the screen. Skyrim stage width: 1280.
		std::atomic<float> maxWidth{ 0.0f };
		// Text color as 0xRRGGBB, or -1 = keep the original color. Works for targets that have a text field
		// (HUD and Dialogue subtitles); the list of response options is not supported.
		std::atomic<int> color{ -1 };

		std::string GetMenu() const
		{
			std::scoped_lock lock(_lock);
			return _menu;
		}
		void SetMenu(std::string a_menu)
		{
			std::scoped_lock lock(_lock);
			_menu = std::move(a_menu);
		}
		std::vector<std::string> GetPaths() const
		{
			std::scoped_lock lock(_lock);
			return _paths;
		}
		void SetPaths(std::vector<std::string> a_paths)
		{
			std::scoped_lock lock(_lock);
			_paths = std::move(a_paths);
		}

		// State accessed only from the UI thread.
		struct UiState
		{
			std::optional<double> baseX;
			std::optional<double> baseY;
			std::optional<double> baseW;  // element width at 100% scale

			// Width limiting (see maxWidth)
			bool                  tfSearched = false;  // text field search already done
			std::string           tfName;              // "." = the element itself, otherwise the name of a child field; empty = not found
			std::optional<double> tfW0;                // original width and position of the child field
			std::optional<double> tfX0;
			bool                  limited = false;     // width is currently limited by us
			bool                  warnedNoTf = false;
			double                lastLoggedScale = -1.0;
			bool                  loggedMissing = false;

			// Text color (see color)
			std::optional<double> origColor;  // original textColor, remembered before the first change
			bool                  colorApplied = false;
			bool                  warnedColor = false;
		} ui;

	private:
		mutable std::mutex       _lock;
		std::string              _menu;   // internal menu name, e.g. "HUD Menu"
		std::vector<std::string> _paths;  // paths to the clip/text field, tried in order
	};

	// A copy of the target list (safe to use from any thread).
	std::vector<std::shared_ptr<Entry>> GetEntries();

	// Finds a target by section name (case-insensitive). nullptr if there is none.
	std::shared_ptr<Entry> Find(std::string_view a_name);

	// Reads the INI. If the file does not exist, default values stay in effect.
	void Load();

	// Writes the current values (Enabled, Scale, OffsetX, OffsetY, MaxWidth) to the INI.
	// Comments and the other lines of the file are preserved. Returns false on a write error.
	bool Save();
}
