import { distance } from '../../core/types.js'
import type { Vector3 } from '../../core/types.js'

export type RecoveryZoneState =
  | 'INACTIVE'
  | 'DESIGNATED'
  | 'ACTIVE'
  | 'CONTESTED'
  | 'SECURED'
  | 'EVACUATION_COMPLETE'

export class RecoveryZone {
  state: RecoveryZoneState = 'INACTIVE'
  center?: Vector3

  designate(center: Vector3): void {
    this.center = { ...center }
    this.state = 'DESIGNATED'
  }

  activate(): void {
    if (this.state === 'DESIGNATED') {
      this.state = 'ACTIVE'
    }
  }

  contest(): void {
    if (
      this.state === 'ACTIVE' ||
      this.state === 'SECURED'
    ) {
      this.state = 'CONTESTED'
    }
  }

  secure(): void {
    if (
      this.state === 'ACTIVE' ||
      this.state === 'CONTESTED'
    ) {
      this.state = 'SECURED'
    }
  }

  completeEvacuation(): void {
    if (this.state === 'SECURED') {
      this.state = 'EVACUATION_COMPLETE'
    }
  }

  contains(position: Vector3, radius = 40): boolean {
    if (!this.center) return false

    return distance(this.center, position) <= radius
  }
}
