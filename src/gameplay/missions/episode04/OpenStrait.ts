import type { Vector3 } from '../../../core/types.js'
import { EventBus } from '../../../core/EventBus.js'
import { AegisSupportController } from '../../support/AegisSupportController.js'
import { ProtectedZone } from '../../support/ProtectedZone.js'
import { ContactManager } from '../../intelligence/ContactManager.js'
import { MissionRunner, type MissionEvents } from '../framework/MissionRunner.js'
import type { MissionDefinition, MissionOutcome } from '../framework/MissionDefinition.js'
import { OilTerminal } from './OilTerminal.js'
import { ShippingLanes, type LaneId } from './ShippingLanes.js'
import { TankerEscort } from './TankerEscort.js'

// Episode 04: Operation Open Strait. A hypothetical scenario on real
// geography (data/missions/episode-04-open-strait.json, setting
// real_world_hypothetical). The graph lives in the JSON; this class owns the
// island assault, the terminal, the lanes and the tankers.

export interface Episode04Rules {
  baseReinforcements: number
  bonusIfEpisode3Succeeded: number
  tankers: number
  terminalIntactThreshold: number
  supplyFlowTarget: number
}

/** Game-space positions (metres) for protected areas on the island. */
export const E04_PROTECTED: readonly { id: string; position: Vector3; radius: number }[] = [
  { id: 'kharg_oil_terminal', position: { x: 2200, y: 0, z: 900 }, radius: 450 },
  { id: 'kharg_town', position: { x: 1500, y: 0, z: 1600 }, radius: 350 }
]

export interface E04Result {
  episode: 4
  outcome: MissionOutcome | undefined
  beach: 'NORTH' | 'WEST' | undefined
  terminalIntegrity: number
  canExport: boolean
  tankersThrough: number
  tankersLost: number
  supplyFlow: number
}

export class OpenStraitMission {
  readonly terminal = new OilTerminal()
  readonly lanes = new ShippingLanes()
  readonly escort = new TankerEscort()
  readonly runner: MissionRunner
  reinforcements: number
  airSupport = false
  private beachDefences = new Map<'north' | 'west', number>()

  constructor(
    definition: MissionDefinition,
    readonly rules: Episode04Rules,
    readonly aegis: AegisSupportController,
    readonly contacts: ContactManager,
    episode3Outcome: MissionOutcome | undefined,
    events = new EventBus<MissionEvents>()
  ) {
    if (definition.setting !== 'real_world_hypothetical') throw new Error('Episode 04 must be declared a real-world hypothetical scenario')
    this.reinforcements = rules.baseReinforcements + (episode3Outcome === 'SUCCESS' ? rules.bonusIfEpisode3Succeeded : 0)
    // Aegis can never select anything in the terminal or the town.
    this.aegis.setProtectedZones(E04_PROTECTED.map(z => new ProtectedZone(z.id, 'PROTECTED_LOCATION', z.position, z.radius)))
    this.runner = new MissionRunner(definition, {
      'landing.north': () => { this.reinforcements = Math.max(0, this.reinforcements - (this.beachDefences.get('north') ?? 0)) },
      'landing.west': () => { this.reinforcements = Math.max(0, this.reinforcements - (this.beachDefences.get('west') ?? 0)) },
      'terminal.protect': () => { /* protected zones are set at start; kept as a named step for UI and audio */ },
      'lanes.seed': () => { if (!this.lanes.lanes.length) this.lanes.seed() },
      'tankers.assemble': () => { if (!this.escort.tankers.length) this.escort.assemble(rules.tankers) }
    }, events, {
      snapshot: () => ({ terminal: this.terminal.snapshot(), lanes: this.lanes.snapshot(), escort: this.escort.snapshot(), reinforcements: this.reinforcements, airSupport: this.airSupport }),
      restore: data => {
        this.terminal.restore(data.terminal as Record<string, unknown>)
        this.lanes.restore(data.lanes as Record<string, unknown>)
        this.escort.restore(data.escort as Record<string, unknown>)
        this.reinforcements = data.reinforcements as number
        this.airSupport = data.airSupport as boolean
      }
    })
  }

