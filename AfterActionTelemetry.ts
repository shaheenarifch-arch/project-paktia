export type TelemetryMetric =
  | 'DRONE_LAUNCH' | 'DRONE_RECOVERY' | 'DRONE_LOSS' | 'CONTACT_DETECTED'
  | 'CONTACT_CONFIRMED' | 'TRACKING_INTERRUPTION' | 'SUPPORT_REQUEST'
  | 'SUPPORT_ABORT' | 'SUPPORT_BLOCKED' | 'SUPPORT_COMPLETE'
export interface TelemetryEvent { metric: TelemetryMetric; at: number; subjectId?: string; details?: Record<string, unknown> }
export class AfterActionTelemetry {
  private readonly events: TelemetryEvent[] = []
  record(event: TelemetryEvent): void { this.events.push(event) }
  all(): readonly TelemetryEvent[] { return [...this.events] }
  count(metric: TelemetryMetric): number { return this.events.filter(event => event.metric === metric).length }
}
