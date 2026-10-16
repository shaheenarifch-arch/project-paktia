import { BattlefieldContact, ContactState, ObservationSource } from './BattlefieldContact.js'
import { TimestampMs, Vector3, clamp01 } from '../../core/types.js'

export class ContactManager {
  private readonly contacts = new Map<string, BattlefieldContact>()
  upsert(input: {
    id: string; classification: string; position: Vector3; source: ObservationSource;
    time: TimestampMs; quality: number; state?: ContactState
  }): BattlefieldContact {
    const prior = this.contacts.get(input.id)
    const contact: BattlefieldContact = {
      id: input.id,
      classification: input.classification,
      state: input.state ?? prior?.state ?? 'DETECTED',
      worldPosition: { ...input.position },
      observationSource: input.source,
      lastObservedAt: input.time,
      trackingQuality: clamp01(input.quality),
      currentlyObserved: true,
      ...(prior?.isProtected !== undefined ? { isProtected: prior.isProtected } : {})
    }
    this.contacts.set(input.id, contact)
    return contact
  }
  get(id: string): BattlefieldContact | undefined { return this.contacts.get(id) }
  all(): readonly BattlefieldContact[] { return [...this.contacts.values()] }
  confirm(id: string): boolean {
    const contact = this.contacts.get(id); if (!contact) return false
    contact.state = 'CONFIRMED'; return true
  }
  loseTracking(id: string, time: TimestampMs): boolean {
    const contact = this.contacts.get(id); if (!contact) return false
    contact.currentlyObserved = false; contact.state = 'TRACKING_LOST'; contact.lastObservedAt = time
    return true
  }
  age(now: TimestampMs, lastKnownAfterMs = 3000): void {
    this.contacts.forEach(contact => {
      if (!contact.currentlyObserved && now - contact.lastObservedAt >= lastKnownAfterMs) contact.state = 'LAST_KNOWN'
    })
  }
}
