import type { LaneId } from './ShippingLanes.js'

// Tankers waiting to transit. Each one through counts toward the supply flow
// the world sees; a tanker at 0 integrity is lost.
export type TankerState = 'WAITING' | 'TRANSITING' | 'THROUGH' | 'LOST'
export interface Tanker { id: string; state: TankerState; integrity: number; lane?: LaneId }

export class TankerEscort {
  tankers: Tanker[] = []

  assemble(count: number): void {
    this.tankers = Array.from({ length: count }, (_, i) => ({ id: `tanker-${i + 1}`, state: 'WAITING' as TankerState, integrity: 1 }))
  }

  begin(id: string, lane: LaneId, laneOpen: boolean): boolean {
    const t = this.tankers.find(x => x.id === id)
    if (!t || t.state !== 'WAITING' || !laneOpen) return false
    t.state = 'TRANSITING'; t.lane = lane
    return true
  }

  arrive(id: string): boolean {
    const t = this.tankers.find(x => x.id === id)
    if (!t || t.state !== 'TRANSITING') return false
    t.state = 'THROUGH'
    return true
  }

  damage(id: string, amount: number): void {
    const t = this.tankers.find(x => x.id === id)
    if (!t || t.state === 'THROUGH' || t.state === 'LOST') return
    t.integrity = Math.max(0, Math.round((t.integrity - Math.max(0, amount)) * 1000) / 1000)
    if (t.integrity === 0) t.state = 'LOST'
  }

  get through(): number { return this.tankers.filter(t => t.state === 'THROUGH').length }
  get lost(): number { return this.tankers.filter(t => t.state === 'LOST').length }
  get settled(): boolean { return this.tankers.length > 0 && this.tankers.every(t => t.state === 'THROUGH' || t.state === 'LOST') }
  /** Share of planned tankers that got through, 0..1: the supply line the world sees. */
  get supplyFlow(): number { return this.tankers.length ? Math.round((this.through / this.tankers.length) * 1000) / 1000 : 0 }

  snapshot(): Record<string, unknown> { return { tankers: this.tankers.map(t => ({ ...t })) } }
  restore(data: Record<string, unknown>): void { this.tankers = (data.tankers as Tanker[]).map(t => ({ ...t })) }
}
