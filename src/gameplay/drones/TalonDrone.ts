import { DroneBase } from './DroneBase.js'
export class TalonDrone extends DroneBase {
  private missionAuthorized = false
  constructor(id = 'talon-1') {
    super({
      id, displayName: 'Talon-1', role: 'PRECISION_SUPPORT', maxSpeed: 34, sensorRange: 300,
      batteryDrainPerSecond: 0.003, signalRange: 1800,
      availableCommands: ['DEPLOY', 'ABORT', 'RETURN'], fallbackBehavior: 'MISSION_DEFINED'
    })
  }
  authorizeMission(): void { this.missionAuthorized = true }
  abortMission(): void { this.missionAuthorized = false; this.returnHome() }
  get authorized(): boolean { return this.missionAuthorized }
}
