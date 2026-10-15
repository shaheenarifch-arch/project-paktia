CREATE TABLE token_transactions (
  id TEXT PRIMARY KEY,
  player_id TEXT NOT NULL,
  currency_code TEXT NOT NULL
    CHECK (currency_code IN ('CREDITS','COMMAND_TOKENS','PREMIUM_TOKENS')),
  transaction_type TEXT NOT NULL
    CHECK (transaction_type IN ('earn','spend','purchase','refund','adjustment')),
  amount_minor INTEGER NOT NULL,
  reference_type TEXT NOT NULL,
  reference_id TEXT NOT NULL,
  idempotency_key TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT (datetime('now')),
  FOREIGN KEY (player_id) REFERENCES players(id),
  UNIQUE (player_id, idempotency_key)
);

CREATE TABLE player_entitlements (
  id TEXT PRIMARY KEY,
  player_id TEXT NOT NULL,
  catalog_item_code TEXT NOT NULL,
  source_type TEXT NOT NULL,
  source_id TEXT NOT NULL,
  granted_at TEXT NOT NULL DEFAULT (datetime('now')),
  revoked_at TEXT,
  FOREIGN KEY (player_id) REFERENCES players(id),
  UNIQUE (player_id, catalog_item_code, source_id)
);
