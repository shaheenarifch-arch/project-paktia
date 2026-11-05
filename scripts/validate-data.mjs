import { readdir, readFile } from "node:fs/promises";
import { join } from "node:path";

const errors = [];
const fail = (file, field, message) => errors.push(`${file}: ${field}: ${message}`);

const SETTING_FICTIONAL = "fictional";
const SETTING_REAL = "real_world_hypothetical";
const UNIT_SUPPLY_KEYS = new Set(["fuel_capacity", "fuel_burn", "ammo_loadout", "maintenance_hours", "crew"]);

const json = new Map(); // path -> parsed JSON

async function walk(directory) {
  const entries = await readdir(directory, { withFileTypes: true });

  for (const entry of entries) {
    const path = join(directory, entry.name);

    if (entry.isDirectory()) {
      await walk(path);
    } else if (entry.name.endsWith(".json")) {
      try {
        json.set(path.replaceAll("\\", "/"), JSON.parse(await readFile(path, "utf8")));
        console.log(`valid: ${path}`);
      } catch (error) {
        fail(path, "(file)", `invalid JSON: ${error.message}`);
      }
    }
  }
}

await walk("data");

const filesIn = (folder) => [...json.entries()].filter(([p]) => p.startsWith(`data/${folder}/`));
const isObject = (v) => typeof v === "object" && v !== null && !Array.isArray(v);
const isInt = (v) => Number.isInteger(v);
const isNum = (v) => typeof v === "number" && Number.isFinite(v);
const isStr = (v) => typeof v === "string" && v.length > 0;

// Collects the entries of `key` from one named file in a folder; reports a missing/non-array list.
function collect(folder, key, name) {
  const items = [];
  for (const [file, doc] of filesIn(folder).filter(([p]) => p.endsWith(`/${name}`))) {
    if (!isObject(doc) || !Array.isArray(doc[key])) {
      fail(file, key, "must be an array");
      continue;
    }
    doc[key].forEach((item, i) => {
      if (!isObject(item)) fail(file, `${key}[${i}]`, "must be an object");
      else items.push({ file, at: `${key}[${i}]`, item });
    });
  }
  return items;
}

function indexById(entries, what) {
  const byId = new Map();
  for (const { file, at, item } of entries) {
    if (!isStr(item.id)) {
      fail(file, `${at}.id`, `${what} id must be a non-empty string`);
    } else if (byId.has(item.id)) {
      fail(file, `${at}.id`, `duplicate ${what} id '${item.id}'`);
    } else {
      byId.set(item.id, { file, at, item });
    }
  }
  return byId;
}

// A map of ids to positive numbers whose keys must be in `known`.
function checkAmounts(file, field, value, known, what, { integer = false, allowEmpty = true } = {}) {
  if (!isObject(value)) return fail(file, field, "must be an object");
  if (!allowEmpty && Object.keys(value).length === 0) fail(file, field, "must not be empty");
  for (const [key, n] of Object.entries(value)) {
    if (!known.has(key)) fail(file, `${field}.${key}`, `unknown ${what} '${key}'`);
    if (!(integer ? isInt(n) : isNum(n)) || n <= 0) fail(file, `${field}.${key}`, `must be a positive ${integer ? "integer" : "number"}`);
  }
}

// --- setting / fictional agreement, for every file that declares them ---
for (const [file, doc] of json) {
  if (!isObject(doc) || (doc.setting === undefined && doc.fictional === undefined)) continue;
  const setting = doc.setting ?? SETTING_FICTIONAL;
  if (setting !== SETTING_FICTIONAL && setting !== SETTING_REAL) {
    fail(file, "setting", `must be '${SETTING_FICTIONAL}' or '${SETTING_REAL}', got '${setting}'`);
  } else if (setting === SETTING_REAL && doc.fictional !== false) {
    fail(file, "fictional", `must be false when setting is '${SETTING_REAL}'`);
  } else if (setting === SETTING_FICTIONAL && doc.fictional === false) {
    fail(file, "fictional", `is false but setting is '${SETTING_FICTIONAL}'`);
  }
}

