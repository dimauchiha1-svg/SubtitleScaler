Scriptname SubtitleScaler Hidden
{Нативные функции плагина SubtitleScaler.dll. Все функции глобальные: SubtitleScaler.SetScale(200.0)}

; asName - имя секции из SubtitleScaler.ini:
;   "HUD"           субтитры NPC в мире (NPC с NPC, NPC к игроку без окна диалога)
;   "Dialogue"      субтитры NPC в окне диалога
;   "PlayerChoices" варианты ответа игрока

; Текущий масштаб цели в процентах (0.0, если такой секции нет).
Float Function GetScale(String asName) global native

; Установить масштаб в процентах (50..400) сразу для ВСЕХ целей.
Function SetScale(Float afPercent) global native

; Установить масштаб только для одной цели.
Function SetTargetScale(String asName, Float afPercent) global native

; Сдвиг по осям X и Y (единицы Flash-сцены) относительно исходной позиции.
Function SetOffset(String asName, Float afX, Float afY) global native

; Включить или выключить цель. Выключенная цель возвращается к исходному виду.
Function SetEnabled(String asName, Bool abEnabled) global native

; Включена ли цель.
Bool Function IsEnabled(String asName) global native

; Применить текущие настройки ещё раз (например, после загрузки сохранения).
Function Reapply() global native

; Перечитать SubtitleScaler.ini и применить.
Function ReloadConfig() global native
