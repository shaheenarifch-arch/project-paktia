#pragma once

// The playable loop of Episode 01, "Operation Mountain Shield". Built on AHavocEpisodeFlowBase, so the
// player controller, HUD and menus reach it through IHavocEpisodeFlow (Missions/HavocEpisodeFlow.h).
// UHavocMountainShieldMission (data/missions/episode-01-mountain-shield.json, mission_01_mountain_shield)
// owns the rules, states, checkpoints and fail state; this actor turns the player's actions into the
// mission's world inputs, state by state:
//
//   M01_BRIEFING / M01_DRONE_TUTORIAL  seven tutorial steps (move, aim, launch Raven, fly it, look, mark, back) -> AdvanceTutorial
//   M01_HOLD_ECHO         hold Checkpoint Echo against raiders until the timer runs out                      -> HoldEcho
//   M01_RIDGE_STATION     the Marine is moved to Ridge Station, E sets up the relay                          -> SetUpRidgeStation
//   M01_FALCON_DOWN       Falcon-2 goes down (a timed beat until the crash cutscene exists)                  -> NotifyCrashSeen
//   M01_CRASH_DECISION    key 1 recover the crew, key 2 abandon them                                         -> ChooseCrew
//   M01_CREW_RECOVERY     clear grid R-17, E picks the crew up, E at Ridge Station delivers them             -> RecoverCrewMember
//   M01_COUNTERATTACK     repel the raiders before the time limit or Ridge Station falls                     -> RepelCounterattack / LoseCheckpoint
//   M01_DEBRIEF           the mission picks SUCCESS (crew home) or PARTIAL (crew left)                       -> Finish
//
// Places come from the AHavocSectorVolume actors of the level (checkpoint_echo, ridge_station,
// crash_grid_r17; docs: data/maps/paktia-highlands.json). Without them (any other map, tests) BeginPlay
// builds a small greybox slab at GreyboxOriginCm. Abstract game rules only.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/actors-in-unreal-engine

#include "CoreMinimal.h"
#include "Missions/HavocEpisodeFlowBase.h"
#include "HavocEpisode01Flow.generated.h"

class AHavocEnemyCharacter;
class AHavocPlayerCharacter;
class AHavocPlayerController;
class AHavocRavenDrone;
class AHavocSectorVolume;
class AStaticMeshActor;
class UHavocMountainShieldMission;

UCLASS()
class MIDNIGHTHAVOC_API AHavocEpisode01Flow : public AHavocEpisodeFlowBase
{
    GENERATED_BODY()

public:
    AHavocEpisode01Flow();

    virtual void Tick(float DeltaSeconds) override;

    // ---- Tuning (abstract game balance; EditAnywhere so a Blueprint child can retune) ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") FName MissionId;
    /** Greybox only: south-west corner of the slab. Floats above any level so it works in every map. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") FVector GreyboxOriginCm = FVector(0.0, 0.0, 300000.0);
    /** Tutorial: metres within which the Marine reaches the move marker, and degrees within which "aim" counts. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float MarkerRadiusM = 4.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float AimToleranceDegrees = 12.f;
    /** Tutorial: metres within which the Raven reaches the ridge marker / sees the contact, and seconds it must look. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float RavenMarkerRadiusM = 40.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float ObserveSeconds = 2.f;
    /** Checkpoint Echo: seconds the Marine must hold inside the radius, raiders in the first wave and in the second (half way). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float EchoHoldSeconds = 45.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float EchoRadiusM = 60.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 EchoWaveSize = 3;
    /** Metres within which E sets up the relay at Ridge Station / works at the crash site. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float InteractRadiusM = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float CrashBeatSeconds = 6.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 CrashGarrison = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 CounterattackSize = 6;
    /** Ridge Station falls (the mission fails to its last checkpoint) if raiders are still alive after this long. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float CounterattackLimitSeconds = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 ResupplyCharges = 3;

    // ---- Player actions (called by AHavocPlayerController through the base) ----
    virtual void StartEpisode() override;
    virtual void RestartFromCheckpoint() override;
    /** Key 1: recover the Falcon-2 crew. Key 2: abandon them. */
    virtual void ChooseBranch(bool bFirst) override;
    virtual void HandleInteract() override;
    virtual void DeployRaven() override;
    virtual void CallFalcon() override;
    virtual void ReturnToMarine() override;

