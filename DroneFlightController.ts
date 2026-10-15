import { Vector3, distance } from '../../core/types.js'

export class DroneFlightController {
  step(current: Vector3, target: Vector3, speed: number, deltaSeconds: number): Vector3 {
    const remaining = distance(current, target)
    if (remaining === 0 || remaining <= speed * deltaSeconds) return { ...target }
    const ratio = (speed * deltaSeconds) / remaining
    return {
      x: current.x + (target.x - current.x) * ratio,
      y: current.y + (target.y - current.y) * ratio,
      z: current.z + (target.z - current.z) * ratio
    }
  }
}
