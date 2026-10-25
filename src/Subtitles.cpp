#include "Subtitles.h"

#include "Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <string_view>
#include <thread>

namespace
{
	using EntryPtr = std::shared_ptr<Settings::Entry>;

	constexpr int kMaxAttempts = 20;  // 20 * 500 ms = 10 seconds of waiting for the menu

	void QueueApply(const EntryPtr& a_entry, int a_attempt, bool a_withRetry);

	// Clips, text fields and arrays in Scaleform have a type >= Object (6, 7, 8 in Scaleform numbering).
	// Checked by type number, because IsObject() means either only "object"
	// or "object, array or display object" depending on the CommonLibSSE version.
	bool IsClip(const RE::GFxValue& a_value)
	{
		const auto t = static_cast<int>(a_value.GetType());
		return t >= 6 && t <= 8;
	}

#ifdef SUBTITLESCALER_DUMP
	// Debug: lists the members of an object, if this CommonLibSSE version supports VisitMembers.
	template <class V>
	void EnumerateMembers(V& a_obj, const std::string& a_path)
	{
		if constexpr (requires(V& v, typename V::ObjectVisitor* p) { v.VisitMembers(p); }) {
			struct Dumper : V::ObjectVisitor
			{
				void Visit(const char* a_name, const V& a_val) override
				{
					spdlog::info("    member: {} (type {})", a_name, static_cast<int>(a_val.GetType()));
				}
			};
			spdlog::info("  Members of {}:", a_path);
			Dumper dumper;
			a_obj.VisitMembers(&dumper);
		} else {
			spdlog::info("  Member enumeration of {} is not available in this CommonLibSSE version", a_path);
		}
	}

	void DumpPath(RE::GFxMovieView* a_movie, const std::string& a_path)
	{
		RE::GFxValue obj;
		if (a_movie->GetVariable(&obj, a_path.c_str()) && IsClip(obj)) {
			spdlog::info("  {} found (type {})", a_path, static_cast<int>(obj.GetType()));
			EnumerateMembers(obj, a_path);
		} else {
			spdlog::info("  {} not found", a_path);
		}
	}

	void DumpEntry(const EntryPtr& a_entry, RE::GFxMovieView* a_movie)
	{
		spdlog::info("[{}] element not found. Diagnostics of menu '{}':", a_entry->name, a_entry->GetMenu());
		DumpPath(a_movie, "_root");

		// Check the path chain piece by piece: shows at which link it breaks.
		for (const auto& path : a_entry->GetPaths()) {
			std::string prefix;
			std::size_t start = 0;
			while (start <= path.size()) {
				const auto dot = path.find('.', start);
				const auto part = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
				prefix += (prefix.empty() ? "" : ".") + part;
				if (prefix != "_root") {
					DumpPath(a_movie, prefix);
				}
				if (dot == std::string::npos) {
					break;
				}
				start = dot + 1;
			}
		}
	}
#endif

	// Reads an object member. false if the member is missing or undefined.
	bool GetMemberSafe(RE::GFxValue& a_obj, const char* a_name, RE::GFxValue& a_out)
	{
		RE::GFxValue v;
		if (!a_obj.GetMember(a_name, &v) || v.IsUndefined()) {
			return false;
		}
		a_out = v;
		return true;
	}

	// Names under which a text field may live inside a holder clip.
	constexpr const char* kTextFieldNames[] = { "SubtitleText", "SubtitleTextField", "TextField", "textField", "tf", "text_tf" };

	// Looks for a text field whose width can be limited (once per menu opening).
	void FindTextField(const EntryPtr& a_entry, RE::GFxValue& a_clip)
	{
		auto& st = a_entry->ui;
		if (st.tfSearched) {
			return;
		}
		st.tfSearched = true;

		RE::GFxValue probe;
		if (GetMemberSafe(a_clip, "wordWrap", probe)) {
			st.tfName = ".";  // the element itself is a text field
		} else {
			for (const auto* name : kTextFieldNames) {
				RE::GFxValue child;
				if (GetMemberSafe(a_clip, name, child) && GetMemberSafe(child, "wordWrap", probe)) {
					st.tfName = name;
					break;
				}
			}
		}

		spdlog::info("[{}] text field for width limiting: {}", a_entry->name,
			st.tfName.empty() ? "not found" : (st.tfName == "." ? "the element itself" : st.tfName.c_str()));
	}

