import { AegisSupportController } from '../../support/AegisSupportController.js'
import { DroneController } from '../../drones/DroneController.js'
import { DroneTutorial } from './DroneTutorial.js'
import { RidgeStation } from './RidgeStation.js'
import { Counterattack } from './Counterattack.js'

export type Mission01State =
  | 'M01_INIT' | 'M01_BRIEFING' | 'M01_DEPLOY' | 'M01_DRONE_TUTORIAL'
  | 'M01_RECON' | 'M01_APPROACH' | 'M01_ECHO_ASSAULT' | 'M01_ECHO_SECURED'
  | 'M01_WARDEN_DEPLOYED' | 'M01_CONVOY' | 'M01_RIDGE_RECON'
  | 'M01_RIDGE_STATION_DISCOVERED' | 'M01_RIDGE_ASSAULT'
  | 'M01_RIDGE_STATION_CAPTURED' | 'M01_AEGIS_ONLINE'
  | 'M01_TARGET_DESIGNATION' | 'M01_COUNTERATTACK' | 'M01_FINAL_DRONE_DEFENSE'
  | 'M01_FINAL_HOLD' | 'M01_REGROUP' | 'M01_COMPLETE'

export class MountainShieldMission {
  state: Mission01State = 'M01_INIT'
  readonly tutorial = new DroneTutorial()
  readonly ridgeStation = new RidgeStation()
  readonly counterattack = new Counterattack()

  constructor(public readonly drones: DroneController, public readonly aegis: AegisSupportController) {}

  transition(next: Mission01State): void {
    this.state = next
    if (next === 'M01_RIDGE_STATION_CAPTURED') {
      this.ridgeStation.capture()
      this.aegis.setOnline(true)
    }
    if (next === 'M01_COUNTERATTACK') this.counterattack.beginPreparation()
    if (next === 'M01_COMPLETE') this.aegis.setOnline(false)
  }

  snapshot(): Record<string, unknown> {
    return {
      mission: 'Operation Mountain Shield', state: this.state,
      ridgeStation: this.ridgeStation.state, aegisState: this.aegis.state,
      counterattack: this.counterattack.state, drones: this.drones.list()
    }
  }
}
