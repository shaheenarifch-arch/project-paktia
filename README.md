# Project Paktia

Project Paktia is a fictional combined-arms game vertical slice set in the fictional city of Gardun in the Paktia Highlands.

## Vertical-slice target

- One 2.5 km x 2.5 km core combat map
- Stronghold Assault mode
- Two fictional factions
- Infantry combat
- Fictional KR-47 rifle
- Warden armored vehicle
- Nomad light transport
- Falcon helicopter
- Server-controlled surprise events
- Dedicated authoritative match server
- Cosmetic Command Token economy
- Cloudflare API gateway, R2 assets, queues, and admin surface

See `docs/vertical-slice.md`, `docs/gardun-map.md`, `docs/architecture.md`, and `docs/MatchProtocol.md`.

## Safety boundary

All factions, characters, organizations, weapons, and missions are fictional. Project Paktia does not recreate real militant organizations, real commanders, or specific attacks. Game data describes balance and simulation behavior, not real-world weapon operation.

The Aegis feature is deliberately an abstract game-support workflow. It contains no real-world targeting, guidance, propulsion, or weapons engineering logic.

## Architecture

```text
Unreal Engine client
        |
Cloudflare gateway
        |
Match allocator
        |
Regional authoritative server
        |
Match result queue
        |
Progression + inventory + token ledger
```

Cloudflare is used for edge-facing services, account APIs, assets, queues, and administration. Real-time infantry, tank, helicopter, projectile, objective, and match-result authority belongs to a dedicated game server.

## Repository areas

- `src/`: engine-agnostic TypeScript gameplay domain layer (drones, intelligence, Aegis support, tactical command, aircraft, Mission 01: Operation Mountain Shield, telemetry) and its tests
- `ui/`: Vite tactical-command UI prototype
- `game/`: Unreal project source and design contracts (`game/Source/ProjectPaktia/Combat/CombatAuthority.h`)
- `services/`: economy and matchmaking contracts
- `workers/`: Cloudflare gateway and jobs Workers (`wrangler.jsonc`)
- `data/`: versioned game definitions
- `migrations/`: persistent-account and economy schema
- `scripts/`: data and contract validators
- `docs/`: architecture and delivery documentation

## Run

```bash
npm install
npm run typecheck   # gameplay domain + workers/services
npm test            # build + node:test suites in src/tests
npm run validate    # data/ JSON + Gardun/mission/weapon contracts

cd ui
npm install
npm run typecheck
npm run build
npm run dev -- --host 0.0.0.0
```

## Gameplay domain integration

Connect your game engine adapters to `DroneController`, `ContactManager`,
`TacticalMap`, `CommandManager`, `HelicopterCrashController`, and
`MountainShieldMission` (all exported from `src/index.ts`). Rendering, physics,
network replication, audio, and platform input remain engine responsibilities.

Mission 01 includes the Falcon-2 crash recovery sequence:
`MountainShieldMission.continueCrashSequence()` advances from
`M01_COUNTERATTACK` through crash, search, crew recovery, and evacuation to
`M01_FINAL_DRONE_DEFENSE`.

## Hologram avatar overlay (optional)

`ui/src/avatar3d.ts` and `ui/src/avatar.css` provide a three.js hologram avatar
(`three` is already a ui dependency). It is not mounted by default. To use it:

1. In `ui/src/main.ts`, import both files:
   `import { mountHologramAvatar } from './avatar3d'` and `import './avatar.css'`
2. Put this where the circular avatar belongs in the markup:
   `<div class="avatar-orbit"><span class="avatar-dot"></span><div class="avatar-window"><canvas id="commandAvatar3d"></canvas></div></div>`
3. After setting `innerHTML`:
   `const avatarCanvas = document.querySelector<HTMLCanvasElement>('#commandAvatar3d'); if (avatarCanvas) mountHologramAvatar(avatarCanvas)`
