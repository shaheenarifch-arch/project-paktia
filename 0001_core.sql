PRAGMA foreign_keys = ON;

CREATE TABLE players (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL UNIQUE,
  display_name TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'active'
    CHECK (status IN ('active', 'suspended', 'banned', 'deleted')),
  level INTEGER NOT NULL DEFAULT 1,
  experience INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at TEXT NOT NULL DEFAULT (datetime('now')),
  version INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE matches (
  id TEXT PRIMARY KEY,
  game_mode TEXT NOT NULL,
  game_version TEXT NOT NULL,
  rules_version TEXT NOT NULL,
  map_code TEXT NOT NULL,
  region TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'waiting'
    CHECK (status IN ('waiting','starting','active','completed','cancelled','failed')),
  authoritative_server_id TEXT,
  match_seed TEXT NOT NULL,
  started_at TEXT,
  completed_at TEXT,
  result_json TEXT,
  created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE match_players (
  match_id TEXT NOT NULL,
  player_id TEXT NOT NULL,
  team_code TEXT NOT NULL,
  score INTEGER NOT NULL DEFAULT 0,
  placement INTEGER,
  result_json TEXT NOT NULL DEFAULT '{}',
  joined_at TEXT NOT NULL DEFAULT (datetime('now')),
  left_at TEXT,
  PRIMARY KEY (match_id, player_id),
  FOREIGN KEY (match_id) REFERENCES matches(id),
  FOREIGN KEY (player_id) REFERENCES players(id)
);
