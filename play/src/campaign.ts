import { Campaign, type CampaignSave, type E02Carryover, type MissionOutcome } from '../../src/index.js'

// Campaign progress lives in this browser only (the game server will own it later).
const KEY = 'paktia.campaign.v1'
const SETTINGS = 'paktia.settings.v1'

export function loadCampaign(): Campaign {
  try {
    const raw = localStorage.getItem(KEY)
    if (raw) return Campaign.load(JSON.parse(raw) as CampaignSave)
  } catch { /* storage blocked or corrupt: start fresh */ }
  return new Campaign()
}

export function saveCampaign(c: Campaign): void {
  try { localStorage.setItem(KEY, JSON.stringify(c.save())) } catch { /* storage unavailable */ }
}

export function resetCampaign(): Campaign {
  try { localStorage.removeItem(KEY) } catch { /* storage unavailable */ }
  return new Campaign()
}

/** Episode 01 is not playable in the browser yet, so the player states how it ended. */
export function loadCrewRecovered(): boolean {
  try { return localStorage.getItem(SETTINGS) !== 'crew-lost' } catch { return true }
}
export function saveCrewRecovered(v: boolean): void {
  try { localStorage.setItem(SETTINGS, v ? 'crew-recovered' : 'crew-lost') } catch { /* storage unavailable */ }
}

export function recordEpisode02(c: Campaign, outcome: MissionOutcome, carryover: E02Carryover, crewRecovered: boolean): void {
  // Episode 01's result is recorded first so the campaign order holds.
  if (!c.outcome(1)) c.record(1, crewRecovered ? 'SUCCESS' : 'PARTIAL', { episode: 1, outcome: crewRecovered ? 'SUCCESS' : 'PARTIAL', crewRecovered })
  c.record(2, outcome, carryover)
  saveCampaign(c)
}

export function recordEpisode03(c: Campaign, outcome: MissionOutcome): void {
  c.record(3, outcome, { episode: 3, outcome })
  saveCampaign(c)
}
