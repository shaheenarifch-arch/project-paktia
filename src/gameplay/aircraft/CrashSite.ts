import type { Vector3 } from '../../core/types.js'

export type CrashSiteState =
  | 'UNKNOWN'
  | 'SEARCHING'
  | 'LOCATED'
  | 'CONTESTED'
  | 'SECURED'
  | 'ABANDONED'
  | 'RECOVERY_COMPLETE'

export class CrashSite {
  state: CrashSiteState = 'UNKNOWN'
  position?: Vector3
  smokeVisible = false
  beaconActive = false

  beginSearch(): void {
    if (this.state === 'UNKNOWN') {
      this.state = 'SEARCHING'
    }
  }

  locate(position: Vector3): void {
    this.position = { ...position }
    this.state = 'LOCATED'
    this.smokeVisible = true
    this.beaconActive = true
  }

  contest(): void {
    if (this.state === 'LOCATED' || this.state === 'SECURED') {
      this.state = 'CONTESTED'
    }
  }

  secure(): void {
    if (
      this.state === 'LOCATED' ||
      this.state === 'CONTESTED'
    ) {
      this.state = 'SECURED'
    }
  }

  abandon(): void {
    if (this.state !== 'RECOVERY_COMPLETE') {
      this.state = 'ABANDONED'
      this.beaconActive = false
    }
  }

  completeRecovery(): void {
    if (this.state === 'SECURED') {
      this.state = 'RECOVERY_COMPLETE'
      this.beaconActive = false
    }
  }
}
