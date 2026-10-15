import { Vector3, distance } from '../../core/types.js'
export type ProtectedZoneType = 'PROTECTED_LOCATION' | 'FRIENDLY_BASE' | 'MISSION_EXCLUSION_ZONE' | 'FRIENDLY_OBJECTIVE'
export class ProtectedZone {
  constructor(public readonly id: string, public readonly type: ProtectedZoneType, public readonly center: Vector3, public readonly radius: number) {}
  contains(position: Vector3): boolean { return distance(this.center, position) <= this.radius }
}
