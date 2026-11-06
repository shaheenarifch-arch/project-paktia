#pragma once

// The playable loop of Episode 04, "Operation Open Strait". One per level, spawned by
// AHavocPlayableGameMode. UHavocOpenStraitMission (data/missions/episode-04-open-strait.json) owns
// the rules, states, checkpoints and fail state; this actor turns the player's actions into the
// mission's world inputs, state by state:
//
//   E04_BEACH_RECON    Raven (possessed) flies over both beaches        -> ScoutBeaches
//   E04_CHOOSE_BEACH   key 1 north / key 2 west                         -> ChooseBeach
//   *_LANDING          Marine lands, clears beach defenders             -> SecureBeachhead
//   TERMINAL / AIRSTRIP clear the garrison, keep fire off the facilities -> SecureTerminal / SecureAirstrip
//   E04_ISLAND_SECURED hold the oil terminal against counter-attacks
//   E04_LANE_CLEARANCE Raven reveals lane hazards (AHavocLaneHazard), E marks them for clearance
//   E04_TANKER_ESCORT  tankers transit the lanes (AHavocTankerSpline); call Falcon support (G)
//   E04_DEBRIEF        Finish() -> SUCCESS / PARTIAL; FAILED from the mission fail state
//
// Raven and Falcon pawns are possessed for their phase and the player returns to the Marine.
// Enemy squads are spawned from data/units (Iran, Houthi, ISIS; AI-only). Abstract game rules only.
// Guide: game/README.md "How to play". Help:
//   https://dev.epicgames.com/documentation/en-us/unreal-engine/possessing-pawns-in-unreal-engine

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Missions/HavocEpisodeFlow.h"
#include "HavocEpisode04Flow.generated.h"

class AHavocCutscene;
class AHavocEnemyCharacter;
class AHavocFalconHelicopter;
class AHavocLaneHazard;
class AHavocPlayerCharacter;
class AHavocPlayerController;
class AHavocRavenDrone;
class AHavocSectorVolume;
class AHavocTankerSpline;
class UHavocOpenStraitMission;
class USkeletalMeshComponent;

/** A tanker moving along its lane spline. Visual from the Vessel.Tanker role. */
UCLASS()
class MIDNIGHTHAVOC_API AHavocTankerVessel : public AActor
{
    GENERATED_BODY()

public:
    AHavocTankerVessel();

    UPROPERTY() FString TankerId;
    UPROPERTY() FName LaneId;
    UPROPERTY() TObjectPtr<AHavocTankerSpline> Route;
    float DistanceCm = 0.f;

    /** Moves along the route. Returns true on reaching the end. */
    bool Advance(float DeltaSeconds, float SpeedCmPerSec);

private:
    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> Visual;
};

UCLASS()
class MIDNIGHTHAVOC_API AHavocEpisode04Flow : public AActor, public IHavocEpisodeFlow
{
    GENERATED_BODY()

public:
    AHavocEpisode04Flow();

    virtual void Tick(float DeltaSeconds) override;

    // ---- Tuning (abstract game balance; EditAnywhere so a Blueprint child can retune) ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") FName MissionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 NorthDefenders = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 WestDefenders = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 AirstripGarrison = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 TerminalGarrison = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float HoldSeconds = 75.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float WaveIntervalSeconds = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 WaveSize = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float TankerSpeedCmPerSec = 12000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float TankerLaunchIntervalSeconds = 18.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float ThreatIntervalSeconds = 28.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float ThreatWarningSeconds = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float ThreatDamage = 0.34f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float FalconSupportSeconds = 45.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float FalconCooldownSeconds = 30.f;
    /** Pacing: the Raven's TS speed (28 m/s) is scaled so a lane sweep fits one battery. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float RavenSpeedScale = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 DeathReinforcementCost = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 ResupplyCharges = 3;

    // ---- Player actions (called by AHavocPlayerController) ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void StartEpisode() override;
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void RestartFromCheckpoint() override;
    virtual void PauseEpisode() override;
    virtual void ResumeEpisode() override;
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") void ChooseBeach(bool bNorth);
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") virtual void HandleInteract() override;
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") void DeployRaven();
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") void CallFalcon();
    UFUNCTION(BlueprintCallable, Category = "Havoc|Flow") void ReturnToMarine();

    // ---- Cutscenes (skippable camera fly-over with subtitles; Cutscene/HavocCutscene.h) ----
    /** Plays the intro; StartEpisode does this by itself. Also used by the smoke test. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Cutscene") void PlayIntro();
    /** Plays the outro for the current outcome; EndEpisode does this before the results screen. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Cutscene") void PlayOutro();
    UFUNCTION(BlueprintCallable, Category = "Havoc|Cutscene") void SkipCutscene();
    UFUNCTION(BlueprintPure, Category = "Havoc|Cutscene") bool IsCutsceneActive() const;
    /** Current cutscene subtitle line, empty when none (the HUD shows it when the Subtitles setting is on). */
    UFUNCTION(BlueprintPure, Category = "Havoc|Cutscene") FString GetCutsceneSubtitle() const;

