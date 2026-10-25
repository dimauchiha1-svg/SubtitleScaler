Scriptname SubtitleScaler Hidden
{Native functions of the SubtitleScaler.dll plugin. All functions are global: SubtitleScaler.SetScale(200.0)}

; asName - section name from SubtitleScaler.ini:
;   "HUD"           NPC subtitles in the world (NPC to NPC, NPC to player outside the dialogue window)
;   "Dialogue"      NPC lines in the dialogue window
;   "PlayerChoices" the player's response options

; Current scale of the target in percent (0.0 if there is no such section).
Float Function GetScale(String asName) global native

; Set the scale in percent (50..400) for ALL targets at once.
Function SetScale(Float afPercent) global native

; Set the scale for one target only.
Function SetTargetScale(String asName, Float afPercent) global native

; Shift along the X and Y axes (Flash stage units) relative to the original position.
Function SetOffset(String asName, Float afX, Float afY) global native

; Enable or disable a target. A disabled target returns to its original look.
Function SetEnabled(String asName, Bool abEnabled) global native

; Whether the target is enabled.
Bool Function IsEnabled(String asName) global native

; Sets the text color of a target as 0xRRGGBB written as a decimal number (for example 16776960 = yellow, 16777215 = white).
; A negative value restores the original color. Works for "HUD" and "Dialogue".
Function SetTargetColor(String asName, Int aiRGB) global native

; Returns the custom text color of a target as 0xRRGGBB, or -1 if the original color is used.
Int Function GetTargetColor(String asName) global native

; Apply the current settings again (for example, after loading a save).
Function Reapply() global native

; Re-read SubtitleScaler.ini and apply it.
Function ReloadConfig() global native
