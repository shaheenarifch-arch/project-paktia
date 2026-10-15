export type HelicopterDamageLevel =
  | 'OPERATIONAL'
  | 'DAMAGED'
  | 'CRITICAL'
  | 'DISABLED'
  | 'DESTROYED'

export interface HelicopterDamageSnapshot {
  level: HelicopterDamageLevel
  flightControl: number
  engineCondition: number
  structuralCondition: number
}

export class HelicopterDamageState {
  private flightControl = 1
  private engineCondition = 1
  private structuralCondition = 1

  applyDamage(input: {
    flightControl?: number
    engine?: number
    structure?: number
  }): void {
    this.flightControl = this.reduce(
      this.flightControl,
      input.flightControl ?? 0
    )

    this.engineCondition = this.reduce(
      this.engineCondition,
      input.engine ?? 0
    )

    this.structuralCondition = this.reduce(
      this.structuralCondition,
      input.structure ?? 0
    )
  }

  snapshot(): HelicopterDamageSnapshot {
    return {
      level: this.getLevel(),
      flightControl: this.flightControl,
      engineCondition: this.engineCondition,
      structuralCondition: this.structuralCondition
    }
  }

  getLevel(): HelicopterDamageLevel {
    const lowest = Math.min(
      this.flightControl,
      this.engineCondition,
      this.structuralCondition
    )

    if (lowest <= 0) return 'DESTROYED'
    if (lowest <= 0.15) return 'DISABLED'
    if (lowest <= 0.35) return 'CRITICAL'
    if (lowest <= 0.7) return 'DAMAGED'

    return 'OPERATIONAL'
  }

  private reduce(current: number, amount: number): number {
    return Math.max(0, Math.min(1, current - Math.max(0, amount)))
  }
}
