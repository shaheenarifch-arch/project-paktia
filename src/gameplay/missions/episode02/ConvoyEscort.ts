// The Episode 02 convoy: Warden-1 leads, Nomad-1 carries the recovered
// Falcon-2 crew, Nomad-2 carries supplies. Integrity is 0..1; at 0 a vehicle
// is destroyed and its cargo is lost. Vehicles stay out of tunnels (docs/gardun-map.md).
export type ConvoyVehicleKind = 'WARDEN' | 'NOMAD'
export type ConvoyCargo = 'FALCON_CREW' | 'SUPPLIES' | 'NONE'
export type ConvoyState = 'FORMING' | 'MOVING' | 'HALTED' | 'AMBUSHED' | 'ARRIVED'
export type ConvoyRoute = 'MAIN_ROAD' | 'RIVERBED' | 'RIVER_DRAINAGE'

export interface ConvoyVehicle { id: string; kind: ConvoyVehicleKind; cargo: ConvoyCargo; integrity: number }

// The drainage tunnel is an infantry route: no vehicle fits.
const INFANTRY_ONLY: readonly ConvoyRoute[] = ['RIVER_DRAINAGE']

export class ConvoyEscort {
  state: ConvoyState = 'FORMING'
  route: ConvoyRoute = 'MAIN_ROAD'
  vehicles: ConvoyVehicle[] = []

  /** `withCrew` is false when Mission 01 abandoned the crash recovery: Nomad-1 carries supplies instead. */
  form(withCrew = true): void {
    this.state = 'FORMING'
    this.route = 'MAIN_ROAD'
    this.vehicles = [
      { id: 'warden-1', kind: 'WARDEN', cargo: 'NONE', integrity: 1 },
      { id: 'nomad-1', kind: 'NOMAD', cargo: withCrew ? 'FALCON_CREW' : 'SUPPLIES', integrity: 1 },
      { id: 'nomad-2', kind: 'NOMAD', cargo: 'SUPPLIES', integrity: 1 }
    ]
  }

  static canUseRoute(_kind: ConvoyVehicleKind, route: ConvoyRoute): boolean {
    return !INFANTRY_ONLY.includes(route)
  }

  /** The whole convoy moves on a route every vehicle can use. */
  depart(route: ConvoyRoute = this.route): boolean {
    if (this.state === 'ARRIVED' || !this.alive().length) return false
    if (!this.alive().every(v => ConvoyEscort.canUseRoute(v.kind, route))) return false
    this.route = route
    this.state = 'MOVING'
    return true
  }

  halt(): void { if (this.state !== 'ARRIVED') this.state = 'HALTED' }
  ambush(): void { if (this.state === 'MOVING' || this.state === 'HALTED') this.state = 'AMBUSHED' }
  resume(): boolean { if (this.state === 'ARRIVED' || !this.alive().length) return false; this.state = 'MOVING'; return true }
  arrive(): void { if (this.alive().length) this.state = 'ARRIVED' }

  applyDamage(id: string, amount: number): ConvoyVehicle | undefined {
    const v = this.vehicles.find(x => x.id === id)
    if (!v || v.integrity <= 0) return v
    v.integrity = Math.max(0, Math.round((v.integrity - Math.max(0, amount)) * 1000) / 1000)
    return v
  }

  repair(id: string, amount: number): boolean {
    const v = this.vehicles.find(x => x.id === id)
    if (!v || v.integrity <= 0) return false   // destroyed vehicles can't be repaired in the field
    v.integrity = Math.min(1, v.integrity + Math.max(0, amount))
    return true
  }

  alive(): readonly ConvoyVehicle[] { return this.vehicles.filter(v => v.integrity > 0) }
  get carriesCrew(): boolean { return this.vehicles.some(v => v.cargo === 'FALCON_CREW') }
  get crewSafe(): boolean { return this.vehicles.some(v => v.cargo === 'FALCON_CREW' && v.integrity > 0) }
  /** The convoy has failed: the crew transport is gone, or (without crew) every Nomad is. */
  get missionCritical(): boolean { return this.carriesCrew ? !this.crewSafe : this.nomadsRemaining === 0 }
  get nomadsRemaining(): number { return this.alive().filter(v => v.kind === 'NOMAD').length }
  get wardenOperational(): boolean { return this.alive().some(v => v.kind === 'WARDEN') }
  get fullStrength(): boolean { return this.vehicles.length > 0 && this.alive().length === this.vehicles.length }

  snapshot(): Record<string, unknown> { return { state: this.state, route: this.route, vehicles: this.vehicles.map(v => ({ ...v })) } }
  restore(data: Record<string, unknown>): void {
    this.state = data.state as ConvoyState
    this.route = data.route as ConvoyRoute
    this.vehicles = (data.vehicles as ConvoyVehicle[]).map(v => ({ ...v }))
  }
}
