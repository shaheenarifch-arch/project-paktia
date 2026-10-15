import { DroneController } from '../drones/DroneController.js'
import { Vector3 } from '../../core/types.js'
export type CommandAction = 'SELECT' | 'CANCEL' | 'PAN' | 'ZOOM' | 'DESIGNATE' | 'AUTHORIZE' | 'ABORT' | 'MOVE' | 'ORBIT' | 'RETURN'
export class CommandManager {
  constructor(private readonly drones: DroneController) {}
  moveDrone(id: string, position: Vector3): boolean { return this.drones.get(id)?.moveTo(position) ?? false }
  returnDrone(id: string): boolean { return this.drones.get(id)?.returnHome() ?? false }
}
