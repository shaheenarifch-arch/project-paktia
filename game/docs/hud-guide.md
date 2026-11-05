# Tactical HUD: making the UMG Blueprint child

`UHavocTacticalHUD` (`Source/MidnightHavoc/TacticalUI/HavocTacticalHUD.h`) is the C++ base for the
tactical HUD. It listens for mission, drone and helicopter events and calls
`BlueprintImplementableEvent`s that you implement in a Widget Blueprint. There are no widget assets
in the repo (they are binary), so each designer creates the Blueprint child locally.

Epic's UMG documentation, for reference:

- UMG UI Designer overview: <https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-for-unreal-engine>
- Creating widgets: <https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-widgets-in-unreal-engine>
- UMG Designer and Graph tabs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-quick-start-guide>
- Widget Blueprint reference: <https://dev.epicgames.com/documentation/en-us/unreal-engine/widget-blueprints-in-unreal-engine>
- Creating C++ base classes for widgets: <https://dev.epicgames.com/documentation/en-us/unreal-engine/using-cplusplus-with-umg-in-unreal-engine>
- Blueprint event dispatching and implementable events: <https://dev.epicgames.com/documentation/en-us/unreal-engine/events-in-unreal-engine>

## 1. Create the Widget Blueprint

1. Build the `MidnightHavoc` module so `HavocTacticalHUD` is registered (see `game/README.md`).
2. In the Content Browser, right-click > **User Interface > Widget Blueprint**.
3. In the **Pick Parent Class** dialog, open **All Classes**, search `HavocTacticalHUD` and select it.
4. Name it `WBP_TacticalHUD` and put it under `Content/UI/`.

## 2. Lay out the widgets (Designer tab)

Add the controls you want to drive, for example:

| Widget | Name (suggested) | Driven by |
| --- | --- | --- |
| Text Block | `ObjectiveText` | `OnObjectiveChanged` |
| Vertical Box | `ChoiceList` | `OnChoicesAvailable` |
| Text Block | `CheckpointToast` | `OnCheckpoint` |
| Text Block / panel | `OutcomeText` | `OnMissionComplete` |
| Progress Bar | `DroneBatteryBar` | `OnDroneBattery` |
| Progress Bar / icon | `DroneSignalBar` | `OnDroneSignal` |
| Image / text | `HeliDamageIcon` | `OnHelicopterDamage` |
| Text Blocks | `ReinforcementsText`, `TerminalIntegrityBar`, `SupplyFlowText` | the getters below |

Tick **Is Variable** on any widget you want to reach from the Graph.

## 3. Implement the events (Graph tab)

In the Graph tab, right-click the Event Graph and add the events (they appear because the base class
declares them). Each event fires only after the HUD has been constructed.

| Event | Parameters | When it fires |
| --- | --- | --- |
| `OnObjectiveChanged` | `Objective`, `StateId` | `mission:state`, and once on construct. `StateId` is empty on the construct call. |
| `OnChoicesAvailable` | `Choices` (strings) | The new state has named branches. Call `Choose(branch)` on `UHavocMissionSubsystem` when the player picks one. |
| `OnCheckpoint` | `StateId` | `mission:checkpoint` |
| `OnMissionComplete` | `Outcome`, `StateId` | `mission:complete`. `Outcome` is `SUCCESS`, `PARTIAL` or `FAILED`. |
| `OnMissionBlocked` | `StateId`, `Missing` | `mission:blocked`: flags that the state still requires. Not in the minimum list, handy for hints. |
| `OnDroneBattery` | `DroneId`, `Battery` (0..1), `bLow`, `bCritical` | `drone:battery`, when the whole-percent value changes. Low is 0.25 or less, critical 0.1 or less. |
| `OnDroneSignal` | `DroneId`, `Signal` (0..1), `SignalState` | `drone:signal`. `SignalState` is Strong, Stable, Degraded, Critical or Lost. |
| `OnHelicopterDamage` | `Callsign`, `Level`, `Snapshot` | `helicopter:damage`. `Level` is Operational, Damaged, Critical, Disabled or Destroyed. |

Pure getters, safe to bind to a widget property (right-click a property > **Bind**):

| Function | Returns |
| --- | --- |
| `GetReinforcements` | Reinforcements left in Episode 04 (int) |
| `GetTerminalIntegrity` | Oil terminal integrity 0..1 |
| `GetSupplyFlow` | Supply flow from the tanker escort |
| `HasEpisode04Data` | False when Episode 04 is not the active mission; the three getters return 0 then |

You do not call anything to subscribe: `NativeConstruct` subscribes and `NativeDestruct` unsubscribes.
Binding a getter runs every frame, so keep the graph behind it light.

## 4. Show it in game

In your player controller or HUD Blueprint, **Create Widget** (class `WBP_TacticalHUD`) then
**Add to Viewport**. Help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-widgets-in-unreal-engine>.
Create it on the local client only (not on a dedicated server).

## Event payloads (for custom listeners)

All events travel on `UHavocEventSubsystem` as `(FName EventName, FString JsonPayload)`.

| Event | Payload |
| --- | --- |
| `mission:state` | `{ "missionId", "from"?, "to" }` |
| `mission:blocked` | `{ "missionId", "state", "missing": [..] }` |
| `mission:checkpoint` | `{ "missionId", "checkpoint": { "state", "flags", "history", "systems" } }` |
| `mission:complete` | `{ "missionId", "state", "outcome" }` |
| `drone:battery` | `{ "droneId", "battery", "low", "critical" }` |
| `drone:signal` | `{ "droneId", "signal" }` |
| `drone:detected` | `{ "droneId", "hazardId", "classification" }` (not shown by the HUD; lane hazards listen for it) |
| `helicopter:damage` | `{ "aircraft", "level", "flightControl", "engineCondition", "structuralCondition" }` |
| `helicopter:state` | `{ "aircraft", "sequence", "flightState" }` |

`mission:*` payloads match `MissionEvents` in `src/gameplay/missions/framework/MissionRunner.ts`.
The `drone:*` and `helicopter:*` events are new in this port (the TypeScript has no equivalents).

## Built-in playable HUD

`UHavocPlayHUD` (`Source/MidnightHavoc/UI/HavocPlayHUD.h`) extends `UHavocTacticalHUD` and builds a complete HUD in C++:
objective, hint line, toast, health bar, ammo, reinforcements, terminal and tanker status, Raven link and battery, and Falcon
status. `AHavocPlayerController::HudClass` defaults to it, so Episode 04 needs no widget asset. To restyle it, make a Widget
Blueprint child of `HavocPlayHUD` (steps 1 to 3 above) and set it as `HudClass` on a Blueprint child of `HavocPlayerController`.
The same events and getters apply. Help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/using-cplusplus-with-umg-in-unreal-engine>

Objective tracker, minimap, vehicle HUD, settings and menu navigation are described in `docs/ui.md`.
