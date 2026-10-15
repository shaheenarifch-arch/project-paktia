# Project Paktia: Drone and Aegis Support Vertical Slice

Engine-agnostic TypeScript domain layer for Mission 01: Operation Mountain Shield.
It models fictional drone control, reconnaissance contacts, tactical-map markers,
protected zones, support authorization, mission progression, and telemetry.

## Run

```bash
npm install
npm run typecheck
npm test
```

## Integration

Connect your game engine adapters to `DroneController`, `ContactManager`,
`TacticalMap`, `CommandManager`, and `MountainShieldMission`. Rendering, physics,
network replication, audio, and platform input remain engine responsibilities.

The Aegis feature is deliberately an abstract game-support workflow. It contains
no real-world targeting, guidance, propulsion, or weapons engineering logic.
