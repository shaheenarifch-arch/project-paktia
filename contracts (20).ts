export type GameMode = "stronghold_assault" | "training";

export interface MatchmakingRequest {
  playerId: string;
  region: string;
  mode: GameMode;
  partyId?: string;
  inputMethod: "keyboard_mouse" | "controller";
}

export interface AllocatedServer {
  matchId: string;
  region: string;
  endpoint: string;
  joinToken: string;
  expiresAt: string;
}

export interface GameServerAllocator {
  allocateMatch(request: MatchmakingRequest): Promise<AllocatedServer>;
  terminateMatch(matchId: string): Promise<void>;
}
