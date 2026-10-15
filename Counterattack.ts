export type CounterattackState = 'INACTIVE' | 'PREPARING' | 'ACTIVE' | 'REPELLED' | 'CHECKPOINT_LOST'
export class Counterattack {
  state: CounterattackState = 'INACTIVE'
  beginPreparation(): void { this.state = 'PREPARING' }
  begin(): void { if (this.state === 'PREPARING') this.state = 'ACTIVE' }
  repel(): void { if (this.state === 'ACTIVE') this.state = 'REPELLED' }
  loseCheckpoint(): void { if (this.state === 'ACTIVE') this.state = 'CHECKPOINT_LOST' }
}
