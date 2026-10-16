import { ContactManager } from './ContactManager.js'
import { TimestampMs, Vector3 } from '../../core/types.js'
export class ContactTracker {
  constructor(private readonly contacts: ContactManager) {}
  update(id: string, position: Vector3, time: TimestampMs, quality: number): boolean {
    const current = this.contacts.get(id); if (!current) return false
    this.contacts.upsert({ id, classification: current.classification, position, source: current.observationSource, time, quality, state: current.state })
    return true
  }
}
