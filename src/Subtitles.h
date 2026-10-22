#pragma once

#include "Settings.h"

#include <memory>

// Применение масштаба к элементам интерфейса (субтитры, варианты ответа, уведомления и т.д.).
namespace Subtitles
{
	// Ставит применение масштаба одной цели в очередь UI-потока. Можно вызывать из любого потока.
	// a_withRetry = true: если меню ещё не загрузилось, повторять попытки в течение ~10 секунд.
	void Apply(const std::shared_ptr<Settings::Entry>& a_entry, bool a_withRetry);

	// Применить настройки ко всем целям. Ожидание (retry) используется только для целей в HUD,
	// остальные меню применяются при их открытии.
	void ApplyAll(bool a_withRetry);

	// Подписывается на открытие/закрытие меню (нужно вызвать после kDataLoaded).
	void RegisterMenuSink();
}
