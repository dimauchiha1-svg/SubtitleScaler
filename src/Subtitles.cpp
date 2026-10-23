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

	// Клипы, текстовые поля и массивы в Scaleform имеют тип >= Object (6, 7, 8 в нумерации Scaleform).
	// Проверяем по номеру типа, потому что IsObject() в разных версиях CommonLibSSE означает
	// либо только "объект", либо "объект, массив или экранный объект".
	bool IsClip(const RE::GFxValue& a_value)
	{
		const auto t = static_cast<int>(a_value.GetType());
		return t >= 6 && t <= 8;
	}

#ifdef SUBTITLESCALER_DUMP
	// Отладка: перечисляет члены объекта, если эта версия CommonLibSSE умеет VisitMembers.
	template <class V>
	void EnumerateMembers(V& a_obj, const std::string& a_path)
	{
		if constexpr (requires(V& v, typename V::ObjectVisitor* p) { v.VisitMembers(p); }) {
			struct Dumper : V::ObjectVisitor
			{
				void Visit(const char* a_name, const V& a_val) override
				{
					spdlog::info("    член: {} (тип {})", a_name, static_cast<int>(a_val.GetType()));
				}
			};
			spdlog::info("  Члены {}:", a_path);
			Dumper dumper;
			a_obj.VisitMembers(&dumper);
		} else {
			spdlog::info("  Перечисление членов {} недоступно в этой версии CommonLibSSE", a_path);
		}
	}

	void DumpPath(RE::GFxMovieView* a_movie, const std::string& a_path)
	{
		RE::GFxValue obj;
		if (a_movie->GetVariable(&obj, a_path.c_str()) && IsClip(obj)) {
			spdlog::info("  {} найден (тип {})", a_path, static_cast<int>(obj.GetType()));
			EnumerateMembers(obj, a_path);
		} else {
			spdlog::info("  {} не найден", a_path);
		}
	}

	void DumpEntry(const EntryPtr& a_entry, RE::GFxMovieView* a_movie)
	{
		spdlog::info("[{}] элемент не найден. Диагностика меню '{}':", a_entry->name, a_entry->GetMenu());
		DumpPath(a_movie, "_root");

		// Проверяем цепочку пути по частям: видно, на каком звене она обрывается.
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
			if (!a_movie->GetVariable(&clip, path.c_str()) || !IsClip(clip)) {
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
