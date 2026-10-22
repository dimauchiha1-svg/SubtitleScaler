#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Настройки плагина. Значения читаются из Data/SKSE/Plugins/SubtitleScaler.ini
// и могут меняться на лету из Papyrus.
//
// Каждая секция INI описывает одну "цель" (Entry): текстовый элемент внутри меню,
// который нужно увеличить. Целей три: HUD (субтитры NPC в мире), Dialogue (реплика NPC
// в окне диалога) и PlayerChoices (варианты ответа игрока).
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

		const std::string  name;  // имя секции INI
		std::atomic<bool>  enabled;
		std::atomic<float> scale{ 150.0f };  // в процентах, 100 = как в оригинале
		std::atomic<float> offsetX{ 0.0f };  // сдвиг по X в единицах Flash-сцены
		std::atomic<float> offsetY{ 0.0f };  // сдвиг по Y в единицах Flash-сцены

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

		// Состояние, к которому обращается только UI-поток.
		struct UiState
		{
			std::optional<double> baseX;
			std::optional<double> baseY;
			double                lastLoggedScale = -1.0;
			bool                  loggedMissing = false;
		} ui;

	private:
		mutable std::mutex       _lock;
		std::string              _menu;   // внутреннее имя меню, например "HUD Menu"
		std::vector<std::string> _paths;  // пути к клипу/текстовому полю, пробуются по порядку
	};

	// Копия списка целей (безопасно использовать в любом потоке).
	std::vector<std::shared_ptr<Entry>> GetEntries();

	// Поиск цели по имени секции (без учёта регистра). nullptr, если такой нет.
	std::shared_ptr<Entry> Find(std::string_view a_name);

	// Читает INI. Если файла нет, остаются значения по умолчанию.
	void Load();

	// Записывает текущие значения (Enabled, Scale, OffsetX, OffsetY) в INI.
	// Комментарии и остальные строки файла сохраняются. Возвращает false при ошибке записи.
	bool Save();
}
