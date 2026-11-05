# Playable vehicles (Episode 04)

Four placeholder vehicles you can get into and drive or fly in `L_KhargHormuz`: a boat, a ground vehicle,
a helicopter and a recon drone. They are fictional, abstract and unarmed. Episode 04 stays
`real_world_hypothetical`: no real equipment, specs or targeting data; stats are game-balance numbers.
The mission pawns (`AHavocFalconHelicopter`, `AHavocRavenDrone`) are separate and unchanged.

## How to play

| Key | Action |
| --- | --- |
| F | On foot within ~4.5 m of a vehicle: get in. In a vehicle: get out (you appear beside it). |
| W / S | Throttle forward / reverse |
| D / A | Steer (helicopter and drone: yaw) |
| Space / Left Ctrl | Up / down (helicopter and drone) |
| Mouse | Orbit the camera |

Only the on-foot Marine can get in (not while a menu is open); the Marine is hidden while you drive.

## Where things are

| Vehicle | Class | Data (`data/vehicles/`) | Asset role (`Config/HavocAssets.ini`) |
| --- | --- | --- | --- |
| Boat | `AHavocBoatVehicle` | `tern-patrol-boat.json` | `Vessel.SmallBoat` |
| Ground vehicle | `AHavocGroundVehicle` | `warden-tank.json` | `Vehicle.Tank.Coalition` |
| Helicopter | `AHavocHelicopterVehicle` | `kestrel-scout-helicopter.json` | `Aircraft.Falcon` |
| Recon drone | `AHavocDroneVehicle` | `wisp-recon-drone.json` | `Drone.Raven` |

Code is in `Source/MidnightHavoc/Vehicles/`: `HavocVehicleBase` (components, data, camera, enter/exit,
Enhanced Input), `HavocSurfaceVehicles` (ground, boat) and `HavocFlyingVehicles` (helicopter, drone).
Input actions and the `IMC_Vehicle` context are built in C++ by `UHavocPlayInput`; `F` is a Meta action
handled by `AHavocPlayerController` (server RPC `ServerToggleVehicle`).

## Data

Each vehicle file has a `stats` object read through `UHavocDataSubsystem` (`FHavocVehicleDef::Stats`):

```json
"stats": { "maxSpeed": 20, "acceleration": 6, "turnRate": 40, "climbRate": 0 }
```

`maxSpeed` m/s, `acceleration` m/s^2, `turnRate` degrees/s at full steer, `climbRate` m/s (flyers only, optional).
Edit the JSON and restart; no C++ change is needed. Seats come from the same file.

## Art

Meshes come from the asset roles above via `UHavocAssetSubsystem`. A role whose asset is not imported yet
is logged (`LogHavocAssets`) and the vehicle is invisible but still drivable. Static and skeletal meshes both work.

## Level placement

`Content/Python/build_kharg_level.py` spawns one of each (`VEHICLES` table): the boat just off the north beach,
the ground vehicle, helicopter and drone on the beach. Re-running the script replaces them (tagged `HavocKhargBuilder`).
Or drag the classes into the level by hand. The boat holds the Z it is placed at, so place it on the water level (0).

## Movement model

A simple arcade model: a swept kinematic step on the server (blocked by geometry, ground vehicle follows the terrain,
boat rides at its spawn Z, flyers climb with lift). It is not Chaos physics. To move the ground vehicle or boat to Chaos
Vehicles later, subclass `AWheeledVehiclePawn` and keep `AHavocVehicleBase`'s data and enter/exit logic.

## Tests

`MidnightHavoc.Vehicles.*` (Data, Math, Spawn, EnterExit, Move) in `Tests/HavocVehicleTests.cpp`. See `README.md`, "Running tests".

## Manual check (in the editor)

1. Build the level (`build_kharg_level.py`) and press Play.
2. Walk to each vehicle, press F: the camera switches and the keys above work; press F to get out.
3. The boat stays on the water, the ground vehicle follows the beach, the helicopter and drone climb and stop at the ground.

## Epic help

- Chaos Vehicles overview: <https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-vehicles-overview-in-unreal-engine>
- Enhanced Input: <https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine>
- Possession and pawns: <https://dev.epicgames.com/documentation/en-us/unreal-engine/controlling-pawns-in-unreal-engine>
- Automation tests: <https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine>
