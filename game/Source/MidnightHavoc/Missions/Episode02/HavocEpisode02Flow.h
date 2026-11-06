#pragma once

// The playable loop of Episode 02, "Operation Silent Corridor", in greybox. Spawned on demand by the
// Campaign menu (AHavocPlayerController::SelectFlow). UHavocSilentCorridorMission
// (data/missions/episode-02-silent-corridor.json) owns the rules, states, checkpoints and fail state; this
// actor turns the player's actions into the mission's world inputs, state by state:
//
//   E02_RELAY_KILO          Raven (possessed) flies to the marked site, E drops Relay Kilo     -> PlaceRelayKilo
//   E02_CORRIDOR_SCOUT      Raven reaches the corridor end with a usable link                  -> ScoutCorridor
//   E02_CORRIDOR_ADVANCE    the convoy rolls down the main road
//   E02_COMMS_OUTAGE        every relay is jammed; the Marine walks to Relay Kilo, E restores  -> RestoreRelayKilo
//   E02_BRIDGE_OBSTRUCTED   key 1 riverbed detour, key 2 drainage infiltration                -> ChooseRoute
//   E02_RIVERBED_DETOUR     repel the ambush in the narrows while it shoots the convoy         -> ClearAmbushContact
//   E02_DRAINAGE_INFILTRATION  infantry only: clear the far bank                               -> ClearFarBank
//   E02_BRIDGE_CLEARED      Warden-1 pushes the wreck off the bridge                           -> ClearBridge
//   E02_SOUTH_GATE_APPROACH clear the staging yard                                             -> ClearStaging
//   E02_STAGING_ESTABLISHED the staging area is set up                                         -> Finish
//
// The world is built here (no level needed): a floating greybox slab at GreyboxOrigin with blocks for the
// landmarks and box vehicles for the convoy. Replace the blocks with the real Gardun level and vehicle
// meshes later; the mission rules do not change. Game-space metres from the TS map to world cm as
// X = x, Y = z, Z = y (x east, z along the valley, y up). Abstract game rules only.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/basic-scene-and-blockout-meshes

#include "CoreMinimal.h"
#include "Missions/HavocEpisodeFlowBase.h"
#include "HavocEpisode02Flow.generated.h"

class AHavocEnemyCharacter;
class AHavocPlayerCharacter;
class AHavocPlayerController;
class AHavocRavenDrone;
class AStaticMeshActor;
class UHavocSilentCorridorMission;

UCLASS()
class MIDNIGHTHAVOC_API AHavocEpisode02Flow : public AHavocEpisodeFlowBase
{
    GENERATED_BODY()

public:
    AHavocEpisode02Flow();

    virtual void Tick(float DeltaSeconds) override;

    // ---- Tuning (abstract game balance; EditAnywhere so a Blueprint child can retune) ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") FName MissionId;
    /** Mission 01 result: false puts supplies on Nomad-1 instead of the Falcon-2 crew. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") bool bCrewRecovered = true;
    /** World position (cm) of Checkpoint Echo. The greybox floats high above the Kharg level so the two never overlap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") FVector GreyboxOrigin = FVector(0.0, 0.0, 300000.0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float ConvoySpeedMps = 12.f;
    /** Metres around the marked site within which E drops Relay Kilo. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float RelayDropRadius = 120.f;
    /** Metres around the corridor end within which the Raven scouts it (the link must still be usable). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float ScoutRadius = 80.f;
    /** Metres within which the Marine can restore Relay Kilo (the mission needs 30 m in 3D). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float RestoreRadius = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 AmbushSize = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 FarBankGarrison = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 StagingGarrison = 5;
    /** Seconds between ambush volleys at the convoy, and damage per living ambusher per volley (the Warden takes half). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float AmbushVolleySeconds = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float AmbushDamagePerHit = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float BridgeClearSeconds = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float StagingSetupSeconds = 8.f;
    /** Pacing: the Raven's TS speed (28 m/s) is scaled so the corridor fits one battery. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") float RavenSpeedScale = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") int32 ResupplyCharges = 3;

    // ---- Player actions (called by AHavocPlayerController) ----
    virtual void StartEpisode() override;
    virtual void RestartFromCheckpoint() override;
    /** Key 1: riverbed detour. Key 2: drainage infiltration. */
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
    virtual FString GetReplayLabel() const override { return TEXT("Play Episode 02 again"); }

