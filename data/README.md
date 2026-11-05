# Game data

Versioned JSON game definitions. `npm run validate` (see `scripts/validate-data.mjs`) checks every file here and prints errors as `file: field: message`.

All ratings are **game-balance values** on a 1–10 scale (armor 0–10). They are never real performance figures. Real-world-hypothetical content declares `"setting": "real_world_hypothetical"` with `"fictional": false`; fictional content uses `"fictional": true`. The two must agree, and ISIS is never `playable`. There are no real people and no real operating, targeting or building instructions anywhere in this data.

## Folders

| Folder | Contents |
| --- | --- |
| `maps/`, `missions/`, `weapons/`, `vehicles/`, `aircraft/`, `economy/` | Existing campaign and slice data |
| `factions/` | One file per faction: `id`, `name`, `setting`, `fictional`, `playable`, one-line `doctrine`, `strengths[]`, `weaknesses[]`, `aiProfile` (string ID for later AI), optional `backedBy` (a faction id, e.g. Houthi → `iran`). File name is `<id>.json`. |
| `units/` | One file per faction: `{ "faction": "<id>", "units": [...] }`. Unit schema below. |
| `formations/` | `{ "formations": [{ id, faction, name, units: { <unitId>: count } }] }`. Every unit must exist and belong to the formation's faction. |
| `logistics/supply-classes.json` | Supply classes (`fuel`, `ammunition`, `spares`, `food_water`, `medical`) with `unit` of measure, `items` (industry resource ids) and `consumptionKeys` (the unit fields that consume them). |
| `industry/resources.json` | Raw and refined resources (`id`, `name`, `unit`), including ammunition and spare parts. |
| `industry/components.json` | Buildable components with `inputs` (resource ids → amount). Units list the components they are made of. |
| `industry/sites.json` | Site types (oil field, refinery, steel mill, electronics plant, munitions plant, vehicle plant, aircraft plant, shipyard, repair depot, plus mine and supply depot) with `inputsPerDay` / `outputsPerDay` per game-day. |

## Vehicle stats

`vehicles/*.json` files may carry `stats`: `{ maxSpeed (m/s), acceleration (m/s^2), turnRate (deg/s), climbRate (m/s, flyers) }`. Abstract game-balance values read by the playable vehicle pawns (`game/docs/vehicles.md`), never real performance figures.

## Unit schema

`id`, `name`, `faction`, `branch`, `role`, `echelon`, `crew` (integer ≥ 0), `cost_points`, `build_time_days`,
`components{componentId: count}`, `mobility`, `armor{front,side,top}` (0–10), `firepower{antiInfantry,antiArmor,antiAir}`,
`sensors{optics,thermal,radar}`, `ew{jam,resist}`, `fuel_capacity`, `fuel_burn` (per game-day), `ammo_loadout{ammoId: crates}`,
`maintenance_hours`, `stealth`, `tech_requirements[]`, `era`.

All other ratings (`mobility`, `firepower.*`, `sensors.*`, `ew.*`, `stealth`) are integers 1–10.

Supply keys used by units: `fuel_capacity`/`fuel_burn` → fuel, `ammo_loadout` → ammunition, `maintenance_hours` → spares, `crew` → food/water and medical.

## How to add a unit

1. Pick the faction file in `units/` and append an object using the schema above. Use a unique id prefixed with the faction (`iran_...`) and `"faction"` matching the file.
2. List only component ids that exist in `industry/components.json` (add a component there first if needed, with inputs from `industry/resources.json`).
3. Use only ammunition ids listed under the `ammunition` class in `logistics/supply-classes.json` as `ammo_loadout` keys.
4. Keep ratings as integers in range; use generic class names and game-balance values, never real specifications.
5. Optionally add the unit to a template in `formations/`.
6. Run `npm run validate`.

## Final stage data (Episodes 05–08)

`final-stage/hearts-minds.json` and `final-stage/surrender-defection.json` hold the Hearts & Minds and surrender/defection specs. The resistance faction is `factions/iran-resistance.json`; regime security-force units and formations live in extra files named `iran-regime-security.json` under `units/` and `formations/`. Story bible and rules: `docs/final-stage.md`. Help: Epic docs links are at the end of that file, and the epic issue is <https://github.com/shaheenarifch-arch/project-paktia/issues/48>.
