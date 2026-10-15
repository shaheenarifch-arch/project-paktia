import type { Vector3 } from '../../core/types.js'
import { HelicopterController } from './HelicopterController.js'
import { CrashSite } from './CrashSite.js'
import { CrewState } from './CrewState.js'
import { RecoveryZone } from './RecoveryZone.js'

export type CrashSequenceState =
  | 'INACTIVE'
  | 'AIRCRAFT_INBOUND'
  | 'DISTRESS_RECEIVED'
  | 'SIGNAL_LOST'
  | 'CRASH_CONFIRMED'
  | 'SEARCH_ACTIVE'
  | 'SITE_LOCATED'
  | 'SITE_SECURED'
  | 'CREW_RECOVERY'
  | 'RECOVERY_DEFENSE'
  | 'EVACUATION_COMPLETE'
  | 'ABANDONED'

export class HelicopterCrashController {
  state: CrashSequenceState = 'INACTIVE'

  readonly crashSite = new CrashSite()
  readonly recoveryZone = new RecoveryZone()

  readonly crew = new CrewState([
    {
      id: 'falcon-pilot',
      role: 'PILOT',
      condition: 'UNKNOWN',
      located: false,
      evacuated: false
    },
    {
      id: 'falcon-copilot',
      role: 'COPILOT',
      condition: 'UNKNOWN',
      located: false,
      evacuated: false
    },
    {
      id: 'falcon-chief',
      role: 'CREW_CHIEF',
      condition: 'UNKNOWN',
      located: false,
      evacuated: false
    }
  ])

  constructor(
    public readonly helicopter: HelicopterController
  ) {}

  beginInbound(position: Vector3): void {
    this.helicopter.beginInbound(position)
    this.state = 'AIRCRAFT_INBOUND'
  }

  triggerDistress(): void {
    if (this.state !== 'AIRCRAFT_INBOUND') return

    this.helicopter.damage.applyDamage({
      flightControl: 0.45,
      engine: 0.55,
      structure: 0.25
    })

    this.helicopter.reportDistress()
    this.state = 'DISTRESS_RECEIVED'
  }

  loseSignal(): void {
    if (this.state === 'DISTRESS_RECEIVED') {
      this.state = 'SIGNAL_LOST'
    }
  }

  registerCrash(position: Vector3): void {
    if (
      this.state !== 'DISTRESS_RECEIVED' &&
      this.state !== 'SIGNAL_LOST'
    ) {
      return
    }

    this.helicopter.registerCrash(position)
    this.state = 'CRASH_CONFIRMED'
  }

  beginSearch(): void {
    if (this.state !== 'CRASH_CONFIRMED') return

    this.crashSite.beginSearch()
    this.state = 'SEARCH_ACTIVE'
  }

  locateCrashSite(position: Vector3): void {
    if (this.state !== 'SEARCH_ACTIVE') return

    this.crashSite.locate(position)
    this.state = 'SITE_LOCATED'
  }

  secureCrashSite(): void {
    if (
      this.state !== 'SITE_LOCATED' &&
      this.state !== 'RECOVERY_DEFENSE'
    ) {
      return
    }

    this.crashSite.secure()
    this.state = 'SITE_SECURED'
  }

  beginCrewRecovery(): void {
    if (
      this.state !== 'SITE_SECURED' ||
      !this.crashSite.position
    ) {
      return
    }

    this.recoveryZone.designate(
      this.crashSite.position
    )

    this.recoveryZone.activate()
    this.state = 'CREW_RECOVERY'
  }

  startRecoveryDefense(): void {
    if (this.state !== 'CREW_RECOVERY') return

    this.crashSite.contest()
    this.recoveryZone.contest()

    this.state = 'RECOVERY_DEFENSE'
  }

  completeEvacuation(): boolean {
    if (!this.crew.allRecovered) {
      return false
    }

    this.crashSite.secure()
    this.crashSite.completeRecovery()

    this.recoveryZone.secure()
    this.recoveryZone.completeEvacuation()

    this.state = 'EVACUATION_COMPLETE'

    return true
  }

  abandon(): void {
    this.crashSite.abandon()
    this.state = 'ABANDONED'
  }

  snapshot(): Record<string, unknown> {
    return {
      sequence: this.state,
      aircraft: this.helicopter.callsign,
      aircraftState: this.helicopter.flightState,
      damage: this.helicopter.damage.snapshot(),
      crashSite: this.crashSite.state,
      recoveryZone: this.recoveryZone.state,
      crew: this.crew.all()
    }
  }
}
