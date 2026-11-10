#pragma once

// The playable loop of Episode 01, "Operation Mountain Shield". Spawned on demand by the Campaign menu or
// -HavocSmokeEpisode=01 (AHavocPlayerController::SelectFlow). UHavocMountainShieldMission
// (data/missions/episode-01-mountain-shield.json) owns the rules; this actor turns the player's actions
// into the mission's world inputs, state by state. E (HandleInteract) moves the mission on:
//
//   M01_DRONE_TUTORIAL   each E completes the current tutorial step (the step prompt is the hint) -> AdvanceTutorial
//   M01_HOLD_ECHO        E                                                                         -> HoldEcho
//   M01_RIDGE_STATION    E                                                                         -> SetUpRidgeStation
//   M01_FALCON_DOWN      E                                                                         -> NotifyCrashSeen
//   M01_CRASH_DECISION   key 1 recover the crew, key 2 abandon them                                -> ChooseCrew
//   M01_CREW_RECOVERY    E carries one crew member to the evacuation point                         -> RecoverCrewMember
//   M01_COUNTERATTACK    E                                                                         -> RepelCounterattack
//   M01_DEBRIEF          E                                                                         -> Finish
//
// Intro: a fly-over of highlands_staging -> checkpoint_echo -> ridge_station before the tutorial starts. Outro: ridge_station
// -> crash_grid_r17, then the results screen. Both are skippable (AHavocEpisodeFlowBase::PlayCutscene).
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/actors-in-unreal-engine

#include "CoreMinimal.h"
#include "Missions/HavocEpisodeFlowBase.h"
#include "HavocEpisode01Flow.generated.h"

class UHavocMountainShieldMission;

UCLASS()
class MIDNIGHTHAVOC_API AHavocEpisode01Flow : public AHavocEpisodeFlowBase
{
    GENERATED_BODY()

public:
    AHavocEpisode01Flow();

    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Flow") FName MissionId;

    // ---- Player actions (called by AHavocPlayerController) ----
    virtual void StartEpisode() override;
    virtual void RestartFromCheckpoint() override;
    /** Key 1: recover the Falcon-2 crew. Key 2: abandon them. Only at the crash decision. */
    virtual void ChooseBranch(bool bFirst) override;
    virtual void HandleInteract() override;

    // ---- Queries for the HUD and menus ----
    virtual FString GetObjectiveText() const override;
    virtual FString GetTrackerText() const override;
    virtual FString GetStatusText() const override;
    virtual FString GetResultTitle() const override;
    virtual FString GetReplayLabel() const override { return TEXT("Play Episode 01 again"); }

    // ---- Pure helpers (static so tests need no world) ----
    /** One E press on the mission: calls the world input for the current state. Returns true if it did something. */
    static bool InteractStep(UHavocMountainShieldMission& Mission);
    /** Hint text for a mission state (the tutorial prompt while in the tutorial). */
    static FString BuildHint(const UHavocMountainShieldMission& Mission);
    /** Steps of the objective tracker with a [x] / [>] / [ ] marker for a state id. */
    static TArray<FString> BuildTracker(const FString& State, bool bComplete);
    /** Sector ids the intro / outro fly over (data/maps/paktia-highlands.json). */
    static TArray<FName> CutsceneSectorIds(bool bIntro);
    /** Where a Paktia sector sits when the level has no volume for it: its centre in build_paktia_highlands_level.py (cm). */
    static FVector FallbackSectorCentre(FName SectorId);

protected:
    virtual TArray<FVector> CutsceneTargets(bool bIntro) override;
    virtual FString CutsceneLabel() const override { return TEXT("01"); }

private:
    UHavocMountainShieldMission* GetMission() const;
    void EndEpisode();
    void Publish(const TCHAR* EventName, const FString& Payload) const;

    FString LastState;
};
