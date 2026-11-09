#include "Missions/Episode01/HavocEpisode01Flow.h"

#include "Core/HavocEventSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/HavocMissionSubsystem.h"
#include "Missions/Episode01/HavocMountainShieldMission.h"
#include "Player/HavocPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogHavocFlow01, Log, All);

namespace
{
    struct FTrackerStep
    {
        const TCHAR* Label;
        const TCHAR* State;
    };

    const FTrackerStep TrackerSteps[] = {
        { TEXT("Learn the controls"), TEXT("M01_DRONE_TUTORIAL") },
        { TEXT("Hold Checkpoint Echo"), TEXT("M01_HOLD_ECHO") },
        { TEXT("Set up Ridge Station"), TEXT("M01_RIDGE_STATION") },
        { TEXT("Watch Falcon-2 go down"), TEXT("M01_FALCON_DOWN") },
        { TEXT("Decide the crew's fate"), TEXT("M01_CRASH_DECISION") },
        { TEXT("Crew recovery"), TEXT("M01_CREW_RECOVERY") },
        { TEXT("Repel the counterattack"), TEXT("M01_COUNTERATTACK") },
        { TEXT("Debrief"), TEXT("M01_DEBRIEF") },
    };

    /** Index into TrackerSteps for a state; the abandoned path shares the recovery slot. */
    int32 TrackerIndex(const FString& State)
    {
        if (State == TEXT("M01_BRIEFING")) { return 0; }
        if (State == TEXT("M01_ABANDONED")) { return 5; }
        for (int32 i = 0; i < UE_ARRAY_COUNT(TrackerSteps); ++i)
        {
            if (State == TrackerSteps[i].State) { return i; }
        }
        return INDEX_NONE;
    }
}

AHavocEpisode01Flow::AHavocEpisode01Flow()
    : MissionId(TEXT("mission_01_mountain_shield"))
{
    PrimaryActorTick.bCanEverTick = true;
}

UHavocMountainShieldMission* AHavocEpisode01Flow::GetMission() const
{
    const UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    return Missions ? Missions->GetMountainShield() : nullptr;
}

void AHavocEpisode01Flow::Publish(const TCHAR* EventName, const FString& Payload) const
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Publish(EventName, Payload);
        }
    }
}

// ---------------------------------------------------------------- pure helpers

TArray<FString> AHavocEpisode01Flow::BuildTracker(const FString& State, bool bComplete)
{
    const int32 Now = TrackerIndex(State);
    TArray<FString> Lines;
    for (int32 i = 0; i < UE_ARRAY_COUNT(TrackerSteps); ++i)
    {
        const bool bDone = bComplete || (Now != INDEX_NONE && i < Now);
        const bool bNow = !bComplete && i == Now;
        Lines.Add(FString::Printf(TEXT("%s %s"), bDone ? TEXT("[x]") : bNow ? TEXT("[>]") : TEXT("[ ]"), TrackerSteps[i].Label));
    }
    return Lines;
}

FString AHavocEpisode01Flow::BuildHint(const UHavocMountainShieldMission& Mission)
{
    const FString State = Mission.GetState();
    if (State == TEXT("M01_DRONE_TUTORIAL"))
    {
        return UHavocMountainShieldMission::GetTutorialPrompt(Mission.GetTutorialStep()) + TEXT(" (E when done)");
    }
    if (State == TEXT("M01_HOLD_ECHO")) { return TEXT("Hold Checkpoint Echo. Press E when the timer runs out."); }
    if (State == TEXT("M01_RIDGE_STATION")) { return TEXT("Move to Ridge Station. Press E to set up the relay."); }
    if (State == TEXT("M01_FALCON_DOWN")) { return TEXT("Falcon-2 is going down. Press E when you have seen it crash."); }
    if (State == TEXT("M01_CRASH_DECISION")) { return TEXT("1: recover the crew (risky)   2: abandon them and hold Ridge Station"); }
    if (State == TEXT("M01_CREW_RECOVERY"))
    {
        return FString::Printf(TEXT("Press E to carry a crew member to the evacuation point (%d of %d)."), Mission.GetCrewRecovered(), Mission.GetCrewSize());
    }
    if (State == TEXT("M01_COUNTERATTACK")) { return TEXT("Survive the counterattack. Press E when it is repelled."); }
    if (State == TEXT("M01_DEBRIEF")) { return TEXT("Press E to finish the debrief."); }
    return FString();
}