// --- industry ---
const resources = indexById(collect("industry", "resources", "resources.json"), "resource");
const components = indexById(collect("industry", "components", "components.json"), "component");
const sites = indexById(collect("industry", "sites", "sites.json"), "site");
const itemIds = new Set([...resources.keys(), ...components.keys()]);
for (const id of components.keys()) {
  if (resources.has(id)) fail(components.get(id).file, `${components.get(id).at}.id`, `'${id}' is both a resource and a component`);
}
for (const [, { file, at, item }] of resources) {
  if (!isStr(item.name)) fail(file, `${at}.name`, "must be a non-empty string");
  if (!isStr(item.unit)) fail(file, `${at}.unit`, "must be a non-empty string");
}
for (const [, { file, at, item }] of components) {
  if (!isStr(item.name)) fail(file, `${at}.name`, "must be a non-empty string");
  checkAmounts(file, `${at}.inputs`, item.inputs, resources, "resource", { allowEmpty: false });
}
for (const [, { file, at, item }] of sites) {
  if (!isStr(item.name)) fail(file, `${at}.name`, "must be a non-empty string");
  checkAmounts(file, `${at}.inputsPerDay`, item.inputsPerDay, itemIds, "resource or component");
  checkAmounts(file, `${at}.outputsPerDay`, item.outputsPerDay, itemIds, "resource or component", { allowEmpty: false });
}
for (const required of ["oil_field", "refinery", "steel_mill", "electronics_plant", "munitions_plant", "vehicle_plant", "aircraft_plant", "shipyard", "repair_depot"]) {
  if (sites.size > 0 && !sites.has(required)) fail("data/industry/sites.json", "sites", `missing required site type '${required}'`);
}

// --- logistics ---
const supplyClasses = indexById(collect("logistics", "supplyClasses", "supply-classes.json"), "supply class");
const ammoItems = new Set();
for (const [id, { file, at, item }] of supplyClasses) {
  if (!isStr(item.name)) fail(file, `${at}.name`, "must be a non-empty string");
  if (!isStr(item.unit)) fail(file, `${at}.unit`, "must be a non-empty string");
  if (!Array.isArray(item.items) || item.items.length === 0) {
    fail(file, `${at}.items`, "must be a non-empty array of resource ids");
  } else {
    for (const r of item.items) {
      if (!resources.has(r)) fail(file, `${at}.items`, `unknown resource '${r}'`);
      if (id === "ammunition") ammoItems.add(r);
    }
  }
  if (!Array.isArray(item.consumptionKeys) || item.consumptionKeys.length === 0) {
    fail(file, `${at}.consumptionKeys`, "must be a non-empty array");
  } else {
    for (const k of item.consumptionKeys) {
      if (!UNIT_SUPPLY_KEYS.has(k)) fail(file, `${at}.consumptionKeys`, `unknown unit supply key '${k}' (allowed: ${[...UNIT_SUPPLY_KEYS].join(", ")})`);
    }
  }
}
for (const required of ["fuel", "ammunition", "spares", "food_water", "medical"]) {
  if (supplyClasses.size > 0 && !supplyClasses.has(required)) fail("data/logistics/supply-classes.json", "supplyClasses", `missing required supply class '${required}'`);
}
const classesWithKey = (key) => [...supplyClasses.values()].filter(({ item }) => Array.isArray(item.consumptionKeys) && item.consumptionKeys.includes(key));
for (const key of UNIT_SUPPLY_KEYS) {
  if (supplyClasses.size > 0 && classesWithKey(key).length === 0) fail("data/logistics/supply-classes.json", "consumptionKeys", `no supply class consumes unit key '${key}'`);
}