    // ---- Queries for the HUD and menus ----
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual EHavocFlowPhase GetPhase() const override { return Phase; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetHint() const override { return Hint; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetToast() const override { return ToastTime > 0.f ? Toast : FString(); }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetSupportStatus() const override;
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetResultSummary() const override { return ResultSummary; }
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") virtual FString GetOutcome() const override { return Outcome; }
    virtual FString GetCurrentObjective() const override;
    UFUNCTION(BlueprintPure, Category = "Havoc|Flow") bool IsPiloting() const;

    /** Pure helper (tested): garrison size for a state-independent sector id, 0 for unknown sectors. */
    int32 GetGarrisonFor(FName SectorId) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnPlayerDied(FName EventName, const FString& Payload);

private:
    // ---- lookup ----
    UHavocOpenStraitMission* GetMission() const;
    AHavocPlayerController* GetPC();
    AHavocPlayerCharacter* GetMarine();
    AHavocSectorVolume* FindSector(FName SectorId);
    bool IsInside(FName SectorId, const FVector& Location);
    int32 CountAlive(FName SectorId) const;

    // ---- state machine ----
    void OnEnterState(const FString& State, bool bRestored);
    void TickState(const FString& State, float Dt);
    void Step();
    void EndEpisode();
    void ClearWorld();
    void SetHint(const FString& Text) { Hint = Text; }
    void ShowToast(const FString& Text, float Seconds = 5.f);
    void Publish(const TCHAR* EventName, const FString& Payload) const;

    // ---- enemies ----
    void SpawnGarrison(FName SectorId, int32 Count, FName Tag = NAME_None, float MinDistFromMarine = 0.f, const TArray<FString>* FactionsOverride = nullptr);
    TArray<FString> FactionsFor(FName SectorId) const;

    // ---- marine ----
    void TeleportMarineToBeach(FName BeachSectorId);
    void RespawnFromCheckpoint(bool bPenalty);
    void BeginRespawn();
    void PossessMarine();

    // ---- Raven ----
    void LaunchRaven(const FVector& Home, bool bFollowRelay);
    void RecoverRaven(const TCHAR* Reason);
    void TickRaven(float Dt);
    void TickRecon();
    void TickClearance(float Dt);
    FVector ForwardLaunchPoint() const;
    FVector LaneLaunchPoint() const;

    // ---- escort ----
    void TickEscort(float Dt);
    void SpawnTanker(const FString& TankerId, FName LaneId);
    void ResolveThreat();

    // ---- Falcon ----
    void StartFalconSupport();
    void EndFalconSupport();
    void TickFalcon(float Dt);

    EHavocFlowPhase Phase = EHavocFlowPhase::Menu;
    bool bMenuShown = false;
    FString LastState;
    FString LastCheckpointState;
    FString Hint;
    FString Toast;
    float ToastTime = 0.f;
    FString ResultSummary;
    FString Outcome;

    UPROPERTY() TObjectPtr<AHavocCutscene> Cutscene;
    UPROPERTY() TObjectPtr<AHavocPlayerController> CachedPC;
    UPROPERTY() TObjectPtr<AHavocPlayerCharacter> CachedMarine;
    UPROPERTY() TMap<FName, TObjectPtr<AHavocSectorVolume>> Sectors;
    UPROPERTY() TArray<TObjectPtr<AHavocEnemyCharacter>> Enemies;

    FTransform InitialMarineTransform;
    FTransform CheckpointTransform;
    bool bHaveInitialTransform = false;
    int32 DeathPenalty = 0;
    bool bRespawnPending = false;
    FTimerHandle RespawnTimer;

    // recon
    bool bNorthSeen = false;
    bool bWestSeen = false;
    bool bScoutedDone = false;

    // hold / waves
    float HoldRemaining = 0.f;
    float WaveTimer = 0.f;

    // Raven
    UPROPERTY() TObjectPtr<AHavocRavenDrone> Raven;
    UPROPERTY() TObjectPtr<AActor> Relay;
    bool bRelayFollows = false;
    TMap<FName, float> PendingClearance; // hazard id -> seconds left
    TMap<FName, TWeakObjectPtr<AHavocLaneHazard>> PendingHazards;

    // escort
    UPROPERTY() TArray<TObjectPtr<AHavocTankerVessel>> Vessels;
    int32 NextTankerIndex = 0;
    float LaunchTimer = 0.f;
    float ThreatTimer = 0.f;
    FString ThreatTankerId;
    float ThreatCountdown = -1.f;

    // Falcon / resupply
    UPROPERTY() TObjectPtr<AHavocFalconHelicopter> Falcon;
    float SupportRemaining = 0.f;
    float SupportCooldown = 0.f;
    float ResupplyCooldown = 0.f;
    int32 ResupplyLeft = 0;
};
