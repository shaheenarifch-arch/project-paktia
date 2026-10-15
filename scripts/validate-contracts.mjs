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

console.log("contracts valid");
