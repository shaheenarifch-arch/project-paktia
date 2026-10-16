// The Kharg oil terminal as game objects: each facility has an integrity of
// 0..1. The mission rewards taking it intact; damage lowers the outcome.
export interface TerminalFacility { id: string; name: string; integrity: number }

export class OilTerminal {
  facilities: TerminalFacility[] = [
    { id: 'east_jetty', name: 'East jetty', integrity: 1 },
    { id: 'sea_island_jetty', name: 'Sea Island jetty', integrity: 1 },
    { id: 'tank_farm', name: 'Tank farm', integrity: 1 },
    { id: 'pump_station', name: 'Pump station', integrity: 1 }
  ]

  damage(id: string, amount: number): boolean {
    const f = this.facilities.find(x => x.id === id)
    if (!f) return false
    f.integrity = Math.max(0, Math.round((f.integrity - Math.max(0, amount)) * 1000) / 1000)
    return true
  }

  /** Average integrity across facilities, 0..1. */
  get integrity(): number { return Math.round((this.facilities.reduce((s, f) => s + f.integrity, 0) / this.facilities.length) * 1000) / 1000 }
  /** Exports need at least one working jetty and the pump station. */
  get canExport(): boolean { return this.facilities.some(f => f.id.endsWith('jetty') && f.integrity > 0.3) && (this.facilities.find(f => f.id === 'pump_station')?.integrity ?? 0) > 0.3 }

  snapshot(): Record<string, unknown> { return { facilities: this.facilities.map(f => ({ ...f })) } }
  restore(data: Record<string, unknown>): void { this.facilities = (data.facilities as TerminalFacility[]).map(f => ({ ...f })) }
}
