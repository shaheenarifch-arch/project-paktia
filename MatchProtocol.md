# Match Protocol

## Client input

```json
{
  "type": "player.input",
  "sequence": 1452,
  "frame": 8124,
  "input": {
    "moveX": 1,
    "moveY": 0,
    "fire": false,
    "interact": true
  }
}
```

## Server snapshot

```json
{
  "type": "state.snapshot",
  "frame": 8124,
  "rulesVersion": "0.1.0",
  "players": [],
  "vehicles": [],
  "objectives": []
}
```

The client may predict presentation. The dedicated server remains authoritative.
