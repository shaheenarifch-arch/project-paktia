#pragma once

// The playable loop of Episode 03, "Operation Broken Citadel". One per level, spawned by
// AHavocPlayableGameMode when the session plays Episode 03 (see the game mode). UHavocBrokenCitadelMission
// (data/missions/episode-03-broken-citadel.json + operation-broken-citadel.json) owns the rules, the five
// phases, the unlocks, the surprise event, reinforcements and the checkpoints; this actor turns the player's
// actions into the mission's world inputs, phase by phase:
//
//   E03_BRIEFING / E03_STAGING   StartEpisode -> BeginAssault (Marine at South Gate)
//   E03_PHASE_1..5               each open objective has a marked point with a garrison; clear the garrison
//                                and hold the point to CaptureObjective. Phase 1 takes either objective,
//                                the other phases need all of theirs.
//   unlocks                      Falcon air support (airfield + crew from Episode 02, G), enemy comms degraded
//                                (counterattacks at 60%), Aegis online (Mountain Fort), medical station protected
//   surprise event               seeded, fires once on entering the surprise phase, ends with it
//   E03_CITADEL_SECURED          SUCCESS; reinforcements at zero fails to E03_ASSAULT_FAILED and the last checkpoint
//
// Greybox: if the level has no AHavocSectorVolume with Gardun sector ids, BeginPlay builds a greybox Gardun
// (floor, ten sector volumes, objective pillars) at GreyboxOriginCm, above the level so it works in any map.
// Falcon support is an abstract strike (no pawn to fly). Aegis is abstract. Abstract game rules only.
// Guide: game/docs/episode-03-playtest-checklist.md. Help:
//   https://dev.epicgames.com/documentation/en-us/unreal-engine/actors-in-unreal-engine

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Missions/Episode04/HavocEpisode04Flow.h"
#include "HavocEpisode03Flow.generated.h"

class AHavocEnemyCharacter;
class AHavocPlayerCharacter;
class AHavocPlayerController;
class AHavocSectorVolume;
class UHavocBrokenCitadelMission;

UCLASS()
class MIDNIGHTHAVOC_API AHavocEpisode03Flow : public AActor
{
    GENERATED_BODY()

public:
    AHavocEpisode03Flow();

    virtual void Tick(float DeltaSeconds) override;

    // ---- Tuning (abstract game balance; EditAnywhere so a Blueprint child can retune) ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") FName MissionId;
    /** Enemies guarding each objective point (the protected medical station has none). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 GarrisonPerObjective = 3;
    /** Extra defenders per objective in the Citadel phase. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 CitadelExtraGarrison = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float CaptureRadiusCm = 3000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float CaptureSeconds = 6.f;
    /** Counterattack waves from phase 2 on. The wave size is scaled by the mission's counterattack strength (0.6 once comms are broken). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float WaveIntervalSeconds = 40.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 WaveSize = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 MaxRaidersAlive = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float FalconStrikeRadiusCm = 6000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float FalconCooldownSeconds = 45.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 DeathReinforcementCost = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 ResupplyCharges = 4;
    /** data/units factions the garrisons draw infantry stats from. Gardun has no faction data of its own yet; see the PR notes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") TArray<FString> EnemyFactions;
    /** Greybox Gardun: south-west corner of the 2.5 km core. Game-space metres (x east, z north) map to this origin; the protected-zone check uses the same frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") FVector GreyboxOriginCm = FVector(0.f, 0.f, 300000.f);

    // ---- Player actions (called by AHavocPlayerController / the menu) ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") void StartEpisode();
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") void RestartFromCheckpoint();
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") void HandleInteract();
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") void CallFalcon();

    // ---- Queries for the HUD and menus ----
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") EHavocFlowPhase GetPhase() const { return Phase; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetHint() const { return Hint; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetToast() const { return ToastTime > 0.f ? Toast : FString(); }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetSupportStatus() const;
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetResultSummary() const { return ResultSummary; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetOutcome() const { return Outcome; }
    /** Objective line for the HUD (the mission's current state objective). Empty before the episode starts. */
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetObjectiveText() const;
    /** Multi-line status for the HUD: phase, reinforcements, open objectives, unlocks, surprise event. */
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") FString GetStatusBlock() const;

    /** Pure helpers (tested). */
    /** Sector id an objective is fought in (docs/gardun-map.md); NAME_None for an unknown objective. */
    static FName GetSectorForObjective(const FString& ObjectiveId);
    /** Greybox layout: sector centre in game-space metres (x east, z north), NAME_None-safe (zero for unknown). */
    static FVector2D GetGreyboxSectorCentre(FName SectorId);
    /** Garrison size for an objective in a phase (0 for the protected medical station). */
    int32 GetGarrisonFor(const FString& ObjectiveId, int32 Phase) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnPlayerDied(FName EventName, const FString& Payload);

private:
    // ---- lookup ----
    UHavocBrokenCitadelMission* GetMission() const;
    AHavocPlayerController* GetPC();
    AHavocPlayerCharacter* GetMarine();
    AHavocSectorVolume* FindSector(FName SectorId) const;
    int32 CountAlive(FName Tag) const;
    FVector ObjectivePoint(const FString& ObjectiveId) const;
    FVector GameToWorld(double X, double Z) const;
    FVector WorldToGame(const FVector& Location) const;

    // ---- greybox ----
    void BuildGreyboxIfNeeded();

    // ---- state machine ----
    void OnEnterState(const FString& State, bool bRestored);
    void TickPhase(float Dt);
    void EndEpisode();
    void ClearWorld();
    void ShowToast(const FString& Text, float Seconds = 5.f);
    void Publish(const TCHAR* EventName, const FString& Payload) const;
    void WatchSurprise();

    // ---- enemies ----
    void SpawnGroup(const FVector& Centre, float RadiusCm, int32 Count, FName Tag, bool bRaiders);
    void TickWaves(float Dt);

    // ---- marine ----
    void TeleportMarine(const FVector& Where);
    void RespawnFromCheckpoint(bool bPenalty);
    void BeginRespawn();

    EHavocFlowPhase Phase = EHavocFlowPhase::Menu;
    bool bMenuShown = false;
    bool bGreybox = false;
    FString LastState;
    FString Hint;
    FString Toast;
    float ToastTime = 0.f;
    FString ResultSummary;
    FString Outcome;
    FString LastSurprise;

    UPROPERTY() TObjectPtr<AHavocPlayerController> CachedPC;
    UPROPERTY() TObjectPtr<AHavocPlayerCharacter> CachedMarine;
    UPROPERTY() TMap<FName, TObjectPtr<AHavocSectorVolume>> Sectors;
    UPROPERTY() TArray<TObjectPtr<AHavocEnemyCharacter>> Enemies;
    UPROPERTY() TArray<TObjectPtr<AActor>> GreyboxActors;

    FTransform InitialMarineTransform;
    FTransform CheckpointTransform;
    bool bHaveInitialTransform = false;
    int32 DeathPenalty = 0;
    bool bRespawnPending = false;
    FTimerHandle RespawnTimer;

    // capture progress (seconds held inside the point), per objective id
    TMap<FString, float> CaptureProgress;

    // counterattacks
    float WaveTimer = 0.f;

    // Falcon / resupply
    float FalconCooldown = 0.f;
    float ResupplyCooldown = 0.f;
    int32 ResupplyLeft = 0;
};
