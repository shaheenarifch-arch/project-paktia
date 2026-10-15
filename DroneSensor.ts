import { EntityId, Vector3, distance } from '../../core/types.js'

export interface SensorCandidate {
  id: EntityId
  position: Vector3
  classification: string
  visible: boolean
}

export class DroneSensor {
  scan(origin: Vector3, range: number, candidates: readonly SensorCandidate[]): SensorCandidate[] {
    return candidates.filter(candidate => candidate.visible && distance(origin, candidate.position) <= range)
  }
}