    // ---- Coordinate helpers (pure, tested) ----
    /** Game-space metres (x east, y up, z valley) to world cm. */
    FVector ToWorld(const FVector& Game) const;
    /** As ToWorld but on the greybox floor (game y ignored). */
    FVector ToWorldGround(const FVector& Game) const;
    FVector ToGame(const FVector& World) const;

    /** Steps of the objective tracker with a [x] / [>] / [ ] marker for a state id (static so tests need no world). */
    static TArray<FString> BuildTracker(const FString& State, bool bComplete);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnPlayerDied(FName EventName, const FString& Payload);

private:
    // ---- lookup ----
    UHavocSilentCorridorMission* GetMission() const;
    AHavocPlayerController* GetPC();
    AHavocPlayerCharacter* GetMarine();
    int32 CountAlive(FName Tag) const;

    // ---- state machine ----
    void OnEnterState(const FString& State, bool bRestored);
    void TickState(const FString& State, float Dt);
    void Step();
    void EndEpisode();
    void ClearWorld();
    void Publish(const TCHAR* EventName, const FString& Payload) const;

    // ---- greybox world ----
    void BuildGreybox();
    AStaticMeshActor* SpawnBlock(const FVector& GameGroundCentre, const FVector& SizeMeters, double BaseMeters = 0.0);

    // ---- convoy ----
    void SyncConvoy();
    void SetConvoyGoal(const FVector& GameGoal, bool bSnapNow);
    void TickConvoy(float Dt);
    bool ConvoyAtGoal() const;
    void DestroyConvoyActors();

    // ---- enemies ----
    void SpawnEnemies(const FVector& GameCentre, int32 Count, float RadiusMeters, FName Tag, TArray<AHavocEnemyCharacter*>* OutSpawned = nullptr);
    void TickAmbush(float Dt);

    // ---- marine ----
    void TeleportMarine(const FVector& GamePos);
    void RespawnFromCheckpoint();
    void BeginRespawn();
    void PossessMarine();

    // ---- Raven ----
    void LaunchRaven(const FVector& GameHome);
    void RecoverRaven(const TCHAR* Reason);
    void TickRaven(float Dt);
    void TickScout(float Dt);

    bool bMenuShown = true; // the Campaign menu starts this flow; it never shows the main menu itself
    FString LastState;
    float ConvoyHeading = 0.f;

    UPROPERTY() TObjectPtr<AHavocPlayerController> CachedPC;
    UPROPERTY() TObjectPtr<AHavocPlayerCharacter> CachedMarine;
    UPROPERTY() TArray<TObjectPtr<AActor>> Greybox;
    UPROPERTY() TArray<TObjectPtr<AHavocEnemyCharacter>> Enemies;
    /** Index i is the enemy for ambush contact "e02-ambush-(i+1)". */
    UPROPERTY() TArray<TObjectPtr<AHavocEnemyCharacter>> AmbushEnemies;
    UPROPERTY() TMap<FString, TObjectPtr<AStaticMeshActor>> ConvoyActors;

    FTransform CheckpointTransform;
    bool bHaveCheckpointTransform = false;
    bool bRespawnPending = false;
    FTimerHandle RespawnTimer;

    // convoy visuals (game-space metres on the ground)
    FVector ConvoyPos = FVector::ZeroVector;
    FVector ConvoyGoal = FVector::ZeroVector;

    // timers
    float AmbushTimer = 0.f;
    int32 AmbushHits = 0;
    float PhaseTimer = 0.f;
    float LinkToastCooldown = 0.f;

    // Raven
    UPROPERTY() TObjectPtr<AHavocRavenDrone> Raven;
    UPROPERTY() TObjectPtr<AActor> LinkRelay;

    // resupply
    float ResupplyCooldown = 0.f;
    int32 ResupplyLeft = 0;
};
