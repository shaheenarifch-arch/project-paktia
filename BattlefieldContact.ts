import { EntityId, TimestampMs, Vector3 } from '../../core/types.js'
export type ContactState = 'UNKNOWN' | 'DETECTED' | 'IDENTIFIED' | 'CONFIRMED' | 'TRACKING_LOST' | 'LAST_KNOWN'
export type ObservationSource = 'PLAYER' | 'RAVEN' | 'WARDEN' | 'VIPER_SQUAD' | 'BRAVO_SQUAD' | 'MISSION_INTELLIGENCE'
export interface BattlefieldContact {
  id: EntityId
  classification: string
  state: ContactState
  worldPosition: Vector3
  observationSource: ObservationSource
  lastObservedAt: TimestampMs
  trackingQuality: number
  currentlyObserved: boolean
  isProtected?: boolean
}
