#include "Aircraft/HavocHelicopterModel.h"

// ---------------------------------------------------------------- damage

double FHavocHelicopterDamageState::Reduce(double Current, double Amount)
{
    return FMath::Max(0.0, FMath::Min(1.0, Current - FMath::Max(0.0, Amount)));
}

void FHavocHelicopterDamageState::ApplyDamage(double InFlightControl, double InEngine, double InStructure)
{
    FlightControl = Reduce(FlightControl, InFlightControl);
    EngineCondition = Reduce(EngineCondition, InEngine);
    StructuralCondition = Reduce(StructuralCondition, InStructure);
}

EHavocHelicopterDamageLevel FHavocHelicopterDamageState::LevelFor(double Lowest)
{
    if (Lowest <= 0.0) { return EHavocHelicopterDamageLevel::Destroyed; }
    if (Lowest <= 0.15) { return EHavocHelicopterDamageLevel::Disabled; }
    if (Lowest <= 0.35) { return EHavocHelicopterDamageLevel::Critical; }
    if (Lowest <= 0.7) { return EHavocHelicopterDamageLevel::Damaged; }
    return EHavocHelicopterDamageLevel::Operational;
}

EHavocHelicopterDamageLevel FHavocHelicopterDamageState::GetLevel() const
{
    return LevelFor(FMath::Min3(FlightControl, EngineCondition, StructuralCondition));
}

FHavocHelicopterDamageSnapshot FHavocHelicopterDamageState::Snapshot() const
{
    FHavocHelicopterDamageSnapshot Out;
    Out.Level = GetLevel();
    Out.FlightControl = FlightControl;
    Out.EngineCondition = EngineCondition;
    Out.StructuralCondition = StructuralCondition;
    return Out;
}

// ---------------------------------------------------------------- crew

TArray<FHavocCrewMember> FHavocCrewState::MakeFalconCrew()
{
    const auto Make = [](const TCHAR* Id, const TCHAR* Role)
    {
        FHavocCrewMember M;
        M.Id = FName(Id);
        M.Role = FName(Role);
        return M;
    };
    TArray<FHavocCrewMember> Crew;
    Crew.Add(Make(TEXT("falcon-pilot"), TEXT("PILOT")));
    Crew.Add(Make(TEXT("falcon-copilot"), TEXT("COPILOT")));
    Crew.Add(Make(TEXT("falcon-chief"), TEXT("CREW_CHIEF")));
    return Crew;
}

void FHavocCrewState::AssignSeats(const TArray<FString>& Seats)
{
    for (int32 i = 0; i < Members.Num(); ++i)
    {
        Members[i].Seat = Seats.IsValidIndex(i) ? FName(*Seats[i]) : NAME_None;
    }
}

bool FHavocCrewState::Locate(FName Id, EHavocCrewCondition Condition)
{
    for (FHavocCrewMember& M : Members)
    {
        if (M.Id == Id)
        {
            M.bLocated = true;
            M.Condition = Condition;
            return true;
        }
    }
    return false;
}

bool FHavocCrewState::Recover(FName Id)
{
    for (FHavocCrewMember& M : Members)
    {
        if (M.Id == Id)
        {
            if (!M.bLocated || M.Condition == EHavocCrewCondition::Lost)
            {
                return false;
            }
            M.Condition = EHavocCrewCondition::Recovered;
            M.bEvacuated = true;
            return true;
        }
    }
    return false;
}

int32 FHavocCrewState::GetLocatedCount() const
{
    int32 Count = 0;
    for (const FHavocCrewMember& M : Members) { Count += M.bLocated ? 1 : 0; }
    return Count;
}

int32 FHavocCrewState::GetRecoveredCount() const
{
    int32 Count = 0;
    for (const FHavocCrewMember& M : Members) { Count += M.bEvacuated ? 1 : 0; }
    return Count;
}

