#include "Aircraft/HavocCrashSequence.h"

FHavocCrashSequence::FHavocCrashSequence()
{
    Crew.Members = FHavocCrewState::MakeFalconCrew();
}

void FHavocCrashSequence::BeginInbound(FHavocHelicopterState& Helicopter, const FVector& Position)
{
    Helicopter.BeginInbound(Position);
    State = EHavocCrashSequenceState::AircraftInbound;
}

void FHavocCrashSequence::TriggerDistress(FHavocHelicopterState& Helicopter)
{
    if (State != EHavocCrashSequenceState::AircraftInbound)
    {
        return;
    }
    Helicopter.Damage.ApplyDamage(0.45, 0.55, 0.25);
    Helicopter.ReportDistress();
    State = EHavocCrashSequenceState::DistressReceived;
}

void FHavocCrashSequence::LoseSignal()
{
    if (State == EHavocCrashSequenceState::DistressReceived)
    {
        State = EHavocCrashSequenceState::SignalLost;
    }
}

void FHavocCrashSequence::RegisterCrash(FHavocHelicopterState& Helicopter, const FVector& Position)
{
    if (State != EHavocCrashSequenceState::DistressReceived && State != EHavocCrashSequenceState::SignalLost)
    {
        return;
    }
    Helicopter.RegisterCrash(Position);
    State = EHavocCrashSequenceState::CrashConfirmed;
}

void FHavocCrashSequence::BeginSearch()
{
    if (State != EHavocCrashSequenceState::CrashConfirmed)
    {
        return;
    }
    CrashSite.BeginSearch();
    State = EHavocCrashSequenceState::SearchActive;
}

void FHavocCrashSequence::LocateCrashSite(const FVector& Position)
{
    if (State != EHavocCrashSequenceState::SearchActive)
    {
        return;
    }
    CrashSite.Locate(Position);
    State = EHavocCrashSequenceState::SiteLocated;
}

void FHavocCrashSequence::SecureCrashSite()
{
    if (State != EHavocCrashSequenceState::SiteLocated && State != EHavocCrashSequenceState::RecoveryDefense)
    {
        return;
    }
    CrashSite.Secure();
    State = EHavocCrashSequenceState::SiteSecured;
}

void FHavocCrashSequence::BeginCrewRecovery()
{
    if (State != EHavocCrashSequenceState::SiteSecured || !CrashSite.bHasPosition)
    {
        return;
    }
    RecoveryZone.Designate(CrashSite.Position);
    RecoveryZone.Activate();
    State = EHavocCrashSequenceState::CrewRecovery;
}

void FHavocCrashSequence::StartRecoveryDefense()
{
    if (State != EHavocCrashSequenceState::CrewRecovery)
    {
        return;
    }
    CrashSite.Contest();
    RecoveryZone.Contest();
    State = EHavocCrashSequenceState::RecoveryDefense;
}

bool FHavocCrashSequence::CompleteEvacuation()
{
    if (!Crew.AllRecovered())
    {
        return false;
    }
    CrashSite.Secure();
    CrashSite.CompleteRecovery();
    RecoveryZone.Secure();
    RecoveryZone.CompleteEvacuation();
    State = EHavocCrashSequenceState::EvacuationComplete;
    return true;
}

void FHavocCrashSequence::Abandon()
{
    CrashSite.Abandon();
    State = EHavocCrashSequenceState::Abandoned;
}
