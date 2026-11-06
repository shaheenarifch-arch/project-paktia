#include "Missions/Episode01/HavocMountainShieldMission.h"

#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionRunner.h"

bool UHavocMountainShieldMission::Initialize(const FHavocMissionDef& Definition, UHavocEventSubsystem* Events, FString& OutError)
{
    TutorialStep = EHavocTutorialStep::Move;
    RidgeStation = EHavocRidgeStationState::Unknown;
    Counterattack = EHavocCounterattackState::Inactive;
    CrewSize = 3;
    CrewRecovered = 0;
    bCrewLocated = false;
    bCrewAbandoned = false;

    Runner = NewObject<UHavocMissionRunner>(this);

    TMap<FString, FHavocEffectHandler> Handlers;
    Handlers.Add(TEXT("ridge.discover"), [this](const FHavocMissionStateDef&) { if (RidgeStation == EHavocRidgeStationState::Unknown) { RidgeStation = EHavocRidgeStationState::Discovered; } });
    // The crash itself is a cutscene (AHavocCrashSequence); the world calls NotifyCrashSeen when it ends.
    Handlers.Add(TEXT("crash.begin"), [](const FHavocMissionStateDef&) {});
    Handlers.Add(TEXT("crew.locate"), [this](const FHavocMissionStateDef&) { bCrewLocated = true; });
    Handlers.Add(TEXT("crew.abandon"), [this](const FHavocMissionStateDef&) { bCrewAbandoned = true; });
    Handlers.Add(TEXT("counterattack.begin"), [this](const FHavocMissionStateDef&) { Counterattack = EHavocCounterattackState::Active; });

    return Runner->Initialize(Definition, Handlers, Events, this, OutError);
}

FString UHavocMountainShieldMission::SnapshotSystems() const
{
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("tutorialStep"), static_cast<int32>(TutorialStep));
    Json->SetNumberField(TEXT("ridgeStation"), static_cast<int32>(RidgeStation));
    Json->SetNumberField(TEXT("counterattack"), static_cast<int32>(Counterattack));
    Json->SetNumberField(TEXT("crewRecovered"), CrewRecovered);
    Json->SetBoolField(TEXT("crewLocated"), bCrewLocated);
    Json->SetBoolField(TEXT("crewAbandoned"), bCrewAbandoned);
    return HavocMissionJson::ToString(Json);
}

void UHavocMountainShieldMission::RestoreSystems(const FString& SnapshotJson)
{
    const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(SnapshotJson);
    int32 Value = 0;
    if (Json->TryGetNumberField(TEXT("tutorialStep"), Value)) { TutorialStep = static_cast<EHavocTutorialStep>(FMath::Clamp(Value, 0, static_cast<int32>(EHavocTutorialStep::Complete))); }
    if (Json->TryGetNumberField(TEXT("ridgeStation"), Value)) { RidgeStation = static_cast<EHavocRidgeStationState>(FMath::Clamp(Value, 0, static_cast<int32>(EHavocRidgeStationState::Lost))); }
    if (Json->TryGetNumberField(TEXT("counterattack"), Value)) { Counterattack = static_cast<EHavocCounterattackState>(FMath::Clamp(Value, 0, static_cast<int32>(EHavocCounterattackState::CheckpointLost))); }
    Json->TryGetNumberField(TEXT("crewRecovered"), CrewRecovered);
    Json->TryGetBoolField(TEXT("crewLocated"), bCrewLocated);
    Json->TryGetBoolField(TEXT("crewAbandoned"), bCrewAbandoned);
}

FString UHavocMountainShieldMission::GetState() const
{
    return Runner ? Runner->GetState() : FString();
}

bool UHavocMountainShieldMission::InState(const TCHAR* StateId) const
{
    return Runner && Runner->GetState() == StateId;
}

FString UHavocMountainShieldMission::Progress()
{
    FString Before;
    do
    {
        Before = Runner->GetState();
        Runner->Advance();
    } while (Runner->GetState() != Before);
    return Runner->GetState();
}

