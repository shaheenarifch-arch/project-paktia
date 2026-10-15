# Project Paktia Architecture

## Control plane

GitHub owns source, review, CI/CD, versioned game definitions, release tags, and rollback references.

## Edge and operational platform

Cloudflare owns DNS, WAF, public API gateway, R2 assets and replays, queues, background jobs, and administration surfaces.

## Real-time authority

Regional dedicated game servers own player movement, health, damage, ammunition, vehicles, aircraft, objectives, event activation, winner, and canonical match result.

## Persistent truth

The persistent store owns player profile, progression, inventory, entitlements, token ledger, match results, moderation, and audit records.

## AI boundary

LLM APIs may draft briefings, radio lines, side-objective text, localization, support summaries, and moderation summaries. LLM APIs cannot determine combat, rewards, purchases, match result, or sanctions.
