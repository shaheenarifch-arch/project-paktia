export interface MatchResultJob {
  type: "match.result.process";
  matchId: string;
  rulesVersion: string;
}

export default {
  async queue(batch: MessageBatch<MatchResultJob>): Promise<void> {
    for (const message of batch.messages) {
      const job = message.body;

      if (job.type !== "match.result.process" || !job.matchId) {
        message.retry();
        continue;
      }

      // Production implementation must load the canonical signed result,
      // verify idempotency, update progression/economy, and write an audit event.
      message.ack();
    }
  }
};
