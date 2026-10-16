import { ContactManager } from '../intelligence/ContactManager.js'
import { ProtectedZone } from './ProtectedZone.js'
import { SupportRequest, SupportState } from './SupportRequest.js'
import { TargetValidator, ValidationResult } from './TargetValidator.js'

export class AegisSupportController {
  state: SupportState = 'OFFLINE'
  private selectedTargetId: string | undefined
  private activeRequest: SupportRequest | undefined
  private zones: ProtectedZone[] = []

  constructor(private readonly contacts: ContactManager, private readonly validator = new TargetValidator()) {}
  setProtectedZones(zones: readonly ProtectedZone[]): void { this.zones = [...zones] }
  setOnline(online: boolean): void { this.state = online ? 'AVAILABLE' : 'OFFLINE'; if (!online) this.selectedTargetId = undefined }
  selectTarget(targetId: string): ValidationResult {
    const result = this.validator.validate(this.contacts.get(targetId), this.state !== 'OFFLINE', this.zones)
    if (result.valid) { this.selectedTargetId = targetId; this.state = 'TARGET_SELECTED' }
    return result
  }
  validateSelection(): ValidationResult {
    this.state = 'VALIDATING'
    const result = this.validator.validate(this.selectedTargetId ? this.contacts.get(this.selectedTargetId) : undefined, true, this.zones)
    this.state = result.valid ? 'READY' : 'AVAILABLE'
    return result
  }
  authorize(now: number): SupportRequest | undefined {
    if (this.state !== 'READY' || !this.selectedTargetId) return undefined
    this.activeRequest = { id: `support-${now}`, targetId: this.selectedTargetId, state: 'AUTHORIZED', requestedAt: now, authorizedAt: now }
    this.state = 'AUTHORIZED'
    return this.activeRequest
  }
  commit(): boolean { if (!this.activeRequest || this.state !== 'AUTHORIZED') return false; this.activeRequest.state = 'COMMITTED'; this.state = 'COMMITTED'; return true }
  complete(now: number): boolean { if (!this.activeRequest || this.state !== 'COMMITTED') return false; this.activeRequest.state = 'COMPLETE'; this.activeRequest.completedAt = now; this.state = 'COMPLETE'; return true }
  abort(): boolean { if (!['TARGET_SELECTED','VALIDATING','READY','AUTHORIZED'].includes(this.state)) return false; if (this.activeRequest) this.activeRequest.state = 'ABORTED'; this.state = 'ABORTED'; return true }
  request(): SupportRequest | undefined { return this.activeRequest }
}