	// Limits the visible width of the text field (line wrapping) or restores the original width.
	void ApplyWidth(const EntryPtr& a_entry, RE::GFxValue& a_clip, bool a_limit, double a_maxW, double a_target, double a_visualW)
	{
		auto& st = a_entry->ui;
		if (st.tfName.empty() || (!a_limit && !st.limited)) {
			return;  // nothing to change
		}

		if (st.tfName == ".") {
			// The element itself: width is set in visible units (scale included).
			if (a_limit) {
				a_clip.SetMember("wordWrap", RE::GFxValue(true));
				a_clip.SetMember("multiline", RE::GFxValue(true));
			}
			a_clip.SetMember("_width", RE::GFxValue(a_limit ? a_maxW : a_visualW));

			// Safeguard: if setting the width changed the text scale, restore the scale.
			RE::GFxValue xs;
			if (a_clip.GetMember("_xscale", &xs) && xs.IsNumber() && std::abs(xs.GetNumber() - a_target) > 0.5) {
				spdlog::warn("[{}] setting the width changed the scale ({} instead of {}), scale restored", a_entry->name, xs.GetNumber(), a_target);
				a_clip.SetMember("_xscale", RE::GFxValue(a_target));
				a_clip.SetMember("_yscale", RE::GFxValue(a_target));
			}
		} else {
			// Child field: width is set in the holder's local units, visible = local * scale.
			RE::GFxValue tf;
			if (!GetMemberSafe(a_clip, st.tfName.c_str(), tf)) {
				return;
			}
			if (!st.tfW0) {
				RE::GFxValue w, x;
				tf.GetMember("_width", &w);
				tf.GetMember("_x", &x);
				st.tfW0 = w.IsNumber() ? w.GetNumber() : 0.0;
				st.tfX0 = x.IsNumber() ? x.GetNumber() : 0.0;
				RE::GFxValue wrap, multi, autoSize;
				GetMemberSafe(tf, "wordWrap", wrap);
				GetMemberSafe(tf, "multiline", multi);
				GetMemberSafe(tf, "autoSize", autoSize);
				spdlog::info("[{}] field '{}': width {}, wordWrap={}, multiline={}, autoSize={}", a_entry->name, st.tfName, *st.tfW0,
					wrap.IsBool() ? static_cast<int>(wrap.GetBool()) : -1, multi.IsBool() ? static_cast<int>(multi.GetBool()) : -1,
					autoSize.IsString() ? autoSize.GetString() : "(not a string)");
			}
			const double w0 = *st.tfW0;
			const double x0 = *st.tfX0;
			const double w = a_maxW * 100.0 / a_target;
			if (a_limit && w < w0) {
				tf.SetMember("wordWrap", RE::GFxValue(true));
				tf.SetMember("multiline", RE::GFxValue(true));
				tf.SetMember("_width", RE::GFxValue(w));
				tf.SetMember("_x", RE::GFxValue(x0 + (w0 - w) / 2.0));  // the field's center stays in place
			} else {
				tf.SetMember("_width", RE::GFxValue(w0));
				tf.SetMember("_x", RE::GFxValue(x0));
			}
		}
		st.limited = a_limit;
	}

	// Gets the text field of an element (found by FindTextField).
	bool GetTextFieldValue(const EntryPtr& a_entry, RE::GFxValue& a_clip, RE::GFxValue& a_out)
	{
		const auto& name = a_entry->ui.tfName;
		if (name.empty()) {
			return false;
		}
		if (name == ".") {
			a_out = a_clip;
			return true;
		}
		return GetMemberSafe(a_clip, name.c_str(), a_out);
	}

	// Sets the custom text color, or restores the original one.
	void ApplyColor(const EntryPtr& a_entry, RE::GFxValue& a_clip, bool a_on)
	{
		auto&     st = a_entry->ui;
		const int color = a_on ? a_entry->color.load() : -1;
		if (color < 0 && !st.colorApplied) {
			return;
		}

		RE::GFxValue tf;
		if (!GetTextFieldValue(a_entry, a_clip, tf)) {
			if (color >= 0 && !st.warnedColor) {
				st.warnedColor = true;
				spdlog::warn("[{}] no text field found, the custom color is not supported for this target", a_entry->name);
			}
			return;
		}

		if (color >= 0) {
			if (!st.origColor) {
				RE::GFxValue cur;
				st.origColor = (tf.GetMember("textColor", &cur) && cur.IsNumber()) ? cur.GetNumber() : 16777215.0;
			}
			tf.SetMember("textColor", RE::GFxValue(static_cast<double>(color)));
			if (!st.colorApplied) {
				spdlog::info("[{}] text color {:06X} applied", a_entry->name, color & 0xFFFFFF);
			}
			st.colorApplied = true;
		} else {
			if (st.origColor) {
				tf.SetMember("textColor", RE::GFxValue(*st.origColor));
			}
			st.origColor.reset();
			st.colorApplied = false;
		}
	}

