export interface Vector3 { x: number; y: number; z: number }
export type EntityId = string
export type TimestampMs = number

export const distance = (a: Vector3, b: Vector3): number =>
  Math.hypot(a.x - b.x, a.y - b.y, a.z - b.z)

export const clamp01 = (value: number): number =>
  Math.max(0, Math.min(1, value))
