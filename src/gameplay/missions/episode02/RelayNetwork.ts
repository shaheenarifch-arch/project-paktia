import { DroneSignalManager, type SignalState } from '../../drones/DroneSignalManager.js'
import { distance, type Vector3 } from '../../../core/types.js'

// Field relays that carry the drone link down the corridor. Signal at a point
// is the best link through any working relay (DroneSignalManager), minus the
// current interference. A communications outage jams every relay until a
// squad restores it in person.
export type RelayState = 'STOWED' | 'ONLINE' | 'JAMMED'
export interface FieldRelay { id: string; position: Vector3; range: number; state: RelayState }

export const RELAY_RESTORE_DISTANCE = 30

export class RelayNetwork {
  relays: FieldRelay[] = []
  interference = 0
  constructor(private readonly signal = new DroneSignalManager(), readonly base: FieldRelay = { id: 'echo-base', position: { x: 0, y: 0, z: 0 }, range: 1600, state: 'ONLINE' }) {}

  place(id: string, position: Vector3, range = 1600): FieldRelay {
    const existing = this.relays.find(r => r.id === id)
    const relay = existing ?? { id, position: { ...position }, range, state: 'ONLINE' as RelayState }
    if (existing) { existing.position = { ...position }; existing.state = 'ONLINE' } else this.relays.push(relay)
    return relay
  }

  /** Communications outage: every field relay is jammed. */
  jamAll(interference = 0.35): void {
    this.interference = interference
    this.relays.forEach(r => { if (r.state === 'ONLINE') r.state = 'JAMMED' })
  }

  /** A squad standing at the relay brings it back. */
  restore(id: string, squadPosition: Vector3): boolean {
    const r = this.relays.find(x => x.id === id)
    if (!r || r.state !== 'JAMMED' || distance(r.position, squadPosition) > RELAY_RESTORE_DISTANCE) return false
    r.state = 'ONLINE'
    if (!this.relays.some(x => x.state === 'JAMMED')) this.interference = 0
    return true
  }

  signalAt(position: Vector3): number {
    const sources = [this.base, ...this.relays].filter(r => r.state === 'ONLINE')
    return sources.reduce((best, r) => Math.max(best, this.signal.calculate(position, r.position, r.range, this.interference)), 0)
  }

  classify(position: Vector3): SignalState { return this.signal.classify(this.signalAt(position)) }
  get online(): number { return this.relays.filter(r => r.state === 'ONLINE').length }

  snapshot(): Record<string, unknown> { return { interference: this.interference, relays: this.relays.map(r => ({ ...r, position: { ...r.position } })) } }
  restoreSnapshot(data: Record<string, unknown>): void {
    this.interference = data.interference as number
    this.relays = (data.relays as FieldRelay[]).map(r => ({ ...r, position: { ...r.position } }))
  }
}
