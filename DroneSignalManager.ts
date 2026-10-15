import { Vector3, clamp01, distance } from '../../core/types.js'

export type SignalState = 'STRONG' | 'STABLE' | 'DEGRADED' | 'CRITICAL' | 'LOST'

export class DroneSignalManager {
  calculate(dronePosition: Vector3, relayPosition: Vector3, maxRange: number, interference = 0): number {
    const rangeFactor = 1 - distance(dronePosition, relayPosition) / Math.max(maxRange, 1)
    return clamp01(rangeFactor - clamp01(interference))
  }

  classify(signal: number): SignalState {
    if (signal <= 0) return 'LOST'
    if (signal < 0.15) return 'CRITICAL'
    if (signal < 0.4) return 'DEGRADED'
    if (signal < 0.75) return 'STABLE'
    return 'STRONG'
  }
}
