#pragma once

// Runs the AI squads of a level: spawns them, shares threats inside a squad, and moves the whole squad
// through Patrol -> Engage -> Fallback -> Patrol (the rules are in HavocSquadLogic.h and are unit tested).
// Tuning and the per-phase deployments come from data/ai/squad-behaviour.json.
//
// Phases: AHavocLevelDirector calls ApplyPhase(<mission state>) whenever the mission state changes; the
// deployments listed for that state are spawned (allied squads in the shipped data) and the ones from
// the previous phase are removed. Squads spawned elsewhere (e.g. the Episode 04 garrisons) join with
// RegisterSquad. Publishes "squad:mode" {"squad","team","sector","mode"} on UHavocEventSubsystem.
// Server only; clients never tick squads. Guide: game/docs/ai.md

#include "CoreMinimal.h"
#include "AI/HavocSquadLogic.h"
#include "Subsystems/WorldSubsystem.h"
#include "HavocSquadSubsystem.generated.h"

class AActor;
class AHavocEnemyCharacter;

UCLASS()
class MIDNIGHTHAVOC_API UHavocSquadSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;

    /** Tuning from data/ai/squad-behaviour.json (defaults if the file is missing). */
    const HavocSquad::FTuning& GetTuning() const;

    /** Adds already-spawned units as a squad. Returns the squad id. Sets SquadId on every member. */
    int32 RegisterSquad(EHavocTeam Team, FName SectorId, EHavocSquadPosture Posture, const TArray<AHavocEnemyCharacter*>& Members, bool bOwnedByPhase = false);

    /** Spawns one squad in the sector volume. Returns the squad id, or -1 if nothing could be placed. */
    int32 SpawnDeployment(const HavocSquad::FDeployment& Deployment, bool bOwnedByPhase = false);

    /** Removes the previous phase's squads and spawns the deployments for State. Returns how many squads were spawned. */
    int32 ApplyPhase(const FString& State);

    /** A member saw or was shot by Threat: the squad treats it as known. */
    void ReportThreat(int32 SquadId, AActor* Threat);

    int32 GetSquadCount() const { return Squads.Num(); }
    EHavocSquadMode GetSquadMode(int32 SquadId) const;
    int32 GetAliveCount(int32 SquadId) const;
    const FString& GetCurrentPhase() const { return CurrentPhase; }

private:
    struct FRuntimeSquad
    {
        int32 Id = 0;
        EHavocTeam Team = EHavocTeam::Opposing;
        FName Sector;
        HavocSquad::FSquadModel Model;
        TArray<TWeakObjectPtr<AHavocEnemyCharacter>> Members;
        TWeakObjectPtr<AActor> Threat;
        bool bOwnedByPhase = false;
    };

    void EnsureConfig() const;
    int32 CountAlive(const FRuntimeSquad& Squad) const;
    void PushMode(FRuntimeSquad& Squad) const;
    void Publish(const FRuntimeSquad& Squad) const;
    FRuntimeSquad* Find(int32 SquadId);
    const FRuntimeSquad* Find(int32 SquadId) const;
    void RemovePhaseSquads();
    bool FindLandSpot(FName SectorId, FVector& OutSpot) const;

    mutable bool bConfigLoaded = false;
    mutable HavocSquad::FConfig Config;
    TArray<FRuntimeSquad> Squads;
    int32 NextSquadId = 1;
    float TickAccumulator = 0.f;
    FString CurrentPhase;
};