// --- factions ---
const factions = indexById(filesIn("factions").map(([file, doc]) => ({ file, at: "(root)", item: doc })), "faction");
for (const [id, { file, item }] of factions) {
  for (const f of ["name", "doctrine", "aiProfile"]) if (!isStr(item[f])) fail(file, f, "must be a non-empty string");
  if (typeof item.playable !== "boolean") fail(file, "playable", "must be true or false");
  if (typeof item.fictional !== "boolean") fail(file, "fictional", "must be true or false");
  if (!isStr(item.setting)) fail(file, "setting", "must be declared");
  for (const f of ["strengths", "weaknesses"]) {
    if (!Array.isArray(item[f]) || item[f].length === 0 || !item[f].every(isStr)) fail(file, f, "must be a non-empty array of strings");
  }
  if (typeof item.doctrine === "string" && /[\r\n]/.test(item.doctrine)) fail(file, "doctrine", "must be a one-line summary");
  if (id === "isis" && item.playable !== false) fail(file, "playable", "ISIS must never be playable (AI-only)");
  if (item.backedBy !== undefined && !factions.has(item.backedBy)) fail(file, "backedBy", `unknown faction '${item.backedBy}'`);
  if (file.split("/").pop() !== `${id}.json`) fail(file, "id", `file name must be '${id}.json'`);
}
if (factions.size > 0 && factions.has("houthi") && factions.get("houthi").item.backedBy !== "iran") {
  fail(factions.get("houthi").file, "backedBy", "must be 'iran'");
}

// --- units ---
const unitEntries = [];
for (const [file, doc] of filesIn("units")) {
  if (!isObject(doc) || !Array.isArray(doc.units)) { fail(file, "units", "must be an array"); continue; }
  if (!factions.has(doc.faction)) fail(file, "faction", `unknown faction '${doc.faction}'`);
  doc.units.forEach((item, i) => {
    if (!isObject(item)) return fail(file, `units[${i}]`, "must be an object");
    unitEntries.push({ file, at: `units[${i}]`, item, fileFaction: doc.faction });
  });
}
const units = indexById(unitEntries, "unit");
const rating = (file, field, v, min = 1) => {
  if (!isInt(v) || v < min || v > 10) fail(file, field, `must be an integer ${min}-10, got ${JSON.stringify(v)}`);
};
const group = (file, at, item, name, fields, min = 1) => {
  if (!isObject(item[name])) return fail(file, `${at}.${name}`, "must be an object");
  for (const f of fields) rating(file, `${at}.${name}.${f}`, item[name][f], min);
  for (const k of Object.keys(item[name])) if (!fields.includes(k)) fail(file, `${at}.${name}.${k}`, "unexpected field");
};
for (const { file, at, item, fileFaction } of unitEntries) {
  if (!factions.has(item.faction)) fail(file, `${at}.faction`, `unknown faction '${item.faction}' on unit '${item.id}'`);
  else if (item.faction !== fileFaction) fail(file, `${at}.faction`, `unit '${item.id}' is '${item.faction}' but file faction is '${fileFaction}'`);
  for (const f of ["name", "branch", "role", "echelon", "era"]) if (!isStr(item[f])) fail(file, `${at}.${f}`, "must be a non-empty string");
  if (!isInt(item.crew) || item.crew < 0) fail(file, `${at}.crew`, "must be an integer >= 0");
  if (!isInt(item.cost_points) || item.cost_points < 1) fail(file, `${at}.cost_points`, "must be an integer >= 1");
  if (!isInt(item.build_time_days) || item.build_time_days < 1) fail(file, `${at}.build_time_days`, "must be an integer >= 1");
  if (!isInt(item.maintenance_hours) || item.maintenance_hours < 0) fail(file, `${at}.maintenance_hours`, "must be an integer >= 0");
  for (const f of ["fuel_capacity", "fuel_burn"]) if (!isNum(item[f]) || item[f] < 0) fail(file, `${at}.${f}`, "must be a number >= 0");
  rating(file, `${at}.mobility`, item.mobility);
  rating(file, `${at}.stealth`, item.stealth);
  group(file, at, item, "armor", ["front", "side", "top"], 0);
  group(file, at, item, "firepower", ["antiInfantry", "antiArmor", "antiAir"]);
  group(file, at, item, "sensors", ["optics", "thermal", "radar"]);
  group(file, at, item, "ew", ["jam", "resist"]);
  checkAmounts(file, `${at}.components`, item.components, components, "component", { integer: true, allowEmpty: false });
  checkAmounts(file, `${at}.ammo_loadout`, item.ammo_loadout, ammoItems, "ammunition supply key", { integer: true });
  if (isNum(item.fuel_burn) && item.fuel_burn > 0 && !(item.fuel_capacity > 0)) fail(file, `${at}.fuel_capacity`, "must be > 0 when fuel_burn > 0");
  if (!Array.isArray(item.tech_requirements) || !item.tech_requirements.every(isStr)) fail(file, `${at}.tech_requirements`, "must be an array of strings");
}

