import { readFile } from "node:fs/promises";

const map = JSON.parse(await readFile("data/maps/gardun-city.json", "utf8"));
const mission = JSON.parse(await readFile("data/missions/operation-broken-citadel.json", "utf8"));
const weapons = JSON.parse(await readFile("data/weapons/kr47.json", "utf8"));

if (!map.fictional) throw new Error("Gardun must remain fictional");
if (map.sectors.length !== 10) throw new Error("Gardun sector contract changed unexpectedly");
if (mission.phases.length !== 5) throw new Error("Stronghold Assault requires five phases");
if (!weapons.fictional) throw new Error("Weapon definitions must remain fictional");

console.log("contracts valid");
