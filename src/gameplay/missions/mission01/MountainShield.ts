import { AegisSupportController } from '../../support/AegisSupportController.js'
import { DroneController } from '../../drones/DroneController.js'
import { DroneTutorial } from './DroneTutorial.js'
import { RidgeStation } from './RidgeStation.js'
import { Counterattack } from './Counterattack.js'
import { HelicopterCrashEvent } from './HelicopterCrashEvent.js'

export type Mission01State =
  | 'M01_INIT' | 'M01_BRIEFING' | 'M01_DEPLOY' | 'M01_DRONE_TUTORIAL'
  | 'M01_RECON' | 'M01_APPROACH' | 'M01_ECHO_ASSAULT' | 'M01_ECHO_SECURED'
  | 'M01_WARDEN_DEPLOYED' | 'M01_CONVOY' | 'M01_RIDGE_RECON'
  | 'M01_RIDGE_STATION_DISCOVERED' | 'M01_RIDGE_ASSAULT'
  | 'M01_RIDGE_STATION_CAPTURED' | 'M01_AEGIS_ONLINE' | 'M01_TARGET_DESIGNATION'
  | 'M01_COUNTERATTACK' | 'M01_HELICOPTER_INBOUND' | 'M01_HELICOPTER_DISTRESS'
  | 'M01_HELICOPTER_SIGNAL_LOST' | 'M01_HELICOPTER_CRASH' | 'M01_CRASH_SITE_RECON'
  | 'M01_CRASH_SITE_LOCATED' | 'M01_CRASH_SITE_SECURE' | 'M01_CREW_RECOVERY'
  | 'M01_RECOVERY_DEFENSE' | 'M01_EVAC_COMPLETE' | 'M01_FINAL_DRONE_DEFENSE'
  | 'M01_FINAL_HOLD' | 'M01_REGROUP' | 'M01_COMPLETE'

export class MountainShieldMission {
  state: Mission01State = 'M01_INIT'
  readonly tutorial = new DroneTutorial()
  readonly ridgeStation = new RidgeStation()
  readonly counterattack = new Counterattack()
  readonly helicopterCrash = new HelicopterCrashEvent()

  constructor(public readonly drones: DroneController, public readonly aegis: AegisSupportController) {}

  transition(next: Mission01State): void {
    this.state = next
    switch (next) {
      case 'M01_RIDGE_STATION_CAPTURED': this.ridgeStation.capture(); this.aegis.setOnline(true); break
      case 'M01_COUNTERATTACK': this.counterattack.beginPreparation(); break
      case 'M01_HELICOPTER_INBOUND': this.helicopterCrash.start(); break
      case 'M01_HELICOPTER_DISTRESS': this.helicopterCrash.triggerDistress(); break
      case 'M01_HELICOPTER_SIGNAL_LOST': this.helicopterCrash.loseSignal(); break
      case 'M01_HELICOPTER_CRASH': this.helicopterCrash.triggerCrash(); break
      case 'M01_CRASH_SITE_RECON': this.helicopterCrash.beginRecon(); break
      case 'M01_CRASH_SITE_LOCATED': this.helicopterCrash.confirmCrashSite(); break
      case 'M01_CRASH_SITE_SECURE': this.helicopterCrash.secureSite(); break
      case 'M01_CREW_RECOVERY': this.helicopterCrash.beginRecovery(); break
      case 'M01_RECOVERY_DEFENSE': this.helicopterCrash.beginDefense(); break
      case 'M01_EVAC_COMPLETE': this.helicopterCrash.evacuateCrew(); break
      case 'M01_COMPLETE': this.aegis.setOnline(false); break
    }
  }

  continueCrashSequence(): Mission01State {
    const next: Partial<Record<Mission01State, Mission01State>> = {
      M01_COUNTERATTACK: 'M01_HELICOPTER_INBOUND',
      M01_HELICOPTER_INBOUND: 'M01_HELICOPTER_DISTRESS',
      M01_HELICOPTER_DISTRESS: 'M01_HELICOPTER_SIGNAL_LOST',
      M01_HELICOPTER_SIGNAL_LOST: 'M01_HELICOPTER_CRASH',
      M01_HELICOPTER_CRASH: 'M01_CRASH_SITE_RECON',
      M01_CRASH_SITE_RECON: 'M01_CRASH_SITE_LOCATED',
      M01_CRASH_SITE_LOCATED: 'M01_CRASH_SITE_SECURE',
      M01_CRASH_SITE_SECURE: 'M01_CREW_RECOVERY',
      M01_CREW_RECOVERY: 'M01_RECOVERY_DEFENSE',
      M01_RECOVERY_DEFENSE: 'M01_EVAC_COMPLETE',
      M01_EVAC_COMPLETE: 'M01_FINAL_DRONE_DEFENSE'
    }
    const target = next[this.state]
    if (target) this.transition(target)
    return this.state
  }

  abandonCrashRecovery(): void {
    this.helicopterCrash.controller.abandon()
    this.transition('M01_FINAL_DRONE_DEFENSE')
  }

  snapshot(): Record<string, unknown> {
    return {
      mission: 'Operation Mountain Shield',
      state: this.state,
      ridgeStation: this.ridgeStation.state,
      aegisState: this.aegis.state,
      counterattack: this.counterattack.state,
      helicopterCrash: this.helicopterCrash.controller.snapshot(),
      drones: this.drones.list()
    }
  }
}
