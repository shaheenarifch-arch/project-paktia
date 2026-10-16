export type SupportState = 'OFFLINE' | 'AVAILABLE' | 'TARGET_SELECTED' | 'VALIDATING' | 'READY' | 'AUTHORIZED' | 'COMMITTED' | 'COMPLETE' | 'ABORTED'
export interface SupportRequest {
  id: string
  targetId: string
  state: SupportState
  requestedAt: number
  authorizedAt?: number
  completedAt?: number
}
