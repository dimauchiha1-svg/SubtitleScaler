# SubtitleScaler: an SKSE mod that enlarges subtitles

An SKSE plugin for Skyrim Special Edition and Anniversary Edition (a single DLL works on SE 1.5.97 and all AE 1.6.x versions through Address Library).

It enlarges subtitles and response options in dialogues. Each element is configured separately: its own size, offset and on/off switch.

| INI section | What it enlarges | Default |
|---|---|---|
| `[HUD]` | Subtitles when an NPC talks to another NPC or addresses the player without a dialogue window (greetings, guard shouts) | enabled, 150% |
| `[Dialogue]` | The NPC's line in the dialogue window with the player | enabled, 150% |
| `[PlayerChoices]` | The player's response options in the dialogue window | disabled |
| `[HUDName]` | The speaker name in world subtitles, with its own scale and color | disabled |
| `[DialogueName]` | The speaker name in the dialogue window, with its own scale, offset and color | disabled |

> VR is not included in this build: it is built on `commonlibsse-ng-flatrim` (SE + AE).

## Requirements

- Skyrim SE or AE
- [SKSE64](https://skse.silverlock.org/) for your game version
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) (optional, only needed for the in-game settings menu)

## How to get the DLL (cloud build, nothing to install)

1. Sign up at [github.com](https://github.com) (free).
2. Create a new repository (the **New** button) and name it, for example, `SubtitleScaler`.
3. Upload **all files from this archive, including folders**, and make sure the hidden `.github` folder is included. The easiest way is to drag the contents of the unpacked folder into the browser window on the repository page (**Add file → Upload files**). If `.github` did not upload, create the file via **Add file → Create new file**, type `.github/workflows/build.yml` as the name and paste in the contents of that file.
4. Open the **Actions** tab. The build starts automatically after the files are uploaded. If it does not, choose **Build SubtitleScaler → Run workflow**.
5. Wait 10–20 minutes (the first time, vcpkg builds the libraries). When the green check mark appears, open the run and download `SubtitleScaler` from the **Artifacts** section at the bottom.
6. The archive contains a ready-made `Data` folder structure.

If the build ends with a red cross, open the step that failed, copy the error text and send it to the author so the code can be fixed. The project was written without the ability to compile it locally, so the first build may need fixes.

## Installation

Copy the contents of the `Data` folder from the artifact into the game's `Data` folder (or install it as an archive with a mod manager):

```
Data/SKSE/Plugins/SubtitleScaler.dll
Data/SKSE/Plugins/SubtitleScaler_Default.ini
```

On the first start the plugin creates `SubtitleScaler.ini` from `SubtitleScaler_Default.ini`. The archive does not contain `SubtitleScaler.ini` itself, so updating the mod never overwrites your settings. If you want to start over, delete `SubtitleScaler.ini`. New options added in later versions simply use their default values until you save the settings once.

Do not forget to enable subtitles in the game settings (**Settings → Display → Subtitles**).

## Settings menu (SKSE Menu Framework)

If SKSE Menu Framework is installed, a **SubtitleScaler → Subtitles** section appears in its Mod Control Panel. For each of the targets (`HUD`, `Dialogue`, `PlayerChoices`, `HUDName`, `DialogueName`) it has:

- an **Enabled** checkbox;
- a **Scale (%)** slider (50–400);
- **Offset X / Y** fields;
- a **Max width** slider;
- a **Keep centered when enlarged** checkbox (the `Anchor` setting);
- a **Custom text color** checkbox with a color picker;
- a **Text shadow** checkbox with a strength slider (`HUD`, `Dialogue` and `PlayerChoices`);
- an **Original size** button (returns to 100% and zero offset).

At the top of the page there is a **Write log file** checkbox. Changes are applied immediately. At the bottom there are two buttons: **Save to INI** (writes the values to `SubtitleScaler.ini`, comments in the file are preserved) and **Reload from INI** (re-reads the file). If the framework is not installed, the plugin works as before and can be configured through the INI.

The menu is designed for SKSE Menu Framework 3. During the build, the workflow downloads only the `SKSEMenuFramework.h` header from the [API repository](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API): the plugin needs neither ImGui nor `imgui.dll`, since the functions are called through the framework itself. Before registering the page, the plugin checks that all required functions exist in the installed framework version. If they do not, the menu is disabled and the reason is written to the log. If the header could not be downloaded, the build still succeeds, but without the menu. To build without the menu on purpose, untick `menu` when running the workflow manually.

## Configuration (SubtitleScaler.ini)

Each `[Name]` section describes one interface element. The parameters are the same for all sections:

| Parameter | Meaning |
|---|---|
| `Enabled` | `1` enabled, `0` disabled |
| `Scale` | Scale in percent, 50–400. `100` = original, `150` = one and a half times larger |
| `OffsetX`, `OffsetY` | Offset. If the enlarged element goes off the edge of the screen, adjust the value (a negative `OffsetY` moves the element up) |
| `MaxWidth` | Maximum visible width of the element (the Skyrim stage is 1280 wide). If the enlarged text is wider, long lines wrap onto new lines instead of running off the screen edge. `0` = no limit. Default is `1100` for subtitles |
| `Color` | Text color as `RRGGBB` (for example `FFFF00` = yellow). Empty = keep the original color. For `[PlayerChoices]` the option under the cursor keeps the game's highlight color |
| `Shadow`, `ShadowStrength` | `[HUD]`, `[Dialogue]`, `[PlayerChoices]` only. `Shadow=1` draws a drop shadow under the text; `ShadowStrength` is its intensity, 0–100 (default 60). The shadow replaces any shadow the game already draws for that text. `Shadow=0` keeps the game's own look |
| `Anchor` | `left` or `center`. When enlarged, the element either grows to the right from its left edge (`left`) or stays in place, centered (`center`). Default for `[Dialogue]` is `center` |
| `Menu` | Internal name of the menu that contains the element |
| `Paths` | Path to the element inside the menu (several separated by `|`, the first one found is used) |

To enlarge the player's response options, set `Enabled=1` in the `[PlayerChoices]` section and choose a `Scale`. The shadow and color only apply while the section is enabled; use `Scale=100` if you do not want to change the size.

The `[General]` section has one option, `Logging` (`1` = write `SubtitleScaler.log`, `0` = write nothing).

## Papyrus

The `SubtitleScaler.psc` script is in `Data/Scripts/Source`. It must be compiled with the Papyrus compiler (Creation Kit or Caprica) to get a `.pex`. The `asName` parameter is the INI section name. Example:

```papyrus
SubtitleScaler.SetScale(200.0)                          ; all targets twice as large
SubtitleScaler.SetTargetScale("Dialogue", 170.0)        ; dialogue window lines only
SubtitleScaler.SetEnabled("PlayerChoices", true)        ; enable response options
SubtitleScaler.SetOffset("HUD", 0.0, -40.0)             ; move subtitles up
SubtitleScaler.SetTargetColor("HUD", 16776960)          ; yellow subtitles (0xFFFF00 as a decimal number)
SubtitleScaler.SetTargetColor("HUD", -1)                ; back to the original color
SubtitleScaler.SetTargetShadow("HUD", true, 80)         ; strong drop shadow on world subtitles
SubtitleScaler.SetLogging(false)                        ; stop writing to the log
Float s = SubtitleScaler.GetScale("Dialogue")
SubtitleScaler.ReloadConfig()                           ; re-read the INI
```

Values set through Papyrus last until the game is closed. To make them persist, call the functions in `OnPlayerLoadGame` (for example, from an MCM) or edit the INI.

## Verification and diagnostics

Plugin log: `Documents\My Games\Skyrim Special Edition\SKSE\SubtitleScaler.log` (for GOG: `Skyrim Special Edition GOG`).

- The line `SubtitleScaler menu registered in SKSE Menu Framework` means the menu is connected. The line `Plugin was built without SKSE Menu Framework support` means the DLL was built without the menu.
- A line like `[Name] scale ...% applied` means the section works. For dialogue window elements the line appears after the window is opened for the first time.
- A line like `[Name] element not found` means your interface uses a different menu name or path. Run the build manually (**Actions → Run workflow**) with the `dump` box ticked: the menu structure will be written to the log. Find the element you need there and put its path in the `Paths` of that section.
- Lines like `text field for width limiting: ...` and `field '...': width ...` show how `MaxWidth` was applied.

## Speaker names

`[HUDName]` and `[DialogueName]` are separate targets for the speaker name, so the name can have its own color and size. They are disabled by default.

- `[DialogueName]` is a separate element of the dialogue window, so it supports `Scale`, `Color`, `OffsetX` and `OffsetY`.
- `[HUDName]` is part of the subtitle text in the world (the game draws "Name: line" as one text with separate font spans), so only `Scale` and `Color` work for it. The position of the name cannot be changed separately; use `[HUD]` to move the whole subtitle. Because the game rewrites the text on every new subtitle, the format is re-applied every frame while a subtitle is shown.

## Shadow

The shadow is a Flash drop-shadow filter put on the subtitle's text field. If the interface does not support filters, the log shows `could not create the filter object`. When the shadow is switched off, the original filters of the text field are restored.

## Limitations

- The element paths (`HUDMovieBaseInstance.SubtitleTextHolder`, `DialogueMenu_mc.SubtitleText`, `DialogueMenu_mc.TopicListHolder`) are based on the standard Skyrim interface and may differ if interface replacers are installed. They can be fixed through `Paths`.
- Enlarging is done by scaling the element, so the text may become less sharp and the element may shift. `OffsetX` and `OffsetY` are there for that.

## Project structure

```
src/main.cpp        entry point, log, SKSE events
src/Settings.*      INI reading and writing
src/Subtitles.*     applying the scale to interface elements through Scaleform
src/Papyrus.*       native functions for Papyrus
src/UI.*            menu for SKSE Menu Framework
CMakeLists.txt, vcpkg*.json   build (CommonLibSSE NG via vcpkg)
.github/workflows/build.yml   build on GitHub Actions
```
