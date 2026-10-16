import { DroneBase } from './DroneBase.js'
import { Vector3 } from '../../core/types.js'
export class WardenDrone extends DroneBase {
  constructor(id = 'warden-1') {
    super({
      id, displayName: 'Warden-1', role: 'SURVEILLANCE', maxSpeed: 22, sensorRange: 600,
      batteryDrainPerSecond: 0.0015, signalRange: 2200,
      availableCommands: ['LAUNCH', 'MOVE', 'ORBIT', 'OBSERVE', 'TRACK', 'RETURN'],
      fallbackBehavior: 'HOLD_POSITION'
    })
  }
  surveilArea(center: Vector3): boolean { return this.moveTo(center, 'SURVEIL_AREA') }
}