	bool TryApply(const EntryPtr& a_entry, RE::GFxMovieView* a_movie)
	{
		auto&      st = a_entry->ui;
		const bool on = a_entry->enabled.load();

		// Target is disabled and we have not touched it: nothing to restore.
		if (!on && !st.baseX) {
			return true;
		}

		for (const auto& path : a_entry->GetPaths()) {
			RE::GFxValue clip;
			if (!a_movie->GetVariable(&clip, path.c_str()) || !IsClip(clip)) {
				continue;
			}

			double       currentScale = 100.0;
			RE::GFxValue v;
			if (clip.GetMember("_xscale", &v) && v.IsNumber()) {
				currentScale = v.GetNumber();
			}

			// A freshly created clip has scale 100: remember its original position.
			RE::GFxValue vx, vy, vw;
			clip.GetMember("_x", &vx);
			clip.GetMember("_y", &vy);
			clip.GetMember("_width", &vw);
			if (std::abs(currentScale - 100.0) < 0.01 || !st.baseX || !st.baseY) {
				st.baseX = vx.IsNumber() ? vx.GetNumber() : 0.0;
				st.baseY = vy.IsNumber() ? vy.GetNumber() : 0.0;
				// Width at 100%: if the clip is already enlarged, recompute back.
				// Skipped while we limit the width ourselves: the measured width is then our own limit.
				if (!st.limited || !st.baseW) {
					st.baseW = vw.IsNumber() ? vw.GetNumber() * 100.0 / std::max(currentScale, 1.0) : 0.0;
				}
			}

			FindTextField(a_entry, clip);

			// A disabled target returns to its original look.
			double       target = on ? a_entry->scale.load() : 100.0;
			const double ox = on ? a_entry->offsetX.load() : 0.0;
			const double oy = on ? a_entry->offsetY.load() : 0.0;
			const double maxW = on ? a_entry->maxWidth.load() : 0.0;
			const double baseW = st.baseW.value_or(0.0);

			// Width limit: enlarged text must not run off the screen edge.
			bool limit = false;
			if (maxW > 0.0 && baseW > 0.0 && baseW * target / 100.0 > maxW) {
				if (st.tfName.empty()) {
					// Text field not found, nothing to wrap lines with: reduce the scale so the width fits.
					target = std::max(100.0, maxW / baseW * 100.0);
					if (!st.warnedNoTf) {
						st.warnedNoTf = true;
						spdlog::warn("[{}] text field not found, line wrapping impossible: scale limited to {}% so the text stays on screen",
							a_entry->name, target);
					}
				} else {
					limit = true;
				}
			}

			const double visualW = baseW * target / 100.0;
			const double effW = limit ? maxW : visualW;

			// Centering: shift left by half of the width increase so the element's center does not drift.
			const double anchorShift = (on && a_entry->centerAnchor.load() && baseW > 0.0) ? -(effW - baseW) / 2.0 : 0.0;

			clip.SetMember("_xscale", RE::GFxValue(target));
			clip.SetMember("_yscale", RE::GFxValue(target));
			clip.SetMember("_x", RE::GFxValue(*st.baseX + ox + anchorShift));
			clip.SetMember("_y", RE::GFxValue(*st.baseY + oy));

			ApplyWidth(a_entry, clip, limit, maxW, target, visualW);
			ApplyColor(a_entry, clip, on);

			if (std::abs(st.lastLoggedScale - target) > 0.001) {
				spdlog::info("[{}] scale {}% applied to '{}' (width at 100%: {}, width limit: {})",
					a_entry->name, target, path, baseW, limit ? "enabled" : "no");
				st.lastLoggedScale = target;
			}

			if (!on) {
				st.baseX.reset();
				st.baseY.reset();
				st.baseW.reset();
			}
			return true;
		}
		return false;
	}

