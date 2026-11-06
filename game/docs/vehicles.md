# Playable vehicles (Episode 04)

Five placeholder vehicles you can get into and drive or fly in `L_KhargHormuz`: a boat, a ground vehicle (Warden tank),
a wheeled truck (Nomad), a helicopter and a recon drone. They are fictional, abstract and unarmed. Episode 04 stays
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
| Ground vehicle (Warden tank, tracked) | `AHavocGroundVehicle` | `warden-tank.json` | `Vehicle.Tank.Coalition` |
| Nomad truck (wheeled) | `AHavocNomadVehicle` | `nomad-transport.json` | `Vehicle.Truck.Coalition` (not mapped yet: greybox) |
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

## Damage and the mission (Q9)

Every vehicle has `Integrity` 0..1 (`GetIntegrity`, `ApplyVehicleDamage(Amount)`, `RepairVehicle(Amount)`), the same scale as
`ConvoyEscort` in `src/gameplay/missions/episode02/ConvoyEscort.ts`. Levels match the helicopter: Operational, Damaged (<= 0.7),
Critical (<= 0.35), Disabled (<= 0.15), Destroyed (0). Damaged vehicles lose speed and turning (x0.8, x0.5); disabled or destroyed
ones coast to a halt. A destroyed vehicle throws the driver out, cannot be entered and cannot be repaired in the field.

Set `ConvoyId` (Warden defaults to `warden-1`, Nomad to `nomad-1`; set `nomad-2` on a second truck). Each change publishes
`convoy:vehicle` on `UHavocEventSubsystem` with `{"id","integrity","level"}`; the Episode 02 convoy system subscribes and
applies it with the same rules as `ConvoyEscort.applyDamage` / `repair`. The Unreal Episode 02 convoy system does not exist yet, so
the event is the hook for it.

## Convoy follow (Episode 02)

On a Warden or Nomad set `bConvoyFollow`, optionally `ConvoyLeader` (default: the nearest Warden) and `FollowGap` (cm). While nobody
drives it, the vehicle follows the leader by straight pursuit on the ground plane and stops inside the gap. Get in (F) to take over.
It does not use the navmesh, so route it along open ground (convoys stay out of tunnels).

## Movement model

A simple arcade model: a swept kinematic step on the server (blocked by geometry, ground vehicle follows the terrain,
boat rides at its spawn Z, flyers climb with lift). It is not Chaos physics yet: Chaos Vehicles needs a rigged skeletal mesh, wheel Blueprints and (for the tracked Warden) a
skid-steer setup authored in the editor, and no free rigged tank/truck is in the project. To move the Warden, Nomad or boat to Chaos
Vehicles once those assets exist, subclass `AWheeledVehiclePawn` and keep `AHavocVehicleBase`'s data and enter/exit logic.

## Tests

`MidnightHavoc.Vehicles.*` (Data, Math, DamageMath, Damage, Spawn, EnterExit, Move) in `Tests/HavocVehicleTests.cpp`. See `README.md`, "Running tests".

## Manual check (in the editor)

1. Build the level (`build_kharg_level.py`) and press Play.
2. Walk to each vehicle (Warden and Nomad are side by side on the beach), press F: the camera switches and the keys above work; press F to get out.
3. The boat stays on the water, the ground vehicle follows the beach, the helicopter and drone climb and stop at the ground.

## Epic help

- Chaos Vehicles overview: <https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-vehicles-overview-in-unreal-engine>
- Enhanced Input: <https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine>
- Possession and pawns: <https://dev.epicgames.com/documentation/en-us/unreal-engine/controlling-pawns-in-unreal-engine>
- Automation tests: <https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine>
- Chaos Vehicles (Epic): <https://dev.epicgames.com/documentation/en-us/unreal-engine/vehicles-in-unreal-engine>
