import { readFile } from "node:fs/promises";

const map = JSON.parse(
  await readFile("data/maps/gardun-city.json", "utf8")
);

const mission = JSON.parse(
  await readFile(
    "data/missions/operation-broken-citadel.json",
    "utf8"
  )
);

const weapon = JSON.parse(
  await readFile("data/weapons/kr47.json", "utf8")
);

if (!map.fictional) {
  throw new Error("Gardun must remain fictional");
}

if (map.sectors.length !== 10) {
  throw new Error("Gardun must contain 10 defined sectors");
}

if (mission.phases.length !== 5) {
  throw new Error("Stronghold Assault must contain 5 phases");
}

if (!weapon.fictional) {
  throw new Error("Weapon definitions must remain fictional");
}

// Campaign episodes: fictional, on known Gardun sectors, and internally consistent.
const sectorIds = new Set(map.sectors.map(sector => sector.id));
const strongholdObjectives = new Set(mission.phases.flatMap(phase => phase.objectives));
const { readdir } = await import("node:fs/promises");
for (const file of (await readdir("data/missions")).filter(name => /^episode-\d+.*\.json$/.test(name))) {
  const episode = JSON.parse(await readFile(`data/missions/${file}`, "utf8"));
  if (episode.fictional !== true) throw new Error(`${file}: episodes must remain fictional`);
  for (const sector of episode.sectors ?? []) {
    if (!sectorIds.has(sector)) throw new Error(`${file}: unknown sector ${sector}`);
  }
  if (episode.states) {
    const ids = new Set(episode.states.map(state => state.id));
    for (const state of episode.states) {
      for (const target of [state.next, ...Object.values(state.branches ?? {})].filter(Boolean)) {
        if (!ids.has(target)) throw new Error(`${file}: ${state.id} leads to unknown state ${target}`);
      }
    }
    if (!ids.has(episode.start)) throw new Error(`${file}: unknown start state ${episode.start}`);
  }
  if (episode.stronghold) {
    if (episode.stronghold !== mission.id) throw new Error(`${file}: unknown stronghold ${episode.stronghold}`);
    for (const objective of Object.keys(episode.unlocks ?? {})) {
      if (!strongholdObjectives.has(objective)) throw new Error(`${file}: unlock on unknown objective ${objective}`);
    }
  }
  console.log(`episode valid: ${file}`);
}

console.log("contracts valid");
