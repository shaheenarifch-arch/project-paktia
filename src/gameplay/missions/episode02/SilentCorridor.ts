import type { Vector3 } from '../../../core/types.js'
import { EventBus } from '../../../core/EventBus.js'
import { ContactManager } from '../../intelligence/ContactManager.js'
import { DroneController } from '../../drones/DroneController.js'
import { MissionRunner, type MissionEvents } from '../framework/MissionRunner.js'
import type { MissionDefinition } from '../framework/MissionDefinition.js'
import { ConvoyEscort } from './ConvoyEscort.js'
import { RelayNetwork } from './RelayNetwork.js'

// Episode 02: Operation Silent Corridor. The state graph lives in
// data/missions/episode-02-silent-corridor.json; this class owns the systems
// (convoy, relays, ambush contacts) and turns gameplay results into the flags
// the graph waits on. Positions are game-space metres from Checkpoint Echo.
export const E02_POINTS = {
  relayKilo: { x: 600, y: 40, z: 900 } as Vector3,
  corridorEnd: { x: 1700, y: 10, z: 1500 } as Vector3,
  ambushNarrows: { x: 1250, y: 0, z: 1650 } as Vector3
}

export interface E02Carryover {
  episode: 2
  outcome: string | undefined
  crewDelivered: boolean
  nomadsRemaining: number
  wardenOperational: boolean
  route: string
  relaysOnline: number
}

export class SilentCorridorMission {
  readonly convoy = new ConvoyEscort()
  readonly relays = new RelayNetwork()
  readonly runner: MissionRunner
  private ambushContacts: string[] = []

  constructor(
    definition: MissionDefinition,
    readonly drones: DroneController,
    readonly contacts: ContactManager,
    events = new EventBus<MissionEvents>(),
    /** From Mission 01: was the Falcon-2 crew recovered? */
    readonly crewRecovered = true
  ) {
    this.runner = new MissionRunner(definition, {
      'convoy.form': () => this.convoy.form(this.crewRecovered),
      'relays.prepare': () => { /* Relay Kilo is placed by the player: see placeRelayKilo() */ },
      'convoy.depart': () => { this.convoy.depart('MAIN_ROAD') },
      'convoy.halt': () => this.convoy.halt(),
      'event.communications_outage': () => this.relays.jamAll(),
      'event.bridge_obstruction': () => this.runner?.clearFlag('bridge_cleared'),
      'convoy.detour_riverbed': () => { this.convoy.depart('RIVERBED') },
      'convoy.ambush': () => this.startAmbush(),
      'route.drainage': () => this.convoy.halt(),
      'convoy.resume': () => { this.convoy.resume() },
      'staging.establish': () => this.convoy.arrive()
    }, events, {
      snapshot: () => ({ convoy: this.convoy.snapshot(), relays: this.relays.snapshot(), ambush: [...this.ambushContacts] }),
      restore: data => {
        this.convoy.restore(data.convoy as Record<string, unknown>)
        this.relays.restoreSnapshot(data.relays as Record<string, unknown>)
        this.ambushContacts = [...(data.ambush as string[])]
      }
    })
  }

  get state(): string { return this.runner.state }

  /** Advances as far as the current flags allow (several states can open at once). */
  progress(): string {
    let before: string
    do { before = this.runner.state; this.runner.advance() } while (this.runner.state !== before)
    return this.runner.state
  }

  /** Raven flies out and drops Relay Kilo; it only counts if the link reaches it. */
  placeRelayKilo(dronePosition: Vector3 = E02_POINTS.relayKilo): boolean {
    if (this.runner.state !== 'E02_RELAY_KILO') return false
    if (this.relays.classify(dronePosition) === 'LOST') return false
    this.relays.place('relay-kilo', dronePosition)
    this.runner.setFlag('relay_kilo_online')
    return true
  }

