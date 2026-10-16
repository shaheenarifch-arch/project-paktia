export type CurrencyCode = "CREDITS" | "COMMAND_TOKENS" | "PREMIUM_TOKENS";

export interface TokenTransactionInput {
  playerId: string;
  currencyCode: CurrencyCode;
  amountMinor: number;
  transactionType: "earn" | "spend" | "purchase" | "refund" | "adjustment";
  referenceType: string;
  referenceId: string;
  idempotencyKey: string;
}

export const assertCosmeticOnlyPurchase = (catalogItem: {
  combatStatChange?: boolean;
}) => {
  if (catalogItem.combatStatChange) {
    throw new Error("Premium currency cannot purchase combat advantages");
  }
};