bool FHavocCrewState::AllRecovered() const
{
    if (Members.Num() == 0)
    {
        return false;
    }
    for (const FHavocCrewMember& M : Members)
    {
        if (!M.bEvacuated && M.Condition != EHavocCrewCondition::Lost)
        {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------- helicopter state

void FHavocHelicopterState::BeginInbound(const FVector& InPosition)
{
    Position = InPosition;
    LastKnownPosition = InPosition;
    FlightState = EHavocHelicopterFlightState::Inbound;
}

void FHavocHelicopterState::UpdatePosition(const FVector& InPosition)
{
    if (FlightState == EHavocHelicopterFlightState::Crashed)
    {
        return;
    }
    Position = InPosition;
    LastKnownPosition = InPosition;
    if (FlightState == EHavocHelicopterFlightState::Inbound)
    {
        FlightState = EHavocHelicopterFlightState::Transit;
    }
}

void FHavocHelicopterState::ReportDistress()
{
    if (FlightState == EHavocHelicopterFlightState::Inbound || FlightState == EHavocHelicopterFlightState::Transit)
    {
        FlightState = EHavocHelicopterFlightState::Distress;
    }
}

void FHavocHelicopterState::AttemptEmergencyLanding(const FVector& InPosition)
{
    Position = InPosition;
    LastKnownPosition = InPosition;
    FlightState = EHavocHelicopterFlightState::EmergencyLanding;
}

void FHavocHelicopterState::RegisterCrash(const FVector& InPosition)
{
    Position = InPosition;
    LastKnownPosition = InPosition;
    FlightState = EHavocHelicopterFlightState::Crashed;
}

void FHavocHelicopterState::RecoverAircraft()
{
    if (FlightState == EHavocHelicopterFlightState::Crashed)
    {
        FlightState = EHavocHelicopterFlightState::Recovered;
    }
}

// ---------------------------------------------------------------- crash site

void FHavocCrashSite::BeginSearch()
{
    if (State == EHavocCrashSiteState::Unknown)
    {
        State = EHavocCrashSiteState::Searching;
    }
}

void FHavocCrashSite::Locate(const FVector& InPosition)
{
    Position = InPosition;
    bHasPosition = true;
    State = EHavocCrashSiteState::Located;
    bSmokeVisible = true;
    bBeaconActive = true;
}

void FHavocCrashSite::Contest()
{
    if (State == EHavocCrashSiteState::Located || State == EHavocCrashSiteState::Secured)
    {
        State = EHavocCrashSiteState::Contested;
    }
}

void FHavocCrashSite::Secure()
{
    if (State == EHavocCrashSiteState::Located || State == EHavocCrashSiteState::Contested)
    {
        State = EHavocCrashSiteState::Secured;
    }
}

void FHavocCrashSite::Abandon()
{
    if (State != EHavocCrashSiteState::RecoveryComplete)
    {
        State = EHavocCrashSiteState::Abandoned;
        bBeaconActive = false;
    }
}

void FHavocCrashSite::CompleteRecovery()
{
    if (State == EHavocCrashSiteState::Secured)
    {
        State = EHavocCrashSiteState::RecoveryComplete;
        bBeaconActive = false;
    }
}

// ---------------------------------------------------------------- recovery zone

void FHavocRecoveryZone::Designate(const FVector& InCenter)
{
    Center = InCenter;
    bHasCenter = true;
    State = EHavocRecoveryZoneState::Designated;
}

void FHavocRecoveryZone::Activate()
{
    if (State == EHavocRecoveryZoneState::Designated)
    {
        State = EHavocRecoveryZoneState::Active;
    }
}

void FHavocRecoveryZone::Contest()
{
    if (State == EHavocRecoveryZoneState::Active || State == EHavocRecoveryZoneState::Secured)
    {
        State = EHavocRecoveryZoneState::Contested;
    }
}

void FHavocRecoveryZone::Secure()
{
    if (State == EHavocRecoveryZoneState::Active || State == EHavocRecoveryZoneState::Contested)
    {
        State = EHavocRecoveryZoneState::Secured;
    }
}

void FHavocRecoveryZone::CompleteEvacuation()
{
    if (State == EHavocRecoveryZoneState::Secured)
    {
        State = EHavocRecoveryZoneState::EvacuationComplete;
    }
}

bool FHavocRecoveryZone::Contains(const FVector& InPosition, double Radius) const
{
    return bHasCenter && FVector::Dist(Center, InPosition) <= Radius;
}
