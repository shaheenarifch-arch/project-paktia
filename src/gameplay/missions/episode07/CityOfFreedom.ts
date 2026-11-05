import { EventBus } from '../../../core/EventBus.js'
import { MissionRunner, type MissionEvents } from '../framework/MissionRunner.js'
import type { MissionDefinition, MissionOutcome } from '../framework/MissionDefinition.js'
import { RulesOfEngagement, type RoeRules, type ViolationKind } from './RulesOfEngagement.js'

// Episode 07: City of Freedom. A hypothetical urban operation in the invented
// city of Parvaneh (data/missions/episode-07-city-of-freedom.json, setting
// real_world_hypothetical). Allied resistance squads are AI; civilians are in
// the streets; the ROE system penalises harm to civilians and surrendered
// soldiers. The enemy is the regime's security forces, never the people.

export interface ResistanceSquadDef { id: string; name: string; strength: number }
export interface Episode07Rules {
  resistanceSquads: ResistanceSquadDef[]
  civilians: Record<string, number>
  roe: RoeRules
  baseReinforcements: number
  patientsToTreat: number
  aidDropsToDeliver: number
}

export interface ResistanceSquad extends ResistanceSquadDef { lost: number; linked: boolean }

export interface E07Result {
  episode: 7
  outcome: MissionOutcome | undefined
  route: 'BRIDGES' | 'ARCADES' | undefined
  heartsAndMinds: number
  civilianHarm: number
  surrenderedHarm: number
  failedObjectives: string[]
  civiliansSheltered: number
  patientsTreated: number
  aidDropsDelivered: number
  resistanceLost: number
}

export class CityOfFreedomMission {
  readonly roe: RulesOfEngagement
  readonly runner: MissionRunner
  squads: ResistanceSquad[]
  civilians: Record<string, { present: number; sheltered: number }>
  reinforcements: number
  patientsTreated = 0
  aidDropsDelivered = 0

  constructor(definition: MissionDefinition, readonly rules: Episode07Rules, events = new EventBus<MissionEvents>()) {
    if (definition.setting !== 'real_world_hypothetical') throw new Error('Episode 07 must be declared a real-world hypothetical scenario')
    this.roe = new RulesOfEngagement(rules.roe)
    this.squads = rules.resistanceSquads.map(s => ({ ...s, lost: 0, linked: false }))
    this.civilians = Object.fromEntries(Object.entries(rules.civilians).map(([d, n]) => [d, { present: n, sheltered: 0 }]))
    this.reinforcements = rules.baseReinforcements
    this.runner = new MissionRunner(definition, {
      'roe.activate': () => { this.roe.active = true }
    }, events, {
      snapshot: () => ({ roe: this.roe.snapshot(), squads: this.squads, civilians: this.civilians, reinforcements: this.reinforcements, patientsTreated: this.patientsTreated, aidDropsDelivered: this.aidDropsDelivered }),
      restore: d => {
        this.roe.restore(d.roe as Record<string, unknown>)
        this.squads = (d.squads as ResistanceSquad[]).map(s => ({ ...s }))
        this.civilians = Object.fromEntries(Object.entries(d.civilians as CityOfFreedomMission['civilians']).map(([k, v]) => [k, { ...v }]))
        this.reinforcements = d.reinforcements as number
        this.patientsTreated = d.patientsTreated as number
        this.aidDropsDelivered = d.aidDropsDelivered as number
      }
    })
  }

  get state(): string { return this.runner.state }

  progress(): string {
    let before: string
    do { before = this.runner.state; this.runner.advance() } while (this.runner.state !== before)
    return this.runner.state
  }

  /** The allied squad reaches the rally point; all linked → the operation begins. */
  linkUp(squadId: string): boolean {
    const s = this.squads.find(x => x.id === squadId)
    if (!s || this.runner.state !== 'E07_LINK_UP' && this.runner.state !== 'E07_BRIEFING') return false
    s.linked = true
    if (this.squads.every(x => x.linked)) this.runner.setFlag('resistance_linked')
    return true
  }

  /** Engine reports an allied squad member lost. Resistance losses cost reinforcements. */
  resistanceLost(squadId: string, count = 1): void {
    const s = this.squads.find(x => x.id === squadId)
    if (!s) return
    const n = Math.min(Math.max(0, count), s.strength - s.lost)
    s.lost += n
    this.spendReinforcements(n)
  }

