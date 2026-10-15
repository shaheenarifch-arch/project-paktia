export type CrewMemberCondition =
  | 'UNKNOWN'
  | 'OPERATIONAL'
  | 'INJURED'
  | 'CRITICAL'
  | 'RECOVERED'
  | 'LOST'

export interface CrewMember {
  id: string
  role: string
  condition: CrewMemberCondition
  located: boolean
  evacuated: boolean
}

export class CrewState {
  private readonly members = new Map<string, CrewMember>()

  constructor(initialCrew: readonly CrewMember[] = []) {
    initialCrew.forEach(member => {
      this.members.set(member.id, { ...member })
    })
  }

  locate(id: string, condition: CrewMemberCondition): boolean {
    const member = this.members.get(id)

    if (!member) return false

    member.located = true
    member.condition = condition

    return true
  }

  recover(id: string): boolean {
    const member = this.members.get(id)

    if (!member || !member.located || member.condition === 'LOST') {
      return false
    }

    member.condition = 'RECOVERED'
    member.evacuated = true

    return true
  }

  all(): readonly CrewMember[] {
    return [...this.members.values()].map(member => ({ ...member }))
  }

  get locatedCount(): number {
    return this.all().filter(member => member.located).length
  }

  get recoveredCount(): number {
    return this.all().filter(member => member.evacuated).length
  }

  get allRecovered(): boolean {
    const crew = this.all()

    return (
      crew.length > 0 &&
      crew.every(member => member.evacuated || member.condition === 'LOST')
    )
  }
}