bool AHavocEpisode01Flow::InteractStep(UHavocMountainShieldMission& Mission)
{
    const FString State = Mission.GetState();
    bool bDone = false;
    if (State == TEXT("M01_DRONE_TUTORIAL"))
    {
        const EHavocTutorialStep Step = Mission.GetTutorialStep();
        bDone = Mission.AdvanceTutorial(Step) != Step;
    }
    else if (State == TEXT("M01_HOLD_ECHO")) { bDone = Mission.HoldEcho(); }
    else if (State == TEXT("M01_RIDGE_STATION")) { bDone = Mission.SetUpRidgeStation(); }
    else if (State == TEXT("M01_FALCON_DOWN")) { bDone = Mission.NotifyCrashSeen(); }
    else if (State == TEXT("M01_CREW_RECOVERY"))
    {
        const int32 Before = Mission.GetCrewRecovered();
        bDone = Mission.RecoverCrewMember() != Before;
    }
    else if (State == TEXT("M01_COUNTERATTACK")) { bDone = Mission.RepelCounterattack(); }
    else if (State == TEXT("M01_DEBRIEF"))
    {
        Mission.Finish();
        bDone = true;
    }
    if (bDone)
    {
        Mission.Progress();
    }
    return bDone;
}

// ---------------------------------------------------------------- queries

FString AHavocEpisode01Flow::GetObjectiveText() const
{
    const UHavocMountainShieldMission* Mission = GetMission();
    return Mission && Mission->GetRunner() ? Mission->GetRunner()->GetObjective() : FString();
}

FString AHavocEpisode01Flow::GetTrackerText() const
{
    const UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission || !Mission->GetRunner())
    {
        return FString();
    }
    const UHavocMissionRunner* Runner = Mission->GetRunner();
    const bool bDone = Runner->IsComplete() && Runner->GetOutcome() != TEXT("FAILED");
    return FString::Join(BuildTracker(Runner->GetState(), bDone), TEXT("\n"));
}

FString AHavocEpisode01Flow::GetStatusText() const
{
    const UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission)
    {
        return FString();
    }
    const TCHAR* Aegis = Mission->IsAegisOnline() ? TEXT("online") : TEXT("offline");
    return FString::Printf(TEXT("Aegis: %s\nCrew recovered: %d of %d"), Aegis, Mission->GetCrewRecovered(), Mission->GetCrewSize());
}

FString AHavocEpisode01Flow::GetResultTitle() const
{
    return Outcome == TEXT("SUCCESS") ? TEXT("MOUNTAIN SHIELD HOLDS - MISSION COMPLETE")
        : Outcome == TEXT("PARTIAL") ? TEXT("MOUNTAIN SHIELD HOLDS - CREW LEFT BEHIND")
        : TEXT("RIDGE STATION LOST");
}

// ---------------------------------------------------------------- episode control

void AHavocEpisode01Flow::StartEpisode()
{
    UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    if (!Missions)
    {
        return;
    }
    if (!Missions->StartMission(MissionId) || !Missions->GetMountainShield())
    {
        ShowToast(TEXT("Episode 01 could not start. Check that the data folder is packaged beside the game (game/README.md)."), 10.f);
        UE_LOG(LogHavocFlow01, Error, TEXT("Mission '%s' failed to start"), *MissionId.ToString());
        return;
    }
    Phase = EHavocFlowPhase::Playing;
    LastState.Empty();
    Outcome.Empty();
    ResultSummary.Empty();
    SetHint(FString());

    if (AHavocPlayerController* PC = Cast<AHavocPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        PC->HideMenu();
        PC->ShowHud();
    }
    // Briefing -> tutorial (the runner stops there until the steps are done).
    Missions->GetMountainShield()->Progress();
    SetHint(BuildHint(*Missions->GetMountainShield()));
}

