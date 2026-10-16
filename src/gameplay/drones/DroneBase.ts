import { clamp01, Vector3 } from '../../core/types.js'

export type DroneRole = 'RECON' | 'SURVEILLANCE' | 'PRECISION_SUPPORT'
export type DroneState =
  | 'STOWED' | 'LAUNCHING' | 'ACTIVE' | 'OBSERVING' | 'TRACKING'
  | 'MOVING' | 'ORBITING' | 'RETURNING' | 'RECOVERED'
  | 'SIGNAL_DEGRADED' | 'SIGNAL_LOST' | 'DISABLED' | 'DESTROYED'
export type LostLinkBehavior = 'HOLD_POSITION' | 'RETURN_HOME' | 'LAND' | 'MISSION_DEFINED'

export interface DroneDefinition {
  id: string
  displayName: string
  role: DroneRole
  maxSpeed: number
  sensorRange: number
  batteryDrainPerSecond: number
  signalRange: number
  availableCommands: readonly string[]
  fallbackBehavior: LostLinkBehavior
}

export interface DroneTelemetry {
  id: string
  displayName: string
  role: DroneRole
  state: DroneState
  position: Vector3
  battery: number
  signal: number
  task: string | undefined
}

export abstract class DroneBase {
  state: DroneState = 'STOWED'
  position: Vector3 = { x: 0, y: 0, z: 0 }
  home: Vector3 = { x: 0, y: 0, z: 0 }
  battery = 1
  signal = 1
  task?: string

  constructor(public readonly definition: DroneDefinition) {}

  launch(home: Vector3): boolean {
    if (!['STOWED', 'RECOVERED'].includes(this.state)) return false
    this.home = { ...home }
    this.position = { ...home }
    this.state = 'LAUNCHING'
    return true
  }

  activate(): void {
    if (this.state === 'LAUNCHING') this.state = 'ACTIVE'
  }

  moveTo(position: Vector3, task = 'MOVE'): boolean {
    if (!this.isControllable()) return false
    this.position = { ...position }
    this.task = task
    this.state = 'MOVING'
    return true
  }

  returnHome(): boolean {
    if (['DESTROYED', 'DISABLED', 'STOWED', 'RECOVERED'].includes(this.state)) return false
    this.task = 'RETURN_HOME'
    this.state = 'RETURNING'
    return true
  }

  recover(): void {
    if (this.state !== 'RETURNING') return
    this.position = { ...this.home }
    this.task = undefined
    this.state = 'RECOVERED'
  }

  applyDamage(amount: number): void {
    if (this.state === 'DESTROYED') return
    if (amount >= 1) this.state = 'DESTROYED'
    else if (amount >= 0.5) this.state = 'DISABLED'
  }

  update(deltaSeconds: number, signalStrength: number): void {
    if (!this.isAirborne()) return
    this.battery = clamp01(this.battery - this.definition.batteryDrainPerSecond * deltaSeconds)
    this.signal = clamp01(signalStrength)

    if (this.signal <= 0) this.handleSignalLost()
    else if (this.signal < 0.3 && !['RETURNING', 'DESTROYED'].includes(this.state)) {
      this.state = 'SIGNAL_DEGRADED'
    }

    if (this.battery <= 0.1 && this.isControllable()) this.returnHome()
  }

  telemetry(): DroneTelemetry {
    const base: DroneTelemetry = {
      id: this.definition.id,
      displayName: this.definition.displayName,
      role: this.definition.role,
      state: this.state,
      position: { ...this.position },
      battery: this.battery,
      signal: this.signal
    }
    return this.task ? { ...base, task: this.task } : base
  }

  protected isControllable(): boolean {
    return !['STOWED', 'RECOVERED', 'DISABLED', 'DESTROYED', 'SIGNAL_LOST'].includes(this.state)
  }

  protected isAirborne(): boolean {
    return !['STOWED', 'RECOVERED', 'DESTROYED'].includes(this.state)
  }

  protected handleSignalLost(): void {
    this.state = 'SIGNAL_LOST'
    switch (this.definition.fallbackBehavior) {
      case 'RETURN_HOME': this.state = 'RETURNING'; this.task = 'LOST_LINK_RETURN'; break
      case 'LAND': this.state = 'DISABLED'; this.task = 'LAND'; break
      case 'HOLD_POSITION': this.task = 'HOLD'; break
      case 'MISSION_DEFINED': this.task = 'AWAIT_MISSION_RULE'; break
    }
  }
}
