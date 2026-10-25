#include "Settings.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>

namespace
{
	constexpr auto kIniPath = "Data/SKSE/Plugins/SubtitleScaler.ini";

	std::string Trim(const std::string& a_str)
	{
		const auto first = a_str.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			return {};
		}
		const auto last = a_str.find_last_not_of(" \t\r\n");
		return a_str.substr(first, last - first + 1);
	}

	std::string Lower(std::string a_str)
	{
		std::transform(a_str.begin(), a_str.end(), a_str.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a_str;
	}

	bool ParseFloat(const std::string& a_str, float& a_out)
	{
		try {
			a_out = std::stof(a_str);
			return true;
		} catch (...) {
			return false;
		}
	}

	bool ParseBool(const std::string& a_str)
	{
		const auto v = Lower(a_str);
		return v == "1" || v == "true" || v == "yes" || v == "on";
	}

	std::mutex                                       g_mutex;
	std::vector<std::shared_ptr<Settings::Entry>>& Registry()
	{
		// Built-in targets. PlayerChoices is disabled by default
		// until the user enables it.
		static std::vector<std::shared_ptr<Settings::Entry>> entries = [] {
			using E = Settings::Entry;
			std::vector<std::shared_ptr<E>> v;
			auto hud = std::make_shared<E>("HUD", "HUD Menu",
				std::vector<std::string>{ "_root.HUDMovieBaseInstance.SubtitleTextHolder" }, true);
			hud->maxWidth = 1100.0f;
			v.push_back(std::move(hud));
			auto dialogue = std::make_shared<E>("Dialogue", "Dialogue Menu",
				std::vector<std::string>{ "_root.DialogueMenu_mc.SubtitleText" }, true);
			dialogue->centerAnchor = true;  // text in the dialogue window is centered: without this it drifts to the right
			dialogue->maxWidth = 1100.0f;   // long lines wrap instead of running off the screen
			v.push_back(std::move(dialogue));
			v.push_back(std::make_shared<E>("PlayerChoices", "Dialogue Menu",
				std::vector<std::string>{ "_root.DialogueMenu_mc.TopicListHolder" }, false));
			return v;
		}();
		return entries;
	}
}

namespace Settings
{
	std::vector<std::shared_ptr<Entry>> GetEntries()
	{
		std::scoped_lock lock(g_mutex);
		return Registry();
	}

	std::shared_ptr<Entry> Find(std::string_view a_name)
	{
		const auto       key = Lower(std::string(a_name));
		std::scoped_lock lock(g_mutex);
		for (auto& e : Registry()) {
			if (Lower(e->name) == key) {
				return e;
			}
		}
		return nullptr;
	}

	bool Save()
	{
		std::vector<std::string> lines;
		{
			std::ifstream in(kIniPath);
			std::string   l;
			while (std::getline(in, l)) {
				if (!l.empty() && l.back() == '\r') {
					l.pop_back();
				}
				lines.push_back(l);
			}
		}

		const auto fmt = [](float a_value) {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%g", a_value);
			return std::string(buf);
		};

		for (const auto& e : GetEntries()) {
			const std::pair<std::string, std::string> kv[] = {
				{ "Enabled", e->enabled.load() ? "1" : "0" },
				{ "Scale", fmt(e->scale.load()) },
				{ "OffsetX", fmt(e->offsetX.load()) },
				{ "OffsetY", fmt(e->offsetY.load()) },
				{ "MaxWidth", fmt(e->maxWidth.load()) },
				{ "Color", e->color.load() >= 0 ? [&] {
					char buf[8];
					std::snprintf(buf, sizeof(buf), "%06X", e->color.load() & 0xFFFFFF);
					return std::string(buf);
				}() : std::string() }
			};

			// Section bounds: header = the "[Name]" line, end = start of the next section.
			const auto nameKey = Lower(e->name);
			std::size_t header = lines.size();
			std::size_t end = lines.size();
			for (std::size_t i = 0; i < lines.size(); ++i) {
				const auto t = Trim(lines[i]);
				if (t.empty() || t[0] != '[') {
					continue;
				}
				if (header != lines.size()) {
					end = i;
					break;
				}
				const auto close = t.find(']');
				if (close != std::string::npos && Lower(Trim(t.substr(1, close - 1))) == nameKey) {
					header = i;
				}
			}

			// Section is missing: append it to the end of the file.
			if (header == lines.size()) {
				if (!lines.empty() && !lines.back().empty()) {
					lines.emplace_back();
				}
				lines.push_back("[" + e->name + "]");
				for (const auto& [k, v] : kv) {
					lines.push_back(k + "=" + v);
				}
				continue;
			}

			for (const auto& [k, v] : kv) {
				bool done = false;
				for (std::size_t i = header + 1; i < end; ++i) {
					const auto t = Trim(lines[i]);
					if (t.empty() || t[0] == ';' || t[0] == '#') {
						continue;
					}
					const auto eq = t.find('=');
					if (eq != std::string::npos && Lower(Trim(t.substr(0, eq))) == Lower(k)) {
						lines[i] = k + "=" + v;
						done = true;
						break;
					}
				}
				if (!done) {
					lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(header) + 1, k + "=" + v);
					++end;
				}
			}
		}