    // ---- Queries for the HUD and menus ----
    virtual bool IsPiloting() const override;
    virtual FString GetObjectiveText() const override;
    virtual FString GetTrackerText() const override;
    virtual FString GetStatusText() const override;
    virtual FString GetResultTitle() const override;
    virtual FString GetReplayLabel() const override { return TEXT("Play Episode 01 again"); }

    // ---- Pure helpers (tested) ----
    /** Steps of the objective tracker with a [x] / [>] / [ ] marker for a state id. */
    static TArray<FString> BuildTracker(const FString& State, bool bComplete);
    /** Result screen text for a finished run. */
    static FString BuildSummary(const FString& Outcome, int32 CrewRecovered, int32 CrewSize, bool bAegisOnline);
    /** Greybox layout: centre of a Mountain Shield sector in metres from the greybox origin (x east, y north). Zero for an unknown id. */
    static FVector2D GetGreyboxPoint(FName SectorId);
    /** The sector ids this flow looks up in the level. */
    static const TArray<FName>& GetSectorIds();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnPlayerDied(FName EventName, const FString& Payload);

private:
    // ---- lookup ----
    UHavocMountainShieldMission* GetMission() const;
    AHavocPlayerController* GetPC();
    AHavocPlayerCharacter* GetMarine();
    int32 CountAlive(FName Tag) const;
    /** World position (on the ground) of a sector: its volume in a built level, else the greybox point. */
    FVector SectorPoint(FName SectorId) const;
    FVector GroundAt(const FVector& Location) const;
    bool IsNear(const FVector& A, const FVector& B, float RadiusM) const;

    // ---- state machine ----
    void OnEnterState(const FString& State, bool bRestored);
    void TickState(const FString& State, float Dt);
    void TickTutorial(float Dt);
    void Step();
    void EndEpisode();
    void ClearWorld();
    void Publish(const TCHAR* EventName, const FString& Payload) const;

    // ---- world ----
    void FindSectors();
    void BuildGreybox();
    AStaticMeshActor* SpawnProp(const TCHAR* MeshPath, const FVector& GroundLocation, const FVector& SizeMeters, TArray<TObjectPtr<AActor>>& Into);
    void BuildTutorialProps();
    void SpawnEnemies(const FVector& Centre, int32 Count, float RadiusM, FName Tag);

    // ---- marine ----
    void TeleportMarine(const FVector& GroundLocation, const FVector& FaceToward);
    void RespawnFromCheckpoint();
    void BeginRespawn();
    void PossessMarine();

    // ---- Raven ----
    void LaunchRaven();
    void RecoverRaven(const TCHAR* Reason);
    void TickRaven();

    FString LastState;
    bool bGreybox = false;

    UPROPERTY() TObjectPtr<AHavocPlayerController> CachedPC;
    UPROPERTY() TObjectPtr<AHavocPlayerCharacter> CachedMarine;
    UPROPERTY() TMap<FName, TObjectPtr<AHavocSectorVolume>> Sectors;
    UPROPERTY() TArray<TObjectPtr<AActor>> Greybox;
    UPROPERTY() TArray<TObjectPtr<AActor>> TutorialProps;
    UPROPERTY() TArray<TObjectPtr<AHavocEnemyCharacter>> Enemies;
    UPROPERTY() TObjectPtr<AHavocRavenDrone> Raven;
    UPROPERTY() TObjectPtr<AActor> LinkRelay;

    FTransform CheckpointTransform;
    bool bHaveCheckpointTransform = false;
    bool bRespawnPending = false;
    FTimerHandle RespawnTimer;

    // tutorial and per-state timers
    FVector MoveMarker = FVector::ZeroVector;
    FVector DummyPoint = FVector::ZeroVector;
    FVector RavenMarker = FVector::ZeroVector;
    float PhaseTimer = 0.f;
    float ObserveTimer = 0.f;
    bool bSecondWaveSent = false;
    int32 CrewCarried = 0;

    // resupply
    float ResupplyCooldown = 0.f;
    int32 ResupplyLeft = 0;
};
