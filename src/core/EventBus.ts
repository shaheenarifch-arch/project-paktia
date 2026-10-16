export type EventMap = Record<string, unknown>
export type EventHandler<T> = (payload: T) => void

export class EventBus<TEvents extends EventMap> {
  private readonly handlers = new Map<keyof TEvents, Set<EventHandler<unknown>>>()

  on<K extends keyof TEvents>(event: K, handler: EventHandler<TEvents[K]>): () => void {
    const set = this.handlers.get(event) ?? new Set<EventHandler<unknown>>()
    set.add(handler as EventHandler<unknown>)
    this.handlers.set(event, set)
    return () => set.delete(handler as EventHandler<unknown>)
  }

  emit<K extends keyof TEvents>(event: K, payload: TEvents[K]): void {
    this.handlers.get(event)?.forEach(handler => handler(payload))
  }
}
