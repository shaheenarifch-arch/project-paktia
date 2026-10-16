import { clamp01 } from '../../core/types.js'
export class DroneBattery {
  constructor(public charge = 1) {}
  consume(amount: number): number { this.charge = clamp01(this.charge - Math.max(0, amount)); return this.charge }
  recharge(amount: number): number { this.charge = clamp01(this.charge + Math.max(0, amount)); return this.charge }
  get low(): boolean { return this.charge <= 0.25 }
  get critical(): boolean { return this.charge <= 0.1 }
}
