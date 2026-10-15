# Security Policy

Never commit production credentials, Cloudflare API tokens, player records, payment secrets, LLM keys, private signing certificates, anti-cheat private configuration, or production database exports.

Authoritative game-state requirements:

- Clients cannot determine damage, health, ammunition, rewards, tokens, inventory, winner, or rank.
- Match servers validate movement, fire rate, objective state, and vehicle state.
- Economy writes require an idempotency key and audit reference.
- Administrative economy adjustments require elevated permission and a reason.
