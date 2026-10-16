import { clamp01 } from '../../core/types.js'

export class DroneCamera {
  private zoom = 0
  private active = false
  setActive(active: boolean): void { this.active = active }
  setZoom(normalizedZoom: number): void { this.zoom = clamp01(normalizedZoom) }
  snapshot(): { active: boolean; zoom: number } { return { active: this.active, zoom: this.zoom } }
}