  get state(): string { return this.runner.state }

  progress(): string {
    let before: string
    do { before = this.runner.state; this.runner.advance() } while (this.runner.state !== before)
    return this.runner.state
  }

  /** Raven passes over both beaches; marked defences cost reinforcements on landing. */
  scoutBeaches(defences: { north: number; west: number }): boolean {
    if (this.runner.state !== 'E04_BEACH_RECON') return false
    this.beachDefences.set('north', Math.max(0, defences.north))
    this.beachDefences.set('west', Math.max(0, defences.west))
    this.runner.setFlag('beaches_scouted')
    return true
  }

  chooseBeach(beach: 'north_beach' | 'west_beach'): string { this.progress(); return this.runner.choose(beach) }

  secureBeachhead(): void { if (/_LANDING$/.test(this.runner.state)) this.runner.setFlag('beachhead_secure') }

  secureAirstrip(): void {
    if (!/AIRSTRIP/.test(this.runner.state)) return
    this.runner.setFlag('airstrip_secure')
    this.airSupport = true
  }

  secureTerminal(): void { if (/TERMINAL/.test(this.runner.state)) this.runner.setFlag('terminal_secure') }

  /** Engine reports damage to a terminal facility (stray fire, a fight inside it). */
  damageTerminal(facility: string, amount: number): boolean { return this.terminal.damage(facility, amount) }

  /** Engine reports squads lost; at zero the island assault fails back to the checkpoint. */
  spendReinforcements(count: number): number {
    this.reinforcements = Math.max(0, this.reinforcements - Math.max(0, count))
    if (this.reinforcements === 0 && !this.runner.hasFlag('terminal_secure')) this.runner.fail()
    return this.reinforcements
  }

  revealHazards(lane: LaneId, count: number): number { return this.runner.state === 'E04_LANE_CLEARANCE' ? this.lanes.reveal(lane, count) : 0 }
  clearHazard(lane: LaneId): boolean {
    if (this.runner.state !== 'E04_LANE_CLEARANCE' || !this.lanes.clear(lane)) return false
    if (this.lanes.allOpen) this.runner.setFlag('lanes_open')
    return true
  }

  startTransit(tanker: string, lane: LaneId): boolean {
    if (this.runner.state !== 'E04_TANKER_ESCORT') return false
    return this.escort.begin(tanker, lane, this.lanes.isOpen(lane))
  }

  tankerArrived(tanker: string): void { this.escort.arrive(tanker); this.checkEscort() }
  damageTanker(tanker: string, amount: number): void { this.escort.damage(tanker, amount); this.checkEscort() }

  /** Closes the episode: full success needs an intact terminal and enough tankers through. */
  finish(): string {
    this.progress()
    if (this.runner.state !== 'E04_DEBRIEF') return this.runner.state
    const full = this.terminal.integrity >= this.rules.terminalIntactThreshold && this.terminal.canExport && this.escort.supplyFlow >= this.rules.supplyFlowTarget
    return this.runner.choose(full ? 'full_success' : 'partial')
  }

  result(): E04Result {
    const h = this.runner.history
    return {
      episode: 4, outcome: this.runner.outcome,
      beach: h.includes('E04_NORTH_LANDING') ? 'NORTH' : h.includes('E04_WEST_LANDING') ? 'WEST' : undefined,
      terminalIntegrity: this.terminal.integrity, canExport: this.terminal.canExport,
      tankersThrough: this.escort.through, tankersLost: this.escort.lost, supplyFlow: this.escort.supplyFlow
    }
  }

  snapshot(): Record<string, unknown> {
    return { mission: 'Operation Open Strait', state: this.runner.state, objective: this.runner.objective, reinforcements: this.reinforcements, airSupport: this.airSupport, ...this.result(), lanes: this.lanes.snapshot(), tankers: this.escort.snapshot() }
  }

  private checkEscort(): void {
    if (this.runner.state !== 'E04_TANKER_ESCORT' || !this.escort.settled) return
    if (this.escort.through === 0) { this.runner.fail(); return }
    this.runner.setFlag('escort_complete')
    this.progress()
  }
}
