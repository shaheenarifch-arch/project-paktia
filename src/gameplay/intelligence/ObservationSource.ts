export interface ObservationSourceStatus {
  id: string
  kind: 'PLAYER' | 'DRONE' | 'SQUAD' | 'MISSION_INTELLIGENCE'
  available: boolean
  quality: number
}