// --- formations ---
for (const [file, doc] of filesIn("formations")) {
  if (!isObject(doc) || !Array.isArray(doc.formations)) { fail(file, "formations", "must be an array"); continue; }
  const seen = new Set();
  doc.formations.forEach((f, i) => {
    const at = `formations[${i}]`;
    if (!isObject(f)) return fail(file, at, "must be an object");
    if (!isStr(f.id)) fail(file, `${at}.id`, "must be a non-empty string");
    else if (seen.has(f.id)) fail(file, `${at}.id`, `duplicate formation id '${f.id}'`);
    else seen.add(f.id);
    if (!isStr(f.name)) fail(file, `${at}.name`, "must be a non-empty string");
    if (!factions.has(f.faction)) fail(file, `${at}.faction`, `unknown faction '${f.faction}'`);
    checkAmounts(file, `${at}.units`, f.units, units, "unit", { integer: true, allowEmpty: false });
    if (isObject(f.units)) {
      for (const uid of Object.keys(f.units)) {
        const u = units.get(uid);
        if (u && u.item.faction !== f.faction) fail(file, `${at}.units.${uid}`, `unit belongs to '${u.item.faction}', not formation faction '${f.faction}'`);
      }
    }
  });
}

// --- world map (data/world) ---
const TERRAINS = new Set(["desert", "marsh", "plains", "coastal", "island", "mountain", "urban"]);
const POP_BANDS = new Set(["low", "medium", "high", "very_high"]);
const ROUTE_MODES = new Set(["sea", "land", "mixed"]);
for (const [file, doc] of filesIn("world")) {
  if (!isObject(doc)) { fail(file, "(root)", "must be an object"); continue; }
  if (!isStr(doc.id)) fail(file, "id", "must be a non-empty string");
  for (const key of ["provinces", "seaZones", "routes", "enemies"]) {
    if (!Array.isArray(doc[key]) || doc[key].length === 0) fail(file, key, "must be a non-empty array");
  }
  const list = (key) => (Array.isArray(doc[key]) ? doc[key] : []);
  const entries = (key) => list(key).map((item, i) => ({ file, at: `${key}[${i}]`, item })).filter(({ at, item }) => isObject(item) || (fail(file, at, "must be an object"), false));
  const provinces = indexById(entries("provinces"), "province");
  const zones = indexById(entries("seaZones"), "sea zone");
  const routes = indexById(entries("routes"), "route");
  for (const id of provinces.keys()) if (zones.has(id)) fail(file, "provinces", `id '${id}' is used by both a province and a sea zone`);
  const nodes = new Map([...provinces, ...zones]); // provinces and zones share one id space for adjacency

  for (const [id, { at, item }] of provinces) {
    if (!isStr(item.name)) fail(file, `${at}.name`, "must be a non-empty string");
    if (!factions.has(item.owner)) fail(file, `${at}.owner`, `unknown faction '${item.owner}'`);
    if (!TERRAINS.has(item.terrain)) fail(file, `${at}.terrain`, `must be one of ${[...TERRAINS].join(", ")}`);
    if (!isInt(item.infrastructure) || item.infrastructure < 0 || item.infrastructure > 10) fail(file, `${at}.infrastructure`, "must be an integer 0-10");
    if (!isNum(item.oil) || item.oil < 0) fail(file, `${at}.oil`, "must be a number >= 0 (game units per day)");
    if (!POP_BANDS.has(item.population)) fail(file, `${at}.population`, `must be one of ${[...POP_BANDS].join(", ")}`);
    for (const f of ["isPort", "hasAirbase"]) if (typeof item[f] !== "boolean") fail(file, `${at}.${f}`, "must be true or false");
    if (item.oil > 0 && !item.isPort && ![...list("routes")].some((r) => isObject(r) && Array.isArray(r.legs) && r.legs.includes(id))) {
      fail(file, `${at}.oil`, "an inland oil province must be a leg of a route so its oil can reach a market");
    }
  }
  for (const [, { at, item }] of zones) {
    if (!isStr(item.name)) fail(file, `${at}.name`, "must be a non-empty string");
    for (const f of ["chokepoint", "worldMarket"]) if (typeof item[f] !== "boolean") fail(file, `${at}.${f}`, "must be true or false");
  }
  for (const required of ["hormuz", "bab_el_mandeb", "suez"]) {
    if (zones.has(required) && zones.get(required).item.chokepoint !== true) fail(file, "seaZones", `'${required}' must be flagged chokepoint: true`);
    if (!zones.has(required)) fail(file, "seaZones", `missing required chokepoint '${required}'`);
  }
  if (![...zones.values()].some(({ item }) => item.worldMarket === true)) fail(file, "seaZones", "at least one zone must be a worldMarket");

  // adjacency: every referenced id exists, and is symmetric
  for (const [id, { at, item }] of nodes) {
    if (!Array.isArray(item.adjacent) || !item.adjacent.every(isStr)) { fail(file, `${at}.adjacent`, "must be an array of ids"); continue; }
    if (new Set(item.adjacent).size !== item.adjacent.length) fail(file, `${at}.adjacent`, "contains duplicates");
    for (const other of item.adjacent) {
      if (other === id) fail(file, `${at}.adjacent`, `'${id}' is adjacent to itself`);
      else if (!nodes.has(other)) fail(file, `${at}.adjacent`, `unknown id '${other}'`);
      else if (!nodes.get(other).item.adjacent?.includes?.(id)) fail(file, `${at}.adjacent`, `'${id}' lists '${other}' but '${other}' does not list '${id}' (adjacency must be symmetric)`);
    }
  }
  const adjacent = (a, b) => nodes.get(a)?.item.adjacent?.includes?.(b) === true;

  for (const pair of list("enemies")) {
    if (!Array.isArray(pair) || pair.length !== 2 || !pair.every((f) => factions.has(f)) || pair[0] === pair[1]) fail(file, "enemies", `each entry must be a pair of two different known factions, got ${JSON.stringify(pair)}`);
  }

  // routes: owner known, legs exist and are consecutive neighbours, exporters sit on the route
  for (const [, { at, item }] of routes) {
    if (!isStr(item.name)) fail(file, `${at}.name`, "must be a non-empty string");
    if (!factions.has(item.owner)) fail(file, `${at}.owner`, `unknown faction '${item.owner}'`);
    if (!ROUTE_MODES.has(item.mode)) fail(file, `${at}.mode`, `must be one of ${[...ROUTE_MODES].join(", ")}`);
    if (!Array.isArray(item.legs) || item.legs.length < 2 || !item.legs.every(isStr)) { fail(file, `${at}.legs`, "must be an array of at least two ids"); continue; }
    item.legs.forEach((leg, i) => {
      if (!nodes.has(leg)) return fail(file, `${at}.legs[${i}]`, `unknown id '${leg}'`);
      if (i > 0 && nodes.has(item.legs[i - 1]) && !adjacent(item.legs[i - 1], leg)) fail(file, `${at}.legs[${i}]`, `'${item.legs[i - 1]}' and '${leg}' are not adjacent`);
    });
    const hasZone = item.legs.some((l) => zones.has(l));
    const hasProvince = item.legs.some((l) => provinces.has(l));
    if (item.mode === "land" && hasZone) fail(file, `${at}.mode`, "a land route must not contain sea zones");
    if (item.mode === "sea" && hasProvince && item.legs.some((l) => provinces.has(l))) fail(file, `${at}.mode`, "a sea route must list only sea zones (use exporters for ports)");
    if (item.mode === "mixed" && !(hasZone && hasProvince)) fail(file, `${at}.mode`, "a mixed route needs both provinces and sea zones");
    if (!Array.isArray(item.exporters) || !item.exporters.every(isStr)) { fail(file, `${at}.exporters`, "must be an array of province ids"); continue; }
    for (const ex of item.exporters) {
      if (!provinces.has(ex)) fail(file, `${at}.exporters`, `unknown province '${ex}'`);
      else if (!item.legs.includes(ex) && !item.legs.some((l) => adjacent(ex, l))) fail(file, `${at}.exporters`, `province '${ex}' is not on or next to the route`);
    }
  }
}

