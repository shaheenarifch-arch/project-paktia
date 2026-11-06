#include "Missions/Episode01/HavocMountainShield.h"

namespace
{
    // Positions from HelicopterCrashEvent.ts.
    const FVector InboundPos(920.0, 210.0, 680.0);
    const FVector CrashPos(740.0, 22.0, 910.0);
}

EHavocTutorialStep FHavocDroneTutorial::Advance(EHavocTutorialStep Completed)
{
    if (Step == Completed && Step != EHavocTutorialStep::Complete)
    {
        Step = static_cast<EHavocTutorialStep>(static_cast<uint8>(Step) + 1);
    }
    return Step;
}

FString FHavocDroneTutorial::GetPrompt() const
{
    switch (Step)
    {
    case EHavocTutorialStep::LaunchRaven:    return TEXT("Press the drone key to launch the Raven.");
    case EHavocTutorialStep::MoveRaven:      return TEXT("Fly the Raven forward with the move keys; aim with the mouse.");
    case EHavocTutorialStep::ObserveContact: return TEXT("Look at the marked enemy contact.");
    case EHavocTutorialStep::ConfirmContact: return TEXT("Mark the contact (interact key).");
    case EHavocTutorialStep::ReturnRaven:    return TEXT("Recall the Raven and switch back to the Marine.");
    default:                                 return FString();
    }
}

void FHavocRidgeStation::Discover() { if (State == EHavocRidgeStationState::Unknown) State = EHavocRidgeStationState::Discovered; }
void FHavocRidgeStation::Contest()
{
    if (State == EHavocRidgeStationState::Discovered || State == EHavocRidgeStationState::Captured)
    {
        State = EHavocRidgeStationState::Contested;
    }
}
void FHavocRidgeStation::Capture() { if (State != EHavocRidgeStationState::Destroyed) State = EHavocRidgeStationState::Captured; }
void FHavocRidgeStation::Destroy() { State = EHavocRidgeStationState::Destroyed; }
void FHavocRidgeStation::Lose() { if (State == EHavocRidgeStationState::Captured) State = EHavocRidgeStationState::Lost; }

void FHavocCounterattack::Begin() { if (State == EHavocCounterattackState::Preparing) State = EHavocCounterattackState::Active; }
void FHavocCounterattack::Repel() { if (State == EHavocCounterattackState::Active) State = EHavocCounterattackState::Repelled; }
void FHavocCounterattack::LoseCheckpoint() { if (State == EHavocCounterattackState::Active) State = EHavocCounterattackState::CheckpointLost; }

const TArray<FString>& FHavocMountainShield::GetStates()
{
    static const TArray<FString> States = {
        TEXT("M01_INIT"), TEXT("M01_BRIEFING"), TEXT("M01_DEPLOY"), TEXT("M01_DRONE_TUTORIAL"),
        TEXT("M01_RECON"), TEXT("M01_APPROACH"), TEXT("M01_ECHO_ASSAULT"), TEXT("M01_ECHO_SECURED"),
        TEXT("M01_WARDEN_DEPLOYED"), TEXT("M01_CONVOY"), TEXT("M01_RIDGE_RECON"),
        TEXT("M01_RIDGE_STATION_DISCOVERED"), TEXT("M01_RIDGE_ASSAULT"),
        TEXT("M01_RIDGE_STATION_CAPTURED"), TEXT("M01_AEGIS_ONLINE"), TEXT("M01_TARGET_DESIGNATION"),
        TEXT("M01_COUNTERATTACK"), TEXT("M01_HELICOPTER_INBOUND"), TEXT("M01_HELICOPTER_DISTRESS"),
        TEXT("M01_HELICOPTER_SIGNAL_LOST"), TEXT("M01_HELICOPTER_CRASH"), TEXT("M01_CRASH_SITE_RECON"),
        TEXT("M01_CRASH_SITE_LOCATED"), TEXT("M01_CRASH_SITE_SECURE"), TEXT("M01_CREW_RECOVERY"),
        TEXT("M01_RECOVERY_DEFENSE"), TEXT("M01_EVAC_COMPLETE"), TEXT("M01_FINAL_DRONE_DEFENSE"),
        TEXT("M01_FINAL_HOLD"), TEXT("M01_REGROUP"), TEXT("M01_COMPLETE")
    };
    return States;
}

FHavocMountainShield::FHavocMountainShield()
{
    Helicopter.Id = TEXT("falcon-2");
    Helicopter.Callsign = TEXT("FALCON-2");
    CrashSequence.Crew.Members = FHavocCrewState::MakeFalconCrew();
}

