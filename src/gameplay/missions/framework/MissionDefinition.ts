// Data-driven mission definitions (data/missions/episode-*.json).
// A mission is a graph of states. Each state can:
//   next      — the single state that follows once its requirements are met
//   branches  — named choices the player/commander makes (route A vs route B)
//   requires  — flags that must be set before the state can be left
//   effects   — named effects the mission's handlers run on entering it
//   checkpoint — whether entering it records a checkpoint
//   terminal  — mission ends here with `outcome`
// Engine adapters load the JSON; the domain layer stays engine-agnostic.

export type MissionOutcome = 'SUCCESS' | 'PARTIAL' | 'FAILED'

export interface MissionStateDefinition {
  id: string
  objective: string
  next?: string
  branches?: Readonly<Record<string, string>>
  requires?: readonly string[]
  effects?: readonly string[]
  checkpoint?: boolean
  terminal?: boolean
  outcome?: MissionOutcome
}

export interface MissionDefinition {
  id: string
  episode: number
  name: string
  fictional: true
  map: string
  sectors: readonly string[]
  start: string
  failState?: string
  states: readonly MissionStateDefinition[]
}

const OUTCOMES: readonly MissionOutcome[] = ['SUCCESS', 'PARTIAL', 'FAILED']

/** Validates raw JSON into a MissionDefinition; throws with every problem found. */
export function parseMissionDefinition(raw: unknown, knownSectors?: readonly string[]): MissionDefinition {
  const problems: string[] = []
  const def = raw as Partial<MissionDefinition>
  if (!def || typeof def !== 'object') throw new Error('Mission definition must be an object')
  if (def.fictional !== true) problems.push('mission must be marked fictional')
  for (const key of ['id', 'name', 'map', 'start'] as const) if (typeof def[key] !== 'string' || !def[key]) problems.push(`missing ${key}`)
  if (typeof def.episode !== 'number') problems.push('missing episode number')
  if (!Array.isArray(def.states) || !def.states.length) problems.push('mission needs states')
  if (!Array.isArray(def.sectors)) problems.push('missing sectors')
  const ids = new Set<string>()
  for (const s of def.states ?? []) {
    if (!s.id || typeof s.objective !== 'string') problems.push(`state ${s.id ?? '?'} needs an id and objective`)
    if (ids.has(s.id)) problems.push(`duplicate state ${s.id}`)
    ids.add(s.id)
  }
  for (const s of def.states ?? []) {
    const targets = [s.next, ...Object.values(s.branches ?? {})].filter((t): t is string => Boolean(t))
    for (const t of targets) if (!ids.has(t)) problems.push(`${s.id} leads to unknown state ${t}`)
    if (s.terminal && (s.next || s.branches)) problems.push(`terminal state ${s.id} cannot lead anywhere`)
    if (s.terminal && !OUTCOMES.includes(s.outcome as MissionOutcome)) problems.push(`terminal state ${s.id} needs an outcome`)
    if (!s.terminal && !targets.length) problems.push(`state ${s.id} is a dead end`)
  }
  if (def.start && !ids.has(def.start)) problems.push(`unknown start state ${def.start}`)
  if (def.failState && !ids.has(def.failState)) problems.push(`unknown fail state ${def.failState}`)
  if (knownSectors) for (const sector of def.sectors ?? []) if (!knownSectors.includes(sector)) problems.push(`unknown sector ${sector}`)
  // Every state must be reachable from the start (or be the fail state).
  if (def.start && ids.has(def.start)) {
    const byId = new Map((def.states ?? []).map(s => [s.id, s]))
    const seen = new Set<string>([def.start])
    const queue = [def.start]
    while (queue.length) {
      const s = byId.get(queue.shift() as string)
      for (const t of [s?.next, ...Object.values(s?.branches ?? {})]) if (t && !seen.has(t)) { seen.add(t); queue.push(t) }
    }
    for (const id of ids) if (!seen.has(id) && id !== def.failState) problems.push(`state ${id} is unreachable`)
  }
  if (problems.length) throw new Error(`Invalid mission ${def.id ?? '?'}: ${problems.join('; ')}`)
  return def as MissionDefinition
}
