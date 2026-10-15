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

## Safety boundary

All factions, characters, organizations, weapons, and missions are fictional. Project Paktia does not recreate real militant organizations, real commanders, or specific attacks. Game data describes balance and simulation behavior, not real-world weapon operation.

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

- `game/`: Unreal project source and design contracts
- `services/`: account, matchmaking, progression, inventory, and economy services
- `workers/`: Cloudflare gateway and jobs Workers
- `apps/`: admin and launcher applications
- `data/`: versioned game definitions
- `migrations/`: persistent-account and economy schema
- `tests/`: validation, simulation, protocol, and security tests
- `docs/`: architecture and delivery documentation

## First validation

```bash
npm install
npm test
```
