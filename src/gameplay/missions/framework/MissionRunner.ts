import { EventBus } from '../../../core/EventBus.js'
import type { MissionDefinition, MissionOutcome, MissionStateDefinition } from './MissionDefinition.js'

export interface MissionEvents {
  [key: string]: unknown
  'mission:state': { missionId: string; from: string | undefined; to: string }
  'mission:blocked': { missionId: string; state: string; missing: readonly string[] }
  'mission:checkpoint': { missionId: string; checkpoint: MissionCheckpoint }
  'mission:complete': { missionId: string; state: string; outcome: MissionOutcome }
}

export interface MissionCheckpoint {
  missionId: string
  state: string
  flags: readonly string[]
  history: readonly string[]
  /** The mission's own systems (convoy, relays, ...) captured by its snapshot provider. */
  systems: Record<string, unknown>
}

export type EffectHandler = (state: MissionStateDefinition) => void

/**
 * Runs a MissionDefinition: follows `next`, takes named `branches`, enforces
 * `requires` flags, runs `effects` through the mission's handlers, records
 * checkpoints and emits every transition on the EventBus so UI, audio and
 * telemetry subscribe instead of polling.
 */
export class MissionRunner {
  private current: MissionStateDefinition
  private readonly states: Map<string, MissionStateDefinition>
  private readonly flagSet = new Set<string>()
  private readonly trail: string[] = []
  private lastCheckpoint: MissionCheckpoint | undefined
  outcome: MissionOutcome | undefined

  constructor(
    readonly definition: MissionDefinition,
    private readonly effects: Readonly<Record<string, EffectHandler>> = {},
    readonly events = new EventBus<MissionEvents>(),
    private readonly systems: { snapshot(): Record<string, unknown>; restore(data: Record<string, unknown>): void } = { snapshot: () => ({}), restore: () => {} }
  ) {
    this.states = new Map(definition.states.map(s => [s.id, s]))
    for (const s of definition.states) for (const e of s.effects ?? []) if (!effects[e]) throw new Error(`Mission ${definition.id} has no handler for effect ${e}`)
    this.current = this.require(definition.start)
    this.enter(this.current, undefined)
  }

  get state(): string { return this.current.id }
  get objective(): string { return this.current.objective }
  get flags(): readonly string[] { return [...this.flagSet] }
  get history(): readonly string[] { return [...this.trail] }
  get complete(): boolean { return Boolean(this.current.terminal) }
  get choices(): readonly string[] { return Object.keys(this.current.branches ?? {}) }
  get checkpoint(): MissionCheckpoint | undefined { return this.lastCheckpoint }

  setFlag(flag: string): void { this.flagSet.add(flag) }
  clearFlag(flag: string): void { this.flagSet.delete(flag) }
  hasFlag(flag: string): boolean { return this.flagSet.has(flag) }

  /** Flags the current state still needs before it can be left. */
  missing(): readonly string[] { return (this.current.requires ?? []).filter(f => !this.flagSet.has(f)) }

  /** Moves to `next` when requirements are met. Returns the (possibly unchanged) state. */
  advance(): string {
    if (this.complete || !this.current.next) return this.state
    if (!this.ready()) return this.state
    this.go(this.current.next)
    return this.state
  }

  /** Takes a named branch from the current state. */
  choose(branch: string): string {
    const target = this.current.branches?.[branch]
    if (!target) throw new Error(`${this.state} has no branch "${branch}" (choices: ${this.choices.join(', ') || 'none'})`)
    if (!this.ready()) return this.state
    this.go(target)
    return this.state
  }

  /** Ends the mission in its fail state (e.g. convoy lost), if it defines one. */
  fail(): string {
    if (!this.complete && this.definition.failState) this.go(this.definition.failState)
    return this.state
  }

  /** Returns to the last checkpoint: state, flags and the mission's systems. */
  restoreCheckpoint(checkpoint = this.lastCheckpoint): boolean {
    if (!checkpoint || checkpoint.missionId !== this.definition.id) return false
    this.current = this.require(checkpoint.state)
    this.flagSet.clear(); checkpoint.flags.forEach(f => this.flagSet.add(f))
    this.trail.splice(0, this.trail.length, ...checkpoint.history)
    this.outcome = undefined
    this.systems.restore(structuredClone(checkpoint.systems))
    this.lastCheckpoint = checkpoint
    this.events.emit('mission:state', { missionId: this.definition.id, from: undefined, to: this.state })
    return true
  }

  private ready(): boolean {
    const missing = this.missing()
    if (missing.length) { this.events.emit('mission:blocked', { missionId: this.definition.id, state: this.state, missing }); return false }
    return true
  }

  private go(id: string): void {
    const from = this.current.id
    this.current = this.require(id)
    this.enter(this.current, from)
  }

  private enter(state: MissionStateDefinition, from: string | undefined): void {
    this.trail.push(state.id)
    for (const e of state.effects ?? []) this.effects[e]?.(state)
    this.events.emit('mission:state', { missionId: this.definition.id, from, to: state.id })
    if (state.checkpoint) {
      this.lastCheckpoint = { missionId: this.definition.id, state: state.id, flags: this.flags, history: this.history, systems: structuredClone(this.systems.snapshot()) }
      this.events.emit('mission:checkpoint', { missionId: this.definition.id, checkpoint: this.lastCheckpoint })
    }
    if (state.terminal && state.outcome) {
      this.outcome = state.outcome
      this.events.emit('mission:complete', { missionId: this.definition.id, state: state.id, outcome: state.outcome })
    }
  }

  private require(id: string): MissionStateDefinition {
    const s = this.states.get(id)
    if (!s) throw new Error(`Mission ${this.definition.id} has no state ${id}`)
    return s
  }
}
