# Contributing

1. Create a `feature/*` or `fix/*` branch.
2. Keep combat, vehicle, map, and economy behavior data-driven.
3. Add tests for protocol, simulation, permissions, and economy changes.
4. Preserve server authority.
5. Never commit secrets or production player data.
6. Open a pull request and pass required checks before merging.

## Unreal Engine 5.5 build rules (Windows runner `paktia-pc`)

These broke the real Windows build before; follow them in every C++ change (Bhaddru included). Unreal can't compile on the Bhaddru VM, so these are only caught on the runner.

- **Automation test flags:** use `EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter`. `EAutomationTestFlags::ApplicationContextMask` does not exist in UE 5.5 (C2838/C2065). [Automation tests help](https://dev.epicgames.com/documentation/en-us/unreal-engine/write-cplusplus-tests-in-unreal-engine)
- **No shadowing:** never name a parameter or local after a base-class member (`Controller`, `Owner`, `Slot`, `Instigator`, `PlayerState`, …). UnrealHeaderTool rejects it for UFUNCTIONs and MSVC treats it as an error (C4458). Use `InController`, `HitOwner`, `BoxSlot`, etc.
- **Unity build:** helpers in an anonymous namespace must have unique names across the module (e.g. `FindTankerMission`, not `FindMission` in two files); unity builds merge .cpp files (C2084).
- **Every declared function needs a definition** (LNK2019), including UFUNCTIONs bound to input.
- **Engine config:** keep the `WaterBodyCollision` profile in `game/Config/DefaultEngine.ini`; levels and assets are Git LFS files.
