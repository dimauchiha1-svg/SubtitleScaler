#include "Subtitles.h"

#include "Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <chrono>
#include <cmath>
#include <string>
#include <string_view>
#include <thread>

namespace
{
	using EntryPtr = std::shared_ptr<Settings::Entry>;

	constexpr int kMaxAttempts = 20;  // 20 * 500 мс = 10 секунд ожидания меню

	void QueueApply(const EntryPtr& a_entry, int a_attempt, bool a_withRetry);

#ifdef SUBTITLESCALER_DUMP
	// Отладка: пишет в лог имена всех членов объекта, чтобы найти правильный путь к элементу интерфейса.
	struct MemberDumper : RE::GFxValue::ObjectVisitor
	{
		void Visit(const char* a_name, const RE::GFxValue& a_val) override
		{
			spdlog::info("    член: {} (тип {})", a_name, static_cast<int>(a_val.GetType()));
		}
	};

	void DumpMembers(RE::GFxMovieView* a_movie, const std::string& a_path)
	{
		RE::GFxValue obj;
		if (a_movie->GetVariable(&obj, a_path.c_str()) && obj.IsObject()) {
			spdlog::info("  Члены {}:", a_path);
			MemberDumper dumper;
			obj.VisitMembers(&dumper);
		} else {
			spdlog::info("  {} не найден", a_path);
		}
	}

	void DumpEntry(const EntryPtr& a_entry, RE::GFxMovieView* a_movie)
	{
		spdlog::info("[{}] элемент не найден. Содержимое меню '{}':", a_entry->name, a_entry->GetMenu());
		DumpMembers(a_movie, "_root");
		// Родитель первого пути, например "_root.DialogueMenu_mc" для "_root.DialogueMenu_mc.SubtitleText"
		for (const auto& path : a_entry->GetPaths()) {
			if (const auto dot = path.rfind('.'); dot != std::string::npos) {
				DumpMembers(a_movie, path.substr(0, dot));
			}
			break;
		}
	}
#endif

	bool TryApply(const EntryPtr& a_entry, RE::GFxMovieView* a_movie)
	{
		auto&      st = a_entry->ui;
		const bool on = a_entry->enabled.load();

		// Цель выключена и мы её ещё не трогали: восстанавливать нечего.
		if (!on && !st.baseX) {
			return true;
		}

		for (const auto& path : a_entry->GetPaths()) {
			RE::GFxValue clip;
			if (!a_movie->GetVariable(&clip, path.c_str()) || !clip.IsObject()) {
				continue;
			}

			double       currentScale = 100.0;
			RE::GFxValue v;
			if (clip.GetMember("_xscale", &v) && v.IsNumber()) {
				currentScale = v.GetNumber();
			}

			// Свежесозданный клип имеет масштаб 100: запоминаем его исходную позицию.
			RE::GFxValue vx, vy;
			clip.GetMember("_x", &vx);
			clip.GetMember("_y", &vy);
			if (std::abs(currentScale - 100.0) < 0.01 || !st.baseX || !st.baseY) {
				st.baseX = vx.IsNumber() ? vx.GetNumber() : 0.0;
				st.baseY = vy.IsNumber() ? vy.GetNumber() : 0.0;
			}

			// Выключенная цель возвращается к исходному виду.
			const double target = on ? a_entry->scale.load() : 100.0;
			const double ox = on ? a_entry->offsetX.load() : 0.0;
			const double oy = on ? a_entry->offsetY.load() : 0.0;

			clip.SetMember("_xscale", RE::GFxValue(target));
			clip.SetMember("_yscale", RE::GFxValue(target));
			clip.SetMember("_x", RE::GFxValue(*st.baseX + ox));
			clip.SetMember("_y", RE::GFxValue(*st.baseY + oy));

			if (std::abs(st.lastLoggedScale - target) > 0.001) {
				spdlog::info("[{}] масштаб {}% применён к '{}'", a_entry->name, target, path);
				st.lastLoggedScale = target;
			}

			if (!on) {
				st.baseX.reset();
				st.baseY.reset();
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
			spdlog::warn("[{}] элемент не найден ни по одному пути из INI (Paths) в меню '{}'. "
						 "Проверьте имя меню и путь или соберите плагин с SUBTITLESCALER_DUMP=ON, "
						 "чтобы вывести структуру меню в лог.",
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
					// Меню создаётся заново при каждом открытии, поэтому исходную позицию
					// элемента нужно запомнить заново. Сброс выполняется на UI-потоке
					// раньше самого применения (задачи идут по порядку).
					if (const auto task = SKSE::GetTaskInterface()) {
						task->AddUITask([entry]() {
							entry->ui.baseX.reset();
							entry->ui.baseY.reset();
							entry->ui.loggedMissing = false;
						});
					}
					Subtitles::Apply(entry, true);
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};
}

namespace Subtitles
{
	void Apply(const std::shared_ptr<Settings::Entry>& a_entry, bool a_withRetry)
	{
		if (a_entry) {
			QueueApply(a_entry, 0, a_withRetry);
		}
	}

	void ApplyAll(bool a_withRetry)
	{
		for (const auto& entry : Settings::GetEntries()) {
			// Ждать появления меню имеет смысл только для HUD: он существует всё время игры.
			// Остальные меню применяются при открытии (MenuSink).
			Apply(entry, a_withRetry && entry->GetMenu() == Settings::kHudMenu);
		}
	}

	void RegisterMenuSink()
	{
		if (const auto ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
			spdlog::info("Подписка на события меню оформлена");
		}
	}
}
