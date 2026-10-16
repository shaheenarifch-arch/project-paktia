import { Vector3 } from '../../core/types.js'
export interface UnitMarker { id: string; label: string; kind: 'FRIENDLY' | 'CONTACT' | 'DRONE' | 'OBJECTIVE' | 'SUPPORT'; position: Vector3; stale?: boolean }