void AHavocEpisode01Flow::RestartFromCheckpoint()
{
    if (Phase == EHavocFlowPhase::Menu)
    {
        return;
    }
    UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    if (!Missions || !Missions->RestoreCheckpoint())
    {
        StartEpisode();
        return;
    }
    Phase = EHavocFlowPhase::Playing;
    LastState.Empty();
    if (AHavocPlayerController* PC = Cast<AHavocPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        PC->HideMenu();
        PC->ShowHud();
    }
}

void AHavocEpisode01Flow::EndEpisode()
{
    UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission || !Mission->GetRunner())
    {
        return;
    }
    Phase = EHavocFlowPhase::Results;
    Outcome = Mission->GetRunner()->GetOutcome();
    SetHint(FString());

    const FHavocEpisode01Result C = Mission->GetCarryover();
    ResultSummary = FString::Printf(TEXT("Aegis: %s\nFalcon-2 crew recovered: %s (%d of %d)"),
        Mission->IsAegisOnline() ? TEXT("online") : TEXT("lost"),
        C.bCrewRecovered ? TEXT("yes") : TEXT("no"), Mission->GetCrewRecovered(), Mission->GetCrewSize());

    // Campaign carryover: Episode 02 reads whether the crew came home.
    if (UHavocMissionSubsystem* Missions = GetWorld()->GetSubsystem<UHavocMissionSubsystem>())
    {
        if (Outcome != TEXT("FAILED"))
        {
            Missions->SetEpisode1CrewRecovered(C.bCrewRecovered);
        }
    }
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("mission"), TEXT("Operation Mountain Shield"));
    Json->SetStringField(TEXT("state"), Mission->GetState());
    Json->SetNumberField(TEXT("episode"), C.Episode);
    Json->SetStringField(TEXT("outcome"), C.Outcome);
    Json->SetBoolField(TEXT("crewRecovered"), C.bCrewRecovered);
    Publish(TEXT("episode:results"), HavocMissionJson::ToString(Json));

    if (AHavocPlayerController* PC = Cast<AHavocPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        PC->ShowMenu(EHavocMenuMode::Results);
    }
}

void AHavocEpisode01Flow::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ToastTime = FMath::Max(0.f, ToastTime - DeltaSeconds);

    if (Phase != EHavocFlowPhase::Playing)
    {
        return;
    }
    UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission || !Mission->GetRunner())
    {
        return;
    }
    if (Mission->GetRunner()->IsComplete())
    {
        EndEpisode();
        return;
    }
    // Move through states whose requirements are met (e.g. the abandoned path has none).
    Mission->Progress();
    if (Mission->GetRunner()->IsComplete())
    {
        EndEpisode();
        return;
    }
    const FString State = Mission->GetState();
    if (State != LastState)
    {
        LastState = State;
        UE_LOG(LogHavocFlow01, Display, TEXT("Episode 01 state %s"), *State);
    }
    // The tutorial prompt changes within one state, so rebuild the hint every tick (cheap string).
    SetHint(BuildHint(*Mission));
}

// ---------------------------------------------------------------- player actions

void AHavocEpisode01Flow::ChooseBranch(bool bFirst)
{
    UHavocMountainShieldMission* Mission = GetMission();
    if (Phase != EHavocFlowPhase::Playing || !Mission)
    {
        return;
    }
    if (Mission->GetState() != TEXT("M01_CRASH_DECISION"))
    {
        ShowToast(TEXT("The choice comes after Falcon-2 goes down."), 3.f);
        return;
    }
    Mission->ChooseCrew(bFirst);
}

void AHavocEpisode01Flow::HandleInteract()
{
    UHavocMountainShieldMission* Mission = GetMission();
    if (Phase != EHavocFlowPhase::Playing || !Mission)
    {
        return;
    }
    if (Mission->GetState() == TEXT("M01_CRASH_DECISION"))
    {
        ShowToast(TEXT("Choose with 1 (recover the crew) or 2 (abandon them)."), 3.f);
        return;
    }
    InteractStep(*Mission);
}
