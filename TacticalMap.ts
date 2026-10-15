import { UnitMarker } from './UnitMarker.js'
export class TacticalMap {
  private readonly markers = new Map<string, UnitMarker>()
  upsert(marker: UnitMarker): void { this.markers.set(marker.id, { ...marker, position: { ...marker.position } }) }
  remove(id: string): void { this.markers.delete(id) }
  get(id: string): UnitMarker | undefined { return this.markers.get(id) }
  all(): readonly UnitMarker[] { return [...this.markers.values()] }
}
