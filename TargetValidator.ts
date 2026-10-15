import { BattlefieldContact } from '../intelligence/BattlefieldContact.js'
import { ProtectedZone } from './ProtectedZone.js'
export type ValidationResult =
  | { valid: true }
  | { valid: false; reason: 'NO_TARGET' | 'TARGET_NOT_CONFIRMED' | 'TRACKING_REQUIRED' | 'PROTECTED_ZONE' | 'SUPPORT_UNAVAILABLE' }

export class TargetValidator {
  validate(target: BattlefieldContact | undefined, supportAvailable: boolean, zones: readonly ProtectedZone[]): ValidationResult {
    if (!target) return { valid: false, reason: 'NO_TARGET' }
    if (!supportAvailable) return { valid: false, reason: 'SUPPORT_UNAVAILABLE' }
    if (target.state !== 'CONFIRMED') return { valid: false, reason: 'TARGET_NOT_CONFIRMED' }
    if (!target.currentlyObserved) return { valid: false, reason: 'TRACKING_REQUIRED' }
    if (target.isProtected || zones.some(zone => zone.contains(target.worldPosition))) return { valid: false, reason: 'PROTECTED_ZONE' }
    return { valid: true }
  }
}
