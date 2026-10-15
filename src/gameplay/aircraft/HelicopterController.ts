import type { Vector3 } from '../../core/types.js'
import {
  HelicopterDamageState
} from './HelicopterDamageState.js'

export type HelicopterFlightState =
  | 'PARKED'
  | 'INBOUND'
  | 'TRANSIT'
  | 'DISTRESS'
  | 'EMERGENCY_LANDING'
  | 'CRASHED'
  | 'RECOVERED'

export class HelicopterController {
  flightState: HelicopterFlightState = 'PARKED'
  position: Vector3 = { x: 0, y: 0, z: 0 }
  lastKnownPosition: Vector3 = { x: 0, y: 0, z: 0 }

  readonly damage = new HelicopterDamageState()

  constructor(
    public readonly id: string,
    public readonly callsign: string
  ) {}

  beginInbound(position: Vector3): void {
    this.position = { ...position }
    this.lastKnownPosition = { ...position }
    this.flightState = 'INBOUND'
  }

  updatePosition(position: Vector3): void {
    if (this.flightState === 'CRASHED') return

    this.position = { ...position }
    this.lastKnownPosition = { ...position }

    if (this.flightState === 'INBOUND') {
      this.flightState = 'TRANSIT'
    }
  }

  reportDistress(): void {
    if (
      this.flightState === 'INBOUND' ||
      this.flightState === 'TRANSIT'
    ) {
      this.flightState = 'DISTRESS'
    }
  }

  attemptEmergencyLanding(position: Vector3): void {
    this.position = { ...position }
    this.lastKnownPosition = { ...position }
    this.flightState = 'EMERGENCY_LANDING'
  }

  registerCrash(position: Vector3): void {
    this.position = { ...position }
    this.lastKnownPosition = { ...position }
    this.flightState = 'CRASHED'
  }

  recoverAircraft(): void {
    if (this.flightState === 'CRASHED') {
      this.flightState = 'RECOVERED'
    }
  }
}
