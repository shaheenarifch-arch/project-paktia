import type { MissionOutcome } from '../missions/framework/MissionDefinition.js'
import type { MountainShieldMission } from '../missions/mission01/MountainShield.js'
import type { E02Carryover } from '../missions/episode02/SilentCorridor.js'

// The campaign: episodes in order, what each one hands to the next, and which
// are unlocked. An episode unlocks when the previous one ended in SUCCESS or
// PARTIAL; a FAILED episode is replayed from its last checkpoint.
export interface EpisodeEntry { episode: number; id: string; name: string }

export const CAMPAIGN: readonly EpisodeEntry[] = [
  { episode: 1, id: 'mission_01_mountain_shield', name: 'Operation Mountain Shield' },
  { episode: 2, id: 'episode_02_silent_corridor', name: 'Operation Silent Corridor' },
  { episode: 3, id: 'episode_03_broken_citadel', name: 'Operation Broken Citadel' }
]

export interface M01Carryover { episode: 1; outcome: MissionOutcome; crewRecovered: boolean }

export interface CampaignSave {
  results: Record<number, { outcome: MissionOutcome; carryover: unknown }>
}

export class Campaign {
  private results = new Map<number, { outcome: MissionOutcome; carryover: unknown }>()

  /** Mission 01 predates the data-driven framework; its result is read from the mission itself. */
  static carryoverFromMission01(mission: MountainShieldMission): M01Carryover {
    const crash = mission.helicopterCrash.controller
    const crewRecovered = crash.state === 'EVACUATION_COMPLETE' && crash.crew.recoveredCount === 3
    return { episode: 1, outcome: mission.state === 'M01_COMPLETE' ? (crewRecovered ? 'SUCCESS' : 'PARTIAL') : 'FAILED', crewRecovered }
  }

  record(episode: number, outcome: MissionOutcome, carryover: unknown): void {
    if (!CAMPAIGN.some(e => e.episode === episode)) throw new Error(`Unknown episode ${episode}`)
    const prior = this.results.get(episode)
    // A replay never downgrades a better earlier result.
    const rank: Record<MissionOutcome, number> = { FAILED: 0, PARTIAL: 1, SUCCESS: 2 }
    if (prior && rank[prior.outcome] > rank[outcome]) return
    this.results.set(episode, { outcome, carryover })
  }

  outcome(episode: number): MissionOutcome | undefined { return this.results.get(episode)?.outcome }
  carryover<T>(episode: number): T | undefined { return this.results.get(episode)?.carryover as T | undefined }

  isUnlocked(episode: number): boolean {
    if (episode === 1) return true
    const prev = this.results.get(episode - 1)?.outcome
    return prev === 'SUCCESS' || prev === 'PARTIAL'
  }

  /** The next episode to play: the first not yet passed. */
  next(): EpisodeEntry | undefined {
    return CAMPAIGN.find(e => !['SUCCESS', 'PARTIAL'].includes(this.results.get(e.episode)?.outcome ?? ''))
  }

  /** What Episode 02 needs from Mission 01. */
  crewRecoveredInMission01(): boolean { return this.carryover<M01Carryover>(1)?.crewRecovered ?? true }
  /** What Episode 03 needs from Episode 02. */
  episode02(): E02Carryover | undefined { return this.carryover<E02Carryover>(2) }

  save(): CampaignSave { return { results: Object.fromEntries(this.results) } }
  static load(save: CampaignSave): Campaign {
    const c = new Campaign()
    for (const [k, v] of Object.entries(save.results)) c.results.set(Number(k), v)
    return c
  }
}