	void ApplyOnUI(const EntryPtr& a_entry, int a_attempt, bool a_withRetry)
	{
		const auto menuName = a_entry->GetMenu();
		if (menuName.empty()) {
			return;
		}

		if (const auto ui = RE::UI::GetSingleton()) {
			const auto menu = ui->GetMenu(menuName);
			if (menu && menu->uiMovie && TryApply(a_entry, menu->uiMovie.get())) {
				return;
			}

#ifdef SUBTITLESCALER_DUMP
			if (menu && menu->uiMovie && (!a_withRetry || a_attempt == kMaxAttempts - 1)) {
				DumpEntry(a_entry, menu->uiMovie.get());
			}
#endif
		}

		if (a_withRetry && a_attempt + 1 < kMaxAttempts) {
			std::thread([a_entry, a_attempt]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				QueueApply(a_entry, a_attempt + 1, true);
			}).detach();
		} else if (a_withRetry && !a_entry->ui.loggedMissing) {
			a_entry->ui.loggedMissing = true;
			spdlog::warn("[{}] element not found at any path from the INI (Paths) in menu '{}'. "
						 "Check the menu name and path, or build the plugin with SUBTITLESCALER_DUMP=ON "
						 "to print the menu structure to the log.",
				a_entry->name, menuName);
		}
	}

	void QueueApply(const EntryPtr& a_entry, int a_attempt, bool a_withRetry)
	{
		if (const auto task = SKSE::GetTaskInterface()) {
			task->AddUITask([a_entry, a_attempt, a_withRetry]() { ApplyOnUI(a_entry, a_attempt, a_withRetry); });
		}
	}

	class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static MenuSink* GetSingleton()
		{
			static MenuSink singleton;
			return std::addressof(singleton);
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (a_event && a_event->opening) {
				const std::string_view name = a_event->menuName.c_str();
				for (const auto& entry : Settings::GetEntries()) {
					if (entry->GetMenu() != name) {
						continue;
					}
					// The menu is recreated on every opening, so the original position
					// of the element must be remembered again. The reset runs on the UI thread
					// before the apply itself (tasks run in order).
					if (const auto task = SKSE::GetTaskInterface()) {
						task->AddUITask([entry]() { entry->ui = Settings::Entry::UiState{}; });
					}
					Subtitles::Apply(entry, true);
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};
}

// ---------------------------------------------------------------------------
// Per-frame color enforcement. The game may reset the text color when it shows a new line, so after each
// frame of the HUD and dialogue menus the color is set again if it differs.
// ---------------------------------------------------------------------------
namespace
{
	void EnforceColor(RE::IMenu* a_menu, std::string_view a_menuName)
	{
		auto* movie = a_menu ? a_menu->uiMovie.get() : nullptr;
		if (!movie) {
			return;
		}

		for (const auto& e : Settings::GetEntries()) {
			const int color = e->color.load();
			if (color < 0 || !e->enabled.load() || !e->ui.colorApplied || e->GetMenu() != a_menuName) {
				continue;
			}
			for (const auto& path : e->GetPaths()) {
				RE::GFxValue clip;
				if (!movie->GetVariable(&clip, path.c_str()) || !IsClip(clip)) {
					continue;
				}
				RE::GFxValue tf;
				if (!GetTextFieldValue(e, clip, tf)) {
					break;
				}
				RE::GFxValue cur;
				if (!tf.GetMember("textColor", &cur) || !cur.IsNumber() || (static_cast<int>(cur.GetNumber()) & 0xFFFFFF) != color) {
					tf.SetMember("textColor", RE::GFxValue(static_cast<double>(color)));
				}
				break;
			}
		}
	}

	using AdvanceMovie_t = void (*)(RE::IMenu*, float, std::uint32_t);
	AdvanceMovie_t g_origHudAdvance = nullptr;
	AdvanceMovie_t g_origDialogueAdvance = nullptr;

	void Hook_HudAdvance(RE::IMenu* a_menu, float a_interval, std::uint32_t a_time)
	{
		g_origHudAdvance(a_menu, a_interval, a_time);
		EnforceColor(a_menu, "HUD Menu");
	}

	void Hook_DialogueAdvance(RE::IMenu* a_menu, float a_interval, std::uint32_t a_time)
	{
		g_origDialogueAdvance(a_menu, a_interval, a_time);
		EnforceColor(a_menu, "Dialogue Menu");
	}
}

namespace Subtitles
{
	void InstallColorHooks()
	{
		constexpr std::size_t kAdvanceMovieIndex = 0x5;  // IMenu::AdvanceMovie

		REL::Relocation<std::uintptr_t> hud{ RE::VTABLE_HUDMenu[0] };
		g_origHudAdvance = reinterpret_cast<AdvanceMovie_t>(hud.write_vfunc(kAdvanceMovieIndex, reinterpret_cast<std::uintptr_t>(&Hook_HudAdvance)));

		REL::Relocation<std::uintptr_t> dlg{ RE::VTABLE_DialogueMenu[0] };
		g_origDialogueAdvance = reinterpret_cast<AdvanceMovie_t>(dlg.write_vfunc(kAdvanceMovieIndex, reinterpret_cast<std::uintptr_t>(&Hook_DialogueAdvance)));

		spdlog::info("Color hooks installed on HUD Menu and Dialogue Menu");
	}

	void Apply(const std::shared_ptr<Settings::Entry>& a_entry, bool a_withRetry)
	{
		if (a_entry) {
			QueueApply(a_entry, 0, a_withRetry);
		}
	}

	void ApplyAll(bool a_withRetry)
	{
		for (const auto& entry : Settings::GetEntries()) {
			// Waiting for the menu only makes sense for HUD: it exists for the whole game.
			// Other menus are applied when they open (MenuSink).
			Apply(entry, a_withRetry && entry->GetMenu() == Settings::kHudMenu);
		}
	}

	void RegisterMenuSink()
	{
		if (const auto ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
			spdlog::info("Subscribed to menu events");
		}
	}
}