// --- ai ---
// data/ai/squad-behaviour.json: tuning for AI squads (game/docs/ai.md). Team comes from faction: coalition is allied, so ISIS can never be.
for (const [file, doc] of filesIn("ai")) {
  if (!isObject(doc) || !isObject(doc.tuning)) { fail(file, "tuning", "must be an object"); continue; }
  for (const k of ["squadTickSeconds", "loseThreatSeconds", "disengageSeconds", "regroupSeconds", "maxFallbackSeconds", "patrolRadiusCm", "coverSearchRadiusCm", "coverGridSpacingCm"]) {
    if (!isNum(doc.tuning[k]) || doc.tuning[k] <= 0) fail(file, `tuning.${k}`, "must be a number > 0");
  }
  if (!isNum(doc.tuning.fallbackStrength) || doc.tuning.fallbackStrength < 0 || doc.tuning.fallbackStrength > 1) fail(file, "tuning.fallbackStrength", "must be a number from 0 to 1");
  if (isNum(doc.tuning.maxFallbackSeconds) && isNum(doc.tuning.regroupSeconds) && doc.tuning.maxFallbackSeconds < doc.tuning.regroupSeconds) fail(file, "tuning.maxFallbackSeconds", "must be >= regroupSeconds");
  if (typeof doc.tuning.useEqsCover !== "boolean") fail(file, "tuning.useEqsCover", "must be true or false");
  const kharg = json.get("data/maps/kharg-hormuz.json");
  const sectorIds = new Set((kharg?.sectors ?? []).map((s) => s.id));
  const missionStates = new Set((json.get("data/missions/episode-04-open-strait.json")?.states ?? []).map((s) => s.id));
  if (!Array.isArray(doc.phases)) { fail(file, "phases", "must be an array"); continue; }
  doc.phases.forEach((phase, i) => {
    const at = `phases[${i}]`;
    if (!missionStates.has(phase.state)) fail(file, `${at}.state`, `unknown Episode 04 state '${phase.state}'`);
    if (!Array.isArray(phase.deployments)) return fail(file, `${at}.deployments`, "must be an array");
    phase.deployments.forEach((d, j) => {
      const dat = `${at}.deployments[${j}]`;
      if (!factions.has(d.faction)) fail(file, `${dat}.faction`, `unknown faction '${d.faction}'`);
      if (!sectorIds.has(d.sector)) fail(file, `${dat}.sector`, `unknown sector '${d.sector}'`);
      if (!isInt(d.size) || d.size < 1 || d.size > 12) fail(file, `${dat}.size`, "must be an integer from 1 to 12");
      if (!["patrol", "assault"].includes(d.posture)) fail(file, `${dat}.posture`, "must be 'patrol' or 'assault'");
      if (d.team !== undefined) fail(file, `${dat}.team`, "not allowed: the team comes from the faction (coalition is allied, all others opposing)");
    });
  });
}

if (errors.length > 0) {
  console.error(`\n${errors.length} data validation error(s):`);
  for (const e of errors) console.error(`  - ${e}`);
  process.exit(1);
}
console.log("data validation passed");
