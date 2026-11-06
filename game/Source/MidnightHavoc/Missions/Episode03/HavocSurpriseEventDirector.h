#pragma once

// Ports: src/gameplay/missions/episode03/SurpriseEventDirector.ts
// One server-controlled surprise event per match, picked deterministically from a seed so a replay
// or a restart picks the same one. Effects are abstract gameplay modifiers that engine systems read.

#include "CoreMinimal.h"

/** Ports SurpriseModifiers. */
struct MIDNIGHTHAVOC_API FHavocSurpriseModifiers
{
    double DroneSensorRangeScale = 1.0;
    double RelayInterference = 0.0;
    TArray<FString> OpenedRoutes;
    TArray<FString> BlockedRoutes;
    double VisibilityScale = 1.0;
};

class MIDNIGHTHAVOC_API FHavocSurpriseEventDirector
{
public:
    /** True for the five event ids the game knows (sandstorm, communications_outage, tunnel_discovery, bridge_obstruction, nightfall). */
    static bool IsKnownEvent(const FString& Event);

    /**
     * Ports pickSurpriseEvent: FNV-1a over the seed picks an index into Events.
     * Returns false (OutError set) for an empty list or an unknown event id.
     */
    static bool Pick(const TArray<FString>& Events, const FString& Seed, FString& OutEvent, FString& OutError);

    /** Modifiers of one event id (neutral for an unknown id). */
    static FHavocSurpriseModifiers ModifiersFor(const FString& Event);

    void SetPlanned(const FString& Event) { Planned = Event; }
    const FString& GetPlanned() const { return Planned; }

    /** Fires the planned event once per match; returns it, or empty if it already fired. */
    FString Trigger();

    void End() { Active.Reset(); }

    /** Checkpoint restore. An unknown active id is dropped. */
    void Restore(bool bInFired, const FString& InActive);

    bool HasFired() const { return bFired; }
    bool IsActive() const { return !Active.IsEmpty(); }
    const FString& GetActive() const { return Active; }
    FHavocSurpriseModifiers GetModifiers() const { return ModifiersFor(Active); }

private:
    FString Planned;
    FString Active;
    bool bFired = false;
};