void FHavocMountainShield::Transition(const FString& Next)
{
    State = Next;
    if (Next == TEXT("M01_RIDGE_STATION_CAPTURED"))
    {
        RidgeStation.Capture();
        bAegisOnline = true;
    }
    else if (Next == TEXT("M01_COUNTERATTACK"))        { Counterattack.BeginPreparation(); }
    else if (Next == TEXT("M01_HELICOPTER_INBOUND"))   { CrashSequence.BeginInbound(Helicopter, InboundPos); }
    else if (Next == TEXT("M01_HELICOPTER_DISTRESS"))  { CrashSequence.TriggerDistress(Helicopter); }
    else if (Next == TEXT("M01_HELICOPTER_SIGNAL_LOST")) { CrashSequence.LoseSignal(); }
    else if (Next == TEXT("M01_HELICOPTER_CRASH"))     { CrashSequence.RegisterCrash(Helicopter, CrashPos); }
    else if (Next == TEXT("M01_CRASH_SITE_RECON"))     { CrashSequence.BeginSearch(); }
    else if (Next == TEXT("M01_CRASH_SITE_LOCATED"))   { CrashSequence.LocateCrashSite(CrashPos); }
    else if (Next == TEXT("M01_CRASH_SITE_SECURE"))    { CrashSequence.SecureCrashSite(); }
    else if (Next == TEXT("M01_CREW_RECOVERY"))
    {
        CrashSequence.BeginCrewRecovery();
        CrashSequence.Crew.Locate(TEXT("falcon-pilot"), EHavocCrewCondition::Injured);
        CrashSequence.Crew.Locate(TEXT("falcon-copilot"), EHavocCrewCondition::Critical);
        CrashSequence.Crew.Locate(TEXT("falcon-chief"), EHavocCrewCondition::Operational);
    }
    else if (Next == TEXT("M01_RECOVERY_DEFENSE"))     { CrashSequence.StartRecoveryDefense(); }
    else if (Next == TEXT("M01_EVAC_COMPLETE"))
    {
        CrashSequence.Crew.Recover(TEXT("falcon-pilot"));
        CrashSequence.Crew.Recover(TEXT("falcon-copilot"));
        CrashSequence.Crew.Recover(TEXT("falcon-chief"));
        // Recovery defense leaves the site contested: secure it again before completing evacuation.
        CrashSequence.SecureCrashSite();
        CrashSequence.CompleteEvacuation();
    }
    else if (Next == TEXT("M01_COMPLETE"))             { bAegisOnline = false; }
}

FString FHavocMountainShield::ContinueCrashSequence()
{
    static const TCHAR* Chain[][2] = {
        { TEXT("M01_COUNTERATTACK"), TEXT("M01_HELICOPTER_INBOUND") },
        { TEXT("M01_HELICOPTER_INBOUND"), TEXT("M01_HELICOPTER_DISTRESS") },
        { TEXT("M01_HELICOPTER_DISTRESS"), TEXT("M01_HELICOPTER_SIGNAL_LOST") },
        { TEXT("M01_HELICOPTER_SIGNAL_LOST"), TEXT("M01_HELICOPTER_CRASH") },
        { TEXT("M01_HELICOPTER_CRASH"), TEXT("M01_CRASH_SITE_RECON") },
        { TEXT("M01_CRASH_SITE_RECON"), TEXT("M01_CRASH_SITE_LOCATED") },
        { TEXT("M01_CRASH_SITE_LOCATED"), TEXT("M01_CRASH_SITE_SECURE") },
        { TEXT("M01_CRASH_SITE_SECURE"), TEXT("M01_CREW_RECOVERY") },
        { TEXT("M01_CREW_RECOVERY"), TEXT("M01_RECOVERY_DEFENSE") },
        { TEXT("M01_RECOVERY_DEFENSE"), TEXT("M01_EVAC_COMPLETE") },
        { TEXT("M01_EVAC_COMPLETE"), TEXT("M01_FINAL_DRONE_DEFENSE") },
    };
    for (const TCHAR* const (&Pair)[2] : Chain)
    {
        if (State == Pair[0])
        {
            Transition(Pair[1]);
            break;
        }
    }
    return State;
}

void FHavocMountainShield::AbandonCrashRecovery()
{
    CrashSequence.Abandon();
    Transition(TEXT("M01_FINAL_DRONE_DEFENSE"));
}

bool FHavocMountainShield::WasCrewRecovered() const
{
    // Not AllRecovered(): that also counts lost crew, which is the abandoned case.
    const int32 Total = CrashSequence.Crew.Members.Num();
    return Total > 0 && CrashSequence.Crew.GetRecoveredCount() == Total;
}
