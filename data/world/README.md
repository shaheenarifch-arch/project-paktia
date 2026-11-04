# World map (strategic layer)

`gulf-theatre.json` is the strategic map the sim compiles in (`npm run sim:gen`): `provinces[]` (owner, terrain, infrastructure 0-10, oil in game units per day, population band, `isPort`, `hasAirbase`, `adjacent`), `seaZones[]` (`chokepoint`, `worldMarket`, `adjacent`), `routes[]` (`owner`, `mode`, ordered `legs`, `exporters`) and `enemies[]` pairs. Provinces and sea zones share one id space, and adjacency must be symmetric. Game layout, not a survey. `npm run validate` checks adjacency, ids, owners and route legs.