  /** Raven reaches the end of the corridor with a usable link and marks what it sees. */
  scoutCorridor(observed: readonly { id: string; position: Vector3 }[] = [], at: Vector3 = E02_POINTS.corridorEnd, now = 0): boolean {
    if (this.runner.state !== 'E02_CORRIDOR_SCOUT') return false
    const link = this.relays.classify(at)
    if (link === 'LOST' || link === 'CRITICAL') return false
    for (const c of observed) this.contacts.upsert({ id: c.id, classification: 'GAME_UNIT', position: c.position, source: 'RAVEN', time: now, quality: this.relays.signalAt(at) })
    this.runner.setFlag('corridor_scouted')
    return true
  }

  /** A squad standing at Relay Kilo restores it after the outage. */
  restoreRelayKilo(squadPosition: Vector3): boolean {
    if (!this.relays.restore('relay-kilo', squadPosition)) return false
    this.runner.setFlag('relay_kilo_restored')
    return true
  }

  chooseRoute(choice: 'riverbed_detour' | 'drainage_infiltration'): string { return this.runner.choose(choice) }

  /** Engine reports convoy damage. Losing the crew transport (or, without crew, every Nomad) fails the mission. */
  damageVehicle(id: string, amount: number): void {
    this.convoy.applyDamage(id, amount)
    if (this.convoy.missionCritical) this.runner.fail()
  }

  /** Each ambush contact the squads neutralise; the ambush is over when none remain. */
  clearAmbushContact(id: string, now = 0): boolean {
    if (!this.ambushContacts.includes(id)) return false
    this.ambushContacts = this.ambushContacts.filter(x => x !== id)
    this.contacts.loseTracking(id, now)
    if (!this.ambushContacts.length && this.runner.state === 'E02_RIVERBED_DETOUR') { this.runner.setFlag('ambush_repelled'); this.convoy.resume() }
    return true
  }

  get remainingAmbush(): readonly string[] { return [...this.ambushContacts] }

  clearFarBank(): void { if (this.runner.state === 'E02_DRAINAGE_INFILTRATION') this.runner.setFlag('far_bank_clear') }
  clearBridge(): void { if (this.runner.state === 'E02_BRIDGE_CLEARED' && this.convoy.wardenOperational) this.runner.setFlag('bridge_cleared') }
  clearStaging(): void { if (this.runner.state === 'E02_SOUTH_GATE_APPROACH') this.runner.setFlag('staging_clear') }

  /** Closes the episode: full strength or with losses, depending on the convoy. */
  finish(): string {
    if (this.runner.state !== 'E02_STAGING_ESTABLISHED') return this.runner.state
    return this.runner.choose(this.convoy.fullStrength ? 'full_strength' : 'with_losses')
  }

  carryover(): E02Carryover {
    return {
      episode: 2,
      outcome: this.runner.outcome,
      crewDelivered: this.convoy.crewSafe && this.convoy.state === 'ARRIVED' && this.runner.outcome !== 'FAILED',
      nomadsRemaining: this.convoy.nomadsRemaining,
      wardenOperational: this.convoy.wardenOperational,
      route: this.runner.history.includes('E02_RIVERBED_DETOUR') ? 'RIVERBED' : this.runner.history.includes('E02_DRAINAGE_INFILTRATION') ? 'RIVER_DRAINAGE' : 'MAIN_ROAD',
      relaysOnline: this.relays.online
    }
  }

  private startAmbush(): void {
    this.convoy.ambush()
    const p = E02_POINTS.ambushNarrows
    this.ambushContacts = ['e02-ambush-1', 'e02-ambush-2', 'e02-ambush-3']
    this.ambushContacts.forEach((id, i) => {
      this.contacts.upsert({ id, classification: 'GAME_UNIT', position: { x: p.x + i * 40, y: p.y, z: p.z - i * 25 }, source: 'RAVEN', time: 0, quality: 0.8 })
    })
  }

  snapshot(): Record<string, unknown> {
    return { mission: 'Operation Silent Corridor', state: this.runner.state, objective: this.runner.objective, flags: this.runner.flags, convoy: this.convoy.snapshot(), relays: this.relays.snapshot(), ambush: this.remainingAmbush }
  }
}
