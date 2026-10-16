import type { Vector3 } from '../../../core/types.js'
import { EventBus } from '../../../core/EventBus.js'
import { AegisSupportController } from '../../support/AegisSupportController.js'
import { ProtectedZone, type ProtectedZoneType } from '../../support/ProtectedZone.js'
import { MissionRunner, type MissionEvents } from '../framework/MissionRunner.js'
import { parseMissionDefinition, type MissionDefinition, type MissionStateDefinition } from '../framework/MissionDefinition.js'
import type { E02Carryover } from '../episode02/SilentCorridor.js'
import { SurpriseEventDirector, pickSurpriseEvent } from './SurpriseEventDirector.js'

// Episode 03: Operation Broken Citadel as a campaign mission. The five phases
// and their objectives come from the Stronghold Assault data
// (data/missions/operation-broken-citadel.json), so the campaign and the
// multiplayer mode share one source. The episode file adds campaign rules:
// phase completion rules, unlocks, reinforcements and the surprise event.

export interface StrongholdData { id: string; name: string; phases: readonly { order: number; name: string; objectives: readonly string[] }[] }
export interface GardunMapData { id: string; sectors: readonly { id: string }[]; surpriseEvents: readonly string[] }
export interface Episode03Data {
  id: string; episode: number; name: string; fictional: true; map: string; stronghold: string; briefing: string
  phaseRules: Readonly<Record<string, 'any' | 'all'>>
  baseReinforcements: number; reinforcementsPerNomad: number; surpriseEventPhase: number
  unlocks: Readonly<Record<string, string>>
  phaseObjectives: Readonly<Record<string, string>>
  protectedZones: readonly { id: string; type: ProtectedZoneType; position: Vector3; radius: number }[]
}

// Which Gardun sectors each phase fights over (docs/gardun-map.md).
const PHASE_SECTORS: Record<number, readonly string[]> = {
  1: ['airfield', 'industrial'], 2: ['west_ridge', 'government'], 3: ['old_gardun'], 4: ['mountain_fort', 'east_power'], 5: ['citadel']
}

export function buildBrokenCitadelDefinition(episode: Episode03Data, stronghold: StrongholdData, map: GardunMapData): MissionDefinition {
  if (episode.stronghold !== stronghold.id) throw new Error(`Episode 03 expects ${episode.stronghold}, got ${stronghold.id}`)
  const phases = [...stronghold.phases].sort((a, b) => a.order - b.order)
  const states: MissionStateDefinition[] = [
    { id: 'E03_BRIEFING', objective: episode.briefing, next: 'E03_STAGING' },
    { id: 'E03_STAGING', objective: 'Stage at South Gate and assign squads to the first objectives.', requires: ['staging_ready'], checkpoint: true, next: 'E03_PHASE_1' }
  ]
  phases.forEach((p, i) => {
    states.push({
      id: `E03_PHASE_${p.order}`,
      objective: episode.phaseObjectives[String(p.order)] ?? `${p.name}: ${p.objectives.join(', ')}`,
      requires: [`phase_${p.order}_complete`],
      checkpoint: true,
      ...(p.order === episode.surpriseEventPhase ? { effects: ['surprise.trigger'] } : {}),
      next: i + 1 < phases.length ? `E03_PHASE_${phases[i + 1]!.order}` : 'E03_CITADEL_SECURED'
    })
  })
  states.push({ id: 'E03_CITADEL_SECURED', objective: 'Gardun Citadel secured. Operation Broken Citadel complete.', terminal: true, outcome: 'SUCCESS' })
  states.push({ id: 'E03_ASSAULT_FAILED', objective: 'Reinforcements exhausted. Regroup at South Gate and try again from the last checkpoint.', terminal: true, outcome: 'FAILED' })
  return parseMissionDefinition({
    id: episode.id, episode: episode.episode, name: episode.name, fictional: true, map: episode.map,
    sectors: ['south_gate', ...phases.flatMap(p => PHASE_SECTORS[p.order] ?? [])],
    start: 'E03_BRIEFING', failState: 'E03_ASSAULT_FAILED', states
  }, map.sectors.map(s => s.id))
}

export class BrokenCitadelMission {
  readonly runner: MissionRunner
  readonly surprise: SurpriseEventDirector
  reinforcements: number
  private captured = new Set<string>()
  private unlocked = new Set<string>()
  private readonly phases: StrongholdData['phases']

