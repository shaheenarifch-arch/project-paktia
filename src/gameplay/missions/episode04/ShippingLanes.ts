// The strait's two shipping lanes. Each holds hidden hazards (abstract game
// obstacles). Drones reveal them; clearance teams remove revealed ones. A
// lane opens when every hazard in it is cleared.
export type LaneId = 'inbound' | 'outbound'
export interface Lane { id: LaneId; hazards: number; revealed: number; cleared: number }

export class ShippingLanes {
  lanes: Lane[] = []

  seed(hazards: Readonly<Record<LaneId, number>> = { inbound: 4, outbound: 3 }): void {
    this.lanes = (Object.keys(hazards) as LaneId[]).map(id => ({ id, hazards: hazards[id], revealed: 0, cleared: 0 }))
  }

  /** A drone pass reveals up to `count` more hazards in the lane. */
  reveal(id: LaneId, count = 1): number {
    const l = this.lane(id)
    if (!l) return 0
    const n = Math.min(Math.max(0, count), l.hazards - l.revealed)
    l.revealed += n
    return n
  }

  /** A clearance team removes one revealed hazard. */
  clear(id: LaneId): boolean {
    const l = this.lane(id)
    if (!l || l.cleared >= l.revealed) return false
    l.cleared += 1
    return true
  }

  isOpen(id: LaneId): boolean { const l = this.lane(id); return Boolean(l && l.cleared === l.hazards) }
  get allOpen(): boolean { return this.lanes.length > 0 && this.lanes.every(l => l.cleared === l.hazards) }
  private lane(id: LaneId): Lane | undefined { return this.lanes.find(l => l.id === id) }

  snapshot(): Record<string, unknown> { return { lanes: this.lanes.map(l => ({ ...l })) } }
  restore(data: Record<string, unknown>): void { this.lanes = (data.lanes as Lane[]).map(l => ({ ...l })) }
}
