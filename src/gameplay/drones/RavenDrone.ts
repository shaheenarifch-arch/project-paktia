import { DroneBase } from './DroneBase.js'
export class RavenDrone extends DroneBase {
  constructor(id = 'raven-1') {
    super({
      id, displayName: 'Raven-1', role: 'RECON', maxSpeed: 28, sensorRange: 450,
      batteryDrainPerSecond: 0.002, signalRange: 1600,
      availableCommands: ['LAUNCH', 'MOVE', 'OBSERVE', 'TRACK', 'MARK', 'RETURN'],
      fallbackBehavior: 'RETURN_HOME'
    })
  }
}
