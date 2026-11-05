// Episode 07 rules of engagement. Harm to a civilian or to a surrendered
// soldier is a violation: it fails the objective that was active, lowers
// Hearts & Minds and, at the mission's limit, fails the operation.
// All numbers are game-balance values from data/missions/episode-07-city-of-freedom.json.
export type ViolationKind = 'CIVILIAN_HARM' | 'SURRENDERED_HARM'

export interface RoeRules {
  startingHeartsAndMinds: number
  civilianHarmPenalty: number
  surrenderedHarmPenalty: number
  failMissionAtViolations: number
  heartsAndMindsTarget: number
  fieldHospitalBonus: number
  aidDropBonus: number
}

export interface Violation { kind: ViolationKind; objective: string; district?: string | undefined }

export class RulesOfEngagement {
  active = false
  heartsAndMinds: number
  violations: Violation[] = []
  failedObjectives: string[] = []

  constructor(readonly rules: RoeRules) { this.heartsAndMinds = rules.startingHeartsAndMinds }

  get civilianHarm(): number { return this.violations.filter(v => v.kind === 'CIVILIAN_HARM').length }
  get surrenderedHarm(): number { return this.violations.filter(v => v.kind === 'SURRENDERED_HARM').length }
  get limitReached(): boolean { return this.violations.length >= this.rules.failMissionAtViolations }
  get targetMet(): boolean { return this.heartsAndMinds >= this.rules.heartsAndMindsTarget }

  /** Records a violation against the active objective. Returns false while ROE is not yet in force. */
  record(kind: ViolationKind, objective: string, district?: string): boolean {
    if (!this.active) return false
    this.violations.push({ kind, objective, district })
    if (!this.failedObjectives.includes(objective)) this.failedObjectives.push(objective)
    this.adjust(-(kind === 'CIVILIAN_HARM' ? this.rules.civilianHarmPenalty : this.rules.surrenderedHarmPenalty))
    return true
  }

  /** Good conduct (treating patients, delivering aid) raises Hearts & Minds. */
  reward(amount: number): void { this.adjust(Math.max(0, amount)) }

  private adjust(delta: number): void { this.heartsAndMinds = Math.min(100, Math.max(0, this.heartsAndMinds + delta)) }

  snapshot(): Record<string, unknown> {
    return { active: this.active, heartsAndMinds: this.heartsAndMinds, violations: this.violations.map(v => ({ ...v })), failedObjectives: [...this.failedObjectives] }
  }
  restore(data: Record<string, unknown>): void {
    this.active = data.active as boolean
    this.heartsAndMinds = data.heartsAndMinds as number
    this.violations = (data.violations as Violation[]).map(v => ({ ...v }))
    this.failedObjectives = [...(data.failedObjectives as string[])]
  }
}