  /** Engine reports player-side losses; at zero the operation fails back to the checkpoint. */
  spendReinforcements(count: number): number {
    this.reinforcements = Math.max(0, this.reinforcements - Math.max(0, count))
    if (this.reinforcements === 0 && !this.runner.complete) this.runner.fail()
    return this.reinforcements
  }

  secureBazaar(): void { if (this.runner.state === 'E07_OLD_BAZAAR') this.runner.setFlag('bazaar_secure') }

  /** Moves `count` civilians from the street into shelter. */
  shelterCivilians(district: string, count: number): number {
    const c = this.civilians[district]
    if (!c) return 0
    const n = Math.min(Math.max(0, count), c.present - c.sheltered)
    c.sheltered += n
    if (this.runner.state === 'E07_PROTECT_COURTYARDS' && this.civiliansSheltered >= this.courtyardNeeded) this.runner.setFlag('civilians_sheltered')
    return n
  }
  /** The courtyard district must be fully sheltered before the objective completes. */
  private get courtyardNeeded(): number { return this.civilians['parvaneh_courtyard_quarter']?.present ?? 0 }
  get civiliansSheltered(): number { return this.civilians['parvaneh_courtyard_quarter']?.sheltered ?? 0 }

  chooseCrossing(route: 'river_bridges' | 'arcade_route'): string { this.progress(); return this.runner.choose(route) }
  secureCrossing(): void { if (/BRIDGE_ASSAULT|ARCADE_ROUTE/.test(this.runner.state)) this.runner.setFlag('crossing_secure') }
  secureSquare(): void { if (this.runner.state === 'E07_DOME_SQUARE') this.runner.setFlag('square_secure') }
  secureCompound(): void { if (this.runner.state === 'E07_SECURITY_COMPOUND') this.runner.setFlag('compound_secure') }

  /** Engine reports a civilian harmed or a surrendered soldier harmed. Fails the active objective. */
  reportViolation(kind: ViolationKind, district?: string): boolean {
    if (this.runner.complete) return false
    const recorded = this.roe.record(kind, this.runner.state, district)
    if (recorded && this.roe.limitReached) this.runner.fail()
    return recorded
  }

  /** Side objective: a casualty treated at the field hospital. */
  treatPatient(): boolean {
    if (this.runner.complete || this.patientsTreated >= this.rules.patientsToTreat) return false
    this.patientsTreated += 1
    if (this.patientsTreated === this.rules.patientsToTreat) this.roe.reward(this.rules.roe.fieldHospitalBonus)
    return true
  }

  /** Side objective: an aid drop reaches the drop zone intact. */
  deliverAidDrop(): boolean {
    if (this.runner.complete || this.aidDropsDelivered >= this.rules.aidDropsToDeliver) return false
    this.aidDropsDelivered += 1
    this.roe.reward(this.rules.roe.aidDropBonus)
    return true
  }

  /** Full success needs no violations, the Hearts & Minds target met and both side objectives done. */
  finish(): string {
    this.progress()
    if (this.runner.state !== 'E07_DEBRIEF') return this.runner.state
    const full = this.roe.violations.length === 0 && this.roe.targetMet &&
      this.patientsTreated >= this.rules.patientsToTreat && this.aidDropsDelivered >= this.rules.aidDropsToDeliver
    return this.runner.choose(full ? 'full_success' : 'partial')
  }

  result(): E07Result {
    const h = this.runner.history
    return {
      episode: 7, outcome: this.runner.outcome,
      route: h.includes('E07_BRIDGE_ASSAULT') ? 'BRIDGES' : h.includes('E07_ARCADE_ROUTE') ? 'ARCADES' : undefined,
      heartsAndMinds: this.roe.heartsAndMinds, civilianHarm: this.roe.civilianHarm, surrenderedHarm: this.roe.surrenderedHarm,
      failedObjectives: [...this.roe.failedObjectives], civiliansSheltered: this.civiliansSheltered,
      patientsTreated: this.patientsTreated, aidDropsDelivered: this.aidDropsDelivered,
      resistanceLost: this.squads.reduce((s, q) => s + q.lost, 0)
    }
  }

  snapshot(): Record<string, unknown> {
    return { mission: 'City of Freedom', state: this.runner.state, objective: this.runner.objective, reinforcements: this.reinforcements, ...this.result(), squads: this.squads.map(s => ({ ...s })) }
  }
}

