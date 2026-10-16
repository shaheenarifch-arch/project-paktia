import { DroneBase, DroneTelemetry } from './DroneBase.js'
import { Vector3 } from '../../core/types.js'

export class DroneController {
  private readonly drones = new Map<string, DroneBase>()

  register(drone: DroneBase): void {
    if (this.drones.has(drone.definition.id)) throw new Error(`Duplicate drone ${drone.definition.id}`)
    this.drones.set(drone.definition.id, drone)
  }

  get(id: string): DroneBase | undefined { return this.drones.get(id) }
  list(): readonly DroneTelemetry[] { return [...this.drones.values()].map(d => d.telemetry()) }

  launch(id: string, home: Vector3): boolean {
    const drone = this.drones.get(id)
    if (!drone) return false
    const launched = drone.launch(home)
    if (launched) drone.activate()
    return launched
  }

  update(deltaSeconds: number, getSignal: (drone: DroneBase) => number): void {
    this.drones.forEach(drone => drone.update(deltaSeconds, getSignal(drone)))
  }
}