		std::ofstream out(kIniPath, std::ios::trunc);
		if (!out.is_open()) {
			spdlog::error("Could not open {} for writing", kIniPath);
			return false;
		}
		for (const auto& l : lines) {
			out << l << '\n';
		}
		out.flush();
		if (!out.good()) {
			spdlog::error("Error writing to {}", kIniPath);
			return false;
		}
		spdlog::info("Settings saved to {}", kIniPath);
		return true;
	}

	void Load()
	{
		std::ifstream file(kIniPath);
		if (!file.is_open()) {
			spdlog::warn("INI not found ({}), using default values", kIniPath);
			return;
		}

		std::shared_ptr<Entry>                    current = Find("HUD");  // keys before the first section belong to HUD
		std::map<Entry*, std::vector<std::string>> newPaths;
		std::string                               line;

		while (std::getline(file, line)) {
			line = Trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#') {
				continue;
			}

			// Section: [Name]
			if (line[0] == '[') {
				const auto close = line.find(']');
				const auto name = Trim(line.substr(1, close == std::string::npos ? std::string::npos : close - 1));
				if (name.empty()) {
					continue;
				}
				current = Find(name);
				if (!current) {
					spdlog::warn("Unknown INI section [{}] skipped", name);
				}
				continue;
			}

			// Skip keys of an unknown section.
			if (!current) {
				continue;
			}

			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}

			const auto key = Lower(Trim(line.substr(0, eq)));
			auto       val = Trim(line.substr(eq + 1));

			// Trailing comment: "Scale=150 ; text"
			if (const auto c = val.find(" ;"); c != std::string::npos) {
				val = Trim(val.substr(0, c));
			}

			float f = 0.0f;
			if (key == "enabled") {
				current->enabled = ParseBool(val);
			} else if (key == "scale") {
				if (ParseFloat(val, f)) {
					current->scale = std::clamp(f, kMinScale, kMaxScale);
				}
			} else if (key == "offsetx") {
				if (ParseFloat(val, f)) {
					current->offsetX = f;
				}
			} else if (key == "offsety") {
				if (ParseFloat(val, f)) {
					current->offsetY = f;
				}
			} else if (key == "maxwidth") {
				if (ParseFloat(val, f)) {
					current->maxWidth = std::max(0.0f, f);
				}
			} else if (key == "color") {
				auto hex = val;
				if (!hex.empty() && hex[0] == '#') {
					hex.erase(0, 1);
				}
				if (hex.size() == 6 && std::all_of(hex.begin(), hex.end(), [](unsigned char c) { return std::isxdigit(c) != 0; })) {
					current->color = static_cast<int>(std::stoul(hex, nullptr, 16));
				} else {
					current->color = -1;  // empty or invalid: original color
				}
			} else if (key == "anchor") {
				current->centerAnchor = (Lower(val) == "center");
			} else if (key == "menu") {
				current->SetMenu(val);
			} else if (key == "paths") {
				std::stringstream ss(val);
				std::string       item;
				while (std::getline(ss, item, '|')) {
					item = Trim(item);
					if (!item.empty()) {
						newPaths[current.get()].push_back(item);
					}
				}
			}
		}

		for (auto& [entry, paths] : newPaths) {
			entry->SetPaths(std::move(paths));
		}

		for (const auto& e : GetEntries()) {
			if (e->GetMenu().empty()) {
				if (e->enabled.load()) {
					spdlog::warn("Section [{}]: Menu is empty, section disabled", e->name);
				}
				e->enabled = false;
			}
			spdlog::info("[{}] menu='{}' enabled={} scale={} offsetX={} offsetY={}",
				e->name, e->GetMenu(), e->enabled.load(), e->scale.load(), e->offsetX.load(), e->offsetY.load());
		}
	}
}
