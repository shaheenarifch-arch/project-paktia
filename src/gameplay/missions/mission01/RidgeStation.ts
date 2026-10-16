export type RidgeStationState = 'UNKNOWN' | 'DISCOVERED' | 'CONTESTED' | 'CAPTURED' | 'DESTROYED' | 'LOST'
export class RidgeStation {
  state: RidgeStationState = 'UNKNOWN'
  discover(): void { if (this.state === 'UNKNOWN') this.state = 'DISCOVERED' }
  contest(): void { if (['DISCOVERED','CAPTURED'].includes(this.state)) this.state = 'CONTESTED' }
  capture(): void { if (this.state !== 'DESTROYED') this.state = 'CAPTURED' }
  destroy(): void { this.state = 'DESTROYED' }
  lose(): void { if (this.state === 'CAPTURED') this.state = 'LOST' }
  get aegisOnline(): boolean { return this.state === 'CAPTURED' }
}
