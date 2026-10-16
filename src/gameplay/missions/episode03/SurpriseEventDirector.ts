// One server-controlled surprise event per match (docs/vertical-slice.md),
// picked deterministically from a seed so a replay or a server restart picks
// the same one. Effects are abstract gameplay modifiers that engine systems
// read; they don't model anything real.
export type SurpriseEvent = 'sandstorm' | 'communications_outage' | 'tunnel_discovery' | 'bridge_obstruction' | 'nightfall'

export interface SurpriseModifiers {
  droneSensorRangeScale: number
  relayInterference: number
  openedRoutes: readonly string[]
  blockedRoutes: readonly string[]
  visibilityScale: number
}

const NONE: SurpriseModifiers = { droneSensorRangeScale: 1, relayInterference: 0, openedRoutes: [], blockedRoutes: [], visibilityScale: 1 }

const EFFECTS: Record<SurpriseEvent, Partial<SurpriseModifiers>> = {
  sandstorm: { droneSensorRangeScale: 0.5, visibilityScale: 0.6 },
  communications_outage: { relayInterference: 0.35 },
  tunnel_discovery: { openedRoutes: ['old_gardun_cellars'] },
  bridge_obstruction: { blockedRoutes: ['riverbed_bridge'] },
  nightfall: { visibilityScale: 0.4 }
}

export function pickSurpriseEvent(events: readonly string[], seed: string): SurpriseEvent {
  if (!events.length) throw new Error('No surprise events defined for this map')
  let h = 2166136261
  for (const ch of seed) h = Math.imul(h ^ ch.charCodeAt(0), 16777619)
  const pick = events[(h >>> 0) % events.length] as string
  if (!(pick in EFFECTS)) throw new Error(`Unknown surprise event ${pick}`)
  return pick as SurpriseEvent
}

export class SurpriseEventDirector {
  active: SurpriseEvent | undefined
  private fired = false
  constructor(readonly planned: SurpriseEvent) {}

  /** Fires the planned event once per match; later calls do nothing. */
  trigger(): SurpriseEvent | undefined {
    if (this.fired) return undefined
    this.fired = true
    this.active = this.planned
    return this.active
  }

  end(): void { this.active = undefined }
  /** Checkpoint restore. */
  restore(fired: boolean, active: string | null): void { this.fired = fired; this.active = active && active in EFFECTS ? active as SurpriseEvent : undefined }
  get hasFired(): boolean { return this.fired }
  get modifiers(): SurpriseModifiers { return this.active ? { ...NONE, ...EFFECTS[this.active] } : NONE }
}