EHavocTutorialStep UHavocMountainShieldMission::AdvanceTutorial(EHavocTutorialStep Completed)
{
    if (!InState(TEXT("M01_DRONE_TUTORIAL")) || TutorialStep != Completed || TutorialStep == EHavocTutorialStep::Complete)
    {
        return TutorialStep;
    }
    TutorialStep = static_cast<EHavocTutorialStep>(static_cast<uint8>(TutorialStep) + 1);
    if (TutorialStep == EHavocTutorialStep::Complete)
    {
        Runner->SetFlag(TEXT("tutorial_complete"));
    }
    return TutorialStep;
}

FString UHavocMountainShieldMission::GetTutorialPrompt(EHavocTutorialStep Step)
{
    switch (Step)
    {
    case EHavocTutorialStep::Move: return TEXT("Move with W A S D. Walk to the marker.");
    case EHavocTutorialStep::Aim: return TEXT("Aim with the mouse. Turn to face the target dummy.");
    case EHavocTutorialStep::LaunchRaven: return TEXT("Press R to launch the Raven drone.");
    case EHavocTutorialStep::MoveRaven: return TEXT("Fly the Raven to the ridge marker (W A S D, mouse to steer).");
    case EHavocTutorialStep::ObserveContact: return TEXT("Look at the enemy contact below the Raven.");
    case EHavocTutorialStep::ConfirmContact: return TEXT("Press E to mark the contact.");
    case EHavocTutorialStep::ReturnToMarine: return TEXT("Press Q to switch back to the Marine.");
    default: return FString();
    }
}

bool UHavocMountainShieldMission::HoldEcho()
{
    if (!InState(TEXT("M01_HOLD_ECHO"))) { return false; }
    Runner->SetFlag(TEXT("echo_held"));
    return true;
}

bool UHavocMountainShieldMission::SetUpRidgeStation()
{
    if (!InState(TEXT("M01_RIDGE_STATION")) || RidgeStation == EHavocRidgeStationState::Destroyed) { return false; }
    RidgeStation = EHavocRidgeStationState::Captured;
    Runner->SetFlag(TEXT("ridge_station_set"));
    return true;
}

bool UHavocMountainShieldMission::NotifyCrashSeen()
{
    if (!InState(TEXT("M01_FALCON_DOWN"))) { return false; }
    Runner->SetFlag(TEXT("crash_seen"));
    return true;
}

FString UHavocMountainShieldMission::ChooseCrew(bool bRecover)
{
    Progress();
    return Runner->Choose(bRecover ? TEXT("recover_crew") : TEXT("abandon_crew"));
}

int32 UHavocMountainShieldMission::RecoverCrewMember()
{
    if (!InState(TEXT("M01_CREW_RECOVERY")) || !bCrewLocated) { return CrewRecovered; }
    CrewRecovered = FMath::Min(CrewSize, CrewRecovered + 1);
    if (CrewRecovered >= CrewSize)
    {
        Runner->SetFlag(TEXT("crew_evacuated"));
    }
    return CrewRecovered;
}

bool UHavocMountainShieldMission::RepelCounterattack()
{
    if (!InState(TEXT("M01_COUNTERATTACK")) || Counterattack != EHavocCounterattackState::Active) { return false; }
    Counterattack = EHavocCounterattackState::Repelled;
    Runner->SetFlag(TEXT("counterattack_repelled"));
    return true;
}

FString UHavocMountainShieldMission::LoseCheckpoint()
{
    if (Counterattack == EHavocCounterattackState::Active) { Counterattack = EHavocCounterattackState::CheckpointLost; }
    if (RidgeStation == EHavocRidgeStationState::Captured) { RidgeStation = EHavocRidgeStationState::Lost; }
    return Runner->Fail();
}

FString UHavocMountainShieldMission::Finish()
{
    Progress();
    if (!InState(TEXT("M01_DEBRIEF"))) { return GetState(); }
    return Runner->Choose(bCrewAbandoned ? TEXT("crew_abandoned") : TEXT("crew_recovered"));
}

FHavocEpisode01Result UHavocMountainShieldMission::GetCarryover() const
{
    FHavocEpisode01Result Result;
    Result.Outcome = Runner ? Runner->GetOutcome() : FString();
    Result.bCrewRecovered = !bCrewAbandoned && CrewRecovered >= CrewSize;
    return Result;
}
