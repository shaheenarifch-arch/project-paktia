# Tactical UI (HUD, minimap, menus, settings)

All UI is built in C++ under `Source/MidnightHavoc/UI/` (no widget assets in the repo, they are binary). A designer can
still make a Widget Blueprint child; see `docs/hud-guide.md`.

Epic references:

- UMG UI Designer: <https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-for-unreal-engine>
- C++ base classes for widgets: <https://dev.epicgames.com/documentation/en-us/unreal-engine/using-cplusplus-with-umg-in-unreal-engine>
- Enhanced Input: <https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine>

## Pieces

| File | What it does |
| --- | --- |
| `HavocPlayHUD` | The HUD controller (extends `UHavocTacticalHUD`). Created by `AHavocPlayerController::ShowHud`. Each tick it reads the mission, flow, pawn and settings and updates the text and bars. |
| `HavocHudModel` | Pure view-model: `HavocHudModel::Build(Mission)` returns objective, reinforcements, terminal integrity, lane hazard counts and tanker counts, plus the tracker and status text. No widgets, so it is covered by an automation test. |
| `HavocMinimapWidget` | Top-down map drawn in `NativePaint`: every `AHavocSectorVolume` as a box (the sector holding the player is highlighted), revealed `AHavocLaneHazard` markers (red, green once cleared) and a player arrow. Scans actors at 10 Hz. |
| `HavocMenuWidget` | Main, campaign, pause, How to play, settings and results menus. |

## What the HUD shows

| Element | Position | Bound to |
| --- | --- | --- |
| Objective tracker (ordered steps, `[x]` done, `[>]` current, `[ ]` to do) | top left | mission state (`HavocHudSnapshot::TrackerText`) |
| Objective line | top centre | `UHavocMissionRunner::GetObjective` |
| Minimap and current sector name | top right | sector volumes, lane hazards, player pawn |
| Reinforcements, terminal integrity, lane progress, tankers through/lost, Falcon status | right | `UHavocOpenStraitMission` (terminal, lanes, escort) |
| Health bar and ammo | bottom left / right | Marine and its weapon component |
| Vehicle HUD: Raven link, signal, battery; altitude and speed for Raven and Falcon | right and bottom centre | the possessed `AHavocDrone` / `AHavocFalconHelicopter` |
| Toast, hint line | centre, bottom | `AHavocEpisode04Flow` (hidden when Subtitles is off) |

## Menus and input

Menus work by keyboard and gamepad. When a menu opens, focus goes to its first button.

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Move between buttons | Up / Down arrows (Tab also works) | D-pad or left stick |
| Press a button | Enter or Space | A (bottom face button) |
| Back / resume / cancel a rebind | Esc | B (right face button) |
| Open the pause menu | Esc or P | Start / Menu |

The settings page scrolls, and focus keeps scrolling the selected row into view.

## How to play and first-time hints

- **How to play** is a button on the main and pause menus. It lists every binding for on foot, vehicles, the Raven drone and the
  Falcon helicopter. The keys are read from the live mapping contexts (`UHavocPlayInput::BuildControlRows`), so a rebind in
  Settings shows there at once. Scroll with the mouse wheel, Up / Down, Page Up / Page Down or the D-pad. Esc or B goes back.
- **First-time hints**: the first time the player takes control of the Raven drone, a vehicle (boat, ground vehicle, helicopter,
  recon drone) or the Falcon helicopter, a short hint with the live keys shows near the screen centre. Press H or gamepad X to
  dismiss it; it also fades after 25 s. Each hint shows once and is remembered (`bHintDroneSeen`, `bHintVehicleSeen`,
  `bHintHelicopterSeen` in `UHavocPlaySettings`). "Reset to defaults" makes them show again. The hints ignore the Subtitles setting.
- **Menu selection fix**: buttons were never registered for focus, so keyboard and gamepad selection did nothing. They are now,
  and the buttons have hover / pressed colours so mouse and focus both show the chosen item.
- Gamepad has menu input only. In-game controls are keyboard and mouse.

## Settings

Saved by `UHavocPlaySettings` (config section in `GameUserSettings.ini`). Each setting has a help line under it in the
menu (`UHavocPlaySettings::HelpFor`).

| Setting | Values | Notes |
| --- | --- | --- |
| Graphics quality | Low, Medium, High, Epic | `UGameUserSettings::SetOverallScalabilityLevel`; applies at once |
| Subtitles | on / off | Off hides the toast and hint lines |
| Reduce flashes and camera shake | on / off | Accessibility; see `docs/vfx-plan.md` |
| Look sensitivity | 0.2 to 3.0 | |
| Master volume | 0 to 100% | |
| Key bindings | interact, reload, Raven, Falcon, return | Select a row, press the new key. A key already used by another of these five is swapped. Movement, pause (Esc, P) and beach choice (1, 2) keys are reserved. Applied through `UHavocPlayInput::ApplyBindings`. |
| Reset to defaults | | |

## Automation tests

`MidnightHavoc.Playable.HudFollowsEpisodeRun` walks the whole north-beach run (briefing to debrief) and checks the HUD
view-model at each phase: tracker step, reinforcements, terminal integrity, lane hazards found and cleared, tankers.
`MidnightHavoc.Playable.SettingsHaveHelp` checks each setting has help text. Run them as described in `README.md`
("Running tests"). The widgets themselves (layout, minimap drawing, focus) need the manual pass in
`docs/episode-04-playtest-checklist.md`.