  constructor(
    readonly episode: Episode03Data,
    stronghold: StrongholdData,
    map: GardunMapData,
    readonly aegis: AegisSupportController,
    readonly carryover: E02Carryover | undefined,
    seed: string,
    events = new EventBus<MissionEvents>()
  ) {
    this.phases = [...stronghold.phases].sort((a, b) => a.order - b.order)
    this.surprise = new SurpriseEventDirector(pickSurpriseEvent(map.surpriseEvents, seed))
    // Episode 02 decides the starting strength: each surviving Nomad brings more troops.
    this.reinforcements = episode.baseReinforcements + episode.reinforcementsPerNomad * (carryover?.nomadsRemaining ?? 0)
    this.aegis.setProtectedZones(episode.protectedZones.map(z => new ProtectedZone(z.id, z.type, z.position, z.radius)))
    this.runner = new MissionRunner(buildBrokenCitadelDefinition(episode, stronghold, map), {
      'surprise.trigger': () => { this.surprise.trigger() }
    }, events, {
      snapshot: () => ({ captured: [...this.captured], unlocked: [...this.unlocked], reinforcements: this.reinforcements, surprise: { fired: this.surprise.hasFired, active: this.surprise.active ?? null }, aegis: this.aegis.state }),
      restore: data => {
        this.captured = new Set(data.captured as string[])
        this.unlocked = new Set(data.unlocked as string[])
        this.reinforcements = data.reinforcements as number
        const s = data.surprise as { fired: boolean; active: string | null }
        this.surprise.restore(s.fired, s.active)
        this.aegis.setOnline(this.unlocked.has('aegis_online'))
      }
    })
  }

  get state(): string { return this.runner.state }
  get phase(): number | undefined { const m = /^E03_PHASE_(\d+)$/.exec(this.runner.state); return m ? Number(m[1]) : undefined }
  get capturedObjectives(): readonly string[] { return [...this.captured] }
  get unlocks(): readonly string[] { return [...this.unlocked] }

  /** Objectives of the current phase still open. */
  openObjectives(): readonly string[] {
    const p = this.phases.find(x => x.order === this.phase)
    return p ? p.objectives.filter(o => !this.captured.has(o)) : []
  }

  beginAssault(): string {
    this.runner.advance()                  // briefing → staging
    this.runner.setFlag('staging_ready')
    return this.runner.advance()           // staging → phase 1
  }

  /** Server-confirmed capture. Only objectives of the current phase count. */
  captureObjective(id: string): boolean {
    const p = this.phases.find(x => x.order === this.phase)
    if (!p || !p.objectives.includes(id) || this.captured.has(id)) return false
    this.captured.add(id)
    this.applyUnlock(id)
    const rule = this.episode.phaseRules[String(p.order)] ?? 'all'
    const done = rule === 'any' ? p.objectives.some(o => this.captured.has(o)) : p.objectives.every(o => this.captured.has(o))
    if (done) {
      this.runner.setFlag(`phase_${p.order}_complete`)
      this.runner.advance()
      // A new phase ends the previous phase's surprise event, except the one it started.
      if (this.phase !== this.episode.surpriseEventPhase && this.surprise.active) this.surprise.end()
    }
    return true
  }

  /** Squads lost; at zero the assault fails back to the last checkpoint. */
  spendReinforcements(count: number): number {
    this.reinforcements = Math.max(0, this.reinforcements - Math.max(0, count))
    if (this.reinforcements === 0) this.runner.fail()
    return this.reinforcements
  }

  /** Falcon air support needs both the airfield and the crew Episode 02 delivered. */
  get airSupportAvailable(): boolean { return this.unlocked.has('falcon_air_support') && Boolean(this.carryover?.crewDelivered) }

  /** Enemy counterattacks weaken once both relay sites in phase 2 are taken. */
  get counterattackStrength(): number {
    const comms = ['west_ridge_radio', 'government_relay'].every(o => this.captured.has(o))
    return comms ? 0.6 : 1
  }

  private applyUnlock(objective: string): void {
    const unlock = this.episode.unlocks[objective]
    if (!unlock) return
    this.unlocked.add(unlock)
    if (unlock === 'aegis_online') this.aegis.setOnline(true)
  }

  snapshot(): Record<string, unknown> {
    return {
      mission: this.episode.name, state: this.runner.state, phase: this.phase ?? null, objective: this.runner.objective,
      openObjectives: this.openObjectives(), captured: this.capturedObjectives, unlocks: this.unlocks,
      reinforcements: this.reinforcements, airSupport: this.airSupportAvailable, counterattackStrength: this.counterattackStrength,
      surpriseEvent: { planned: this.surprise.planned, active: this.surprise.active ?? null, modifiers: this.surprise.modifiers },
      aegis: this.aegis.state
    }
  }
}
