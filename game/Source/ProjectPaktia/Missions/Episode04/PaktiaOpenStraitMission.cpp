#include "Missions/Episode04/PaktiaOpenStraitMission.h"

#include "Missions/PaktiaMissionJson.h"
#include "Missions/PaktiaMissionRunner.h"
#include "Missions/Episode04/PaktiaOilTerminal.h"
#include "Missions/Episode04/PaktiaShippingLanes.h"
#include "Missions/Episode04/PaktiaTankerEscort.h"

namespace
{
    const TCHAR* const RealWorldSetting = TEXT("real_world_hypothetical");

    FPaktiaProtectedZoneDef MakeZone(const TCHAR* Id, double X, double Y, double Z, double Radius)
    {
        FPaktiaProtectedZoneDef Zone;
        Zone.Id = Id;
        Zone.Type = TEXT("PROTECTED_LOCATION");
        Zone.Position.X = X;
        Zone.Position.Y = Y;
        Zone.Position.Z = Z;
        Zone.Radius = Radius;
        return Zone;
    }
}

bool UPaktiaOpenStraitMission::Initialize(const FPaktiaMissionDef& Definition, const FString& Episode3Outcome,
    UPaktiaEventSubsystem* Events, FString& OutError)
{
    if (Definition.Setting != RealWorldSetting)
    {
        OutError = TEXT("Episode 04 must be declared a real-world hypothetical scenario");
        UE_LOG(LogPaktiaMission, Error, TEXT("%s"), *OutError);
        return false;
    }

    Rules.BaseReinforcements = Definition.BaseReinforcements;
    Rules.BonusIfEpisode3Succeeded = Definition.BonusIfEpisode3Succeeded;
    Rules.Tankers = Definition.Tankers;
    Rules.TerminalIntactThreshold = Definition.TerminalIntactThreshold;
    Rules.SupplyFlowTarget = Definition.SupplyFlowTarget;
    Reinforcements = Rules.BaseReinforcements + (Episode3Outcome == TEXT("SUCCESS") ? Rules.BonusIfEpisode3Succeeded : 0);
    bAirSupport = false;
    NorthDefences = 0;
    WestDefences = 0;

    Terminal = NewObject<UPaktiaOilTerminal>(this);
    Lanes = NewObject<UPaktiaShippingLanes>(this);
    Escort = NewObject<UPaktiaTankerEscort>(this);
    Runner = NewObject<UPaktiaMissionRunner>(this);

    TMap<FString, FPaktiaEffectHandler> Handlers;
    Handlers.Add(TEXT("landing.north"), [this](const FPaktiaMissionStateDef&) { Reinforcements = FMath::Max(0, Reinforcements - NorthDefences); });
    Handlers.Add(TEXT("landing.west"), [this](const FPaktiaMissionStateDef&) { Reinforcements = FMath::Max(0, Reinforcements - WestDefences); });
    // Protected zones are fixed (GetProtectedZones); kept as a named step for UI and audio.
    Handlers.Add(TEXT("terminal.protect"), [](const FPaktiaMissionStateDef&) {});
    Handlers.Add(TEXT("lanes.seed"), [this](const FPaktiaMissionStateDef&) { if (Lanes->NumLanes() == 0) { Lanes->Seed(4, 3); } });
    Handlers.Add(TEXT("tankers.assemble"), [this](const FPaktiaMissionStateDef&) { if (Escort->NumTankers() == 0) { Escort->Assemble(Rules.Tankers); } });

    return Runner->Initialize(Definition, Handlers, Events, this, OutError);
}

FString UPaktiaOpenStraitMission::SnapshotSystems() const
{
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetObjectField(TEXT("terminal"), Terminal->Snapshot());
    Json->SetObjectField(TEXT("lanes"), Lanes->Snapshot());
    Json->SetObjectField(TEXT("escort"), Escort->Snapshot());
    Json->SetNumberField(TEXT("reinforcements"), Reinforcements);
    Json->SetBoolField(TEXT("airSupport"), bAirSupport);
    return PaktiaMissionJson::ToString(Json);
}

void UPaktiaOpenStraitMission::RestoreSystems(const FString& SnapshotJson)
{
    const TSharedRef<FJsonObject> Json = PaktiaMissionJson::FromString(SnapshotJson);
    const TSharedPtr<FJsonObject>* Child = nullptr;
    if (Json->TryGetObjectField(TEXT("terminal"), Child) && Child) { Terminal->Restore(*Child); }
    if (Json->TryGetObjectField(TEXT("lanes"), Child) && Child) { Lanes->Restore(*Child); }
    if (Json->TryGetObjectField(TEXT("escort"), Child) && Child) { Escort->Restore(*Child); }
    Json->TryGetNumberField(TEXT("reinforcements"), Reinforcements);
    Json->TryGetBoolField(TEXT("airSupport"), bAirSupport);
}

FString UPaktiaOpenStraitMission::GetState() const
{
    return Runner ? Runner->GetState() : FString();
}

FString UPaktiaOpenStraitMission::Progress()
{
    FString Before;
    do
    {
        Before = Runner->GetState();
        Runner->Advance();
    } while (Runner->GetState() != Before);
    return Runner->GetState();
}

bool UPaktiaOpenStraitMission::ScoutBeaches(int32 North, int32 West)
{
    if (Runner->GetState() != TEXT("E04_BEACH_RECON"))
    {
        return false;
    }
    NorthDefences = FMath::Max(0, North);
    WestDefences = FMath::Max(0, West);
    Runner->SetFlag(TEXT("beaches_scouted"));
    return true;
}

FString UPaktiaOpenStraitMission::ChooseBeach(const FString& BeachBranch)
{
    Progress();
    return Runner->Choose(BeachBranch);
}

void UPaktiaOpenStraitMission::SecureBeachhead()
{
    if (Runner->GetState().EndsWith(TEXT("_LANDING"), ESearchCase::CaseSensitive))
    {
        Runner->SetFlag(TEXT("beachhead_secure"));
    }
}

void UPaktiaOpenStraitMission::SecureAirstrip()
{
    if (!Runner->GetState().Contains(TEXT("AIRSTRIP"), ESearchCase::CaseSensitive))
    {
        return;
    }
    Runner->SetFlag(TEXT("airstrip_secure"));
    bAirSupport = true;
}

void UPaktiaOpenStraitMission::SecureTerminal()
{
    if (Runner->GetState().Contains(TEXT("TERMINAL"), ESearchCase::CaseSensitive))
    {
        Runner->SetFlag(TEXT("terminal_secure"));
    }
}

bool UPaktiaOpenStraitMission::DamageTerminal(FName FacilityId, double Amount)
{
    return Terminal->Damage(FacilityId, Amount);
}

int32 UPaktiaOpenStraitMission::SpendReinforcements(int32 Count)
{
    Reinforcements = FMath::Max(0, Reinforcements - FMath::Max(0, Count));
    if (Reinforcements == 0 && !Runner->HasFlag(TEXT("terminal_secure")))
    {
        Runner->Fail();
    }
    return Reinforcements;
}

int32 UPaktiaOpenStraitMission::RevealHazards(FName LaneId, int32 Count)
{
    return Runner->GetState() == TEXT("E04_LANE_CLEARANCE") ? Lanes->Reveal(LaneId, Count) : 0;
}

bool UPaktiaOpenStraitMission::ClearHazard(FName LaneId)
{
    if (Runner->GetState() != TEXT("E04_LANE_CLEARANCE") || !Lanes->Clear(LaneId))
    {
        return false;
    }
    if (Lanes->AllOpen())
    {
        Runner->SetFlag(TEXT("lanes_open"));
    }
    return true;
}

bool UPaktiaOpenStraitMission::StartTransit(const FString& TankerId, FName LaneId)
{
    if (Runner->GetState() != TEXT("E04_TANKER_ESCORT"))
    {
        return false;
    }
    return Escort->Begin(TankerId, LaneId, Lanes->IsOpen(LaneId));
}

void UPaktiaOpenStraitMission::TankerArrived(const FString& TankerId)
{
    Escort->Arrive(TankerId);
    CheckEscort();
}

void UPaktiaOpenStraitMission::DamageTanker(const FString& TankerId, double Amount)
{
    Escort->Damage(TankerId, Amount);
    CheckEscort();
}

FString UPaktiaOpenStraitMission::Finish()
{
    Progress();
    if (Runner->GetState() != TEXT("E04_DEBRIEF"))
    {
        return Runner->GetState();
    }
    const bool bFull = Terminal->GetIntegrity() >= Rules.TerminalIntactThreshold
        && Terminal->CanExport()
        && Escort->GetSupplyFlow() >= Rules.SupplyFlowTarget;
    return Runner->Choose(bFull ? TEXT("full_success") : TEXT("partial"));
}

FPaktiaEpisode04Result UPaktiaOpenStraitMission::GetResult() const
{
    const TArray<FString> History = Runner->GetHistory();
    FPaktiaEpisode04Result Result;
    Result.Outcome = Runner->GetOutcome();
    Result.Beach = History.Contains(TEXT("E04_NORTH_LANDING")) ? TEXT("NORTH")
        : History.Contains(TEXT("E04_WEST_LANDING")) ? TEXT("WEST") : TEXT("");
    Result.TerminalIntegrity = Terminal->GetIntegrity();
    Result.bCanExport = Terminal->CanExport();
    Result.TankersThrough = Escort->GetThrough();
    Result.TankersLost = Escort->GetLost();
    Result.SupplyFlow = Escort->GetSupplyFlow();
    return Result;
}

FString UPaktiaOpenStraitMission::GetSummaryJson() const
{
    const FPaktiaEpisode04Result R = GetResult();
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("mission"), TEXT("Operation Open Strait"));
    Json->SetStringField(TEXT("state"), Runner->GetState());
    Json->SetStringField(TEXT("objective"), Runner->GetObjective());
    Json->SetNumberField(TEXT("reinforcements"), Reinforcements);
    Json->SetBoolField(TEXT("airSupport"), bAirSupport);
    Json->SetNumberField(TEXT("episode"), R.Episode);
    if (!R.Outcome.IsEmpty()) { Json->SetStringField(TEXT("outcome"), R.Outcome); }
    if (!R.Beach.IsEmpty()) { Json->SetStringField(TEXT("beach"), R.Beach); }
    Json->SetNumberField(TEXT("terminalIntegrity"), R.TerminalIntegrity);
    Json->SetBoolField(TEXT("canExport"), R.bCanExport);
    Json->SetNumberField(TEXT("tankersThrough"), R.TankersThrough);
    Json->SetNumberField(TEXT("tankersLost"), R.TankersLost);
    Json->SetNumberField(TEXT("supplyFlow"), R.SupplyFlow);
    Json->SetObjectField(TEXT("lanes"), Lanes->Snapshot());
    Json->SetObjectField(TEXT("tankers"), Escort->Snapshot());
    return PaktiaMissionJson::ToString(Json);
}

const TArray<FPaktiaProtectedZoneDef>& UPaktiaOpenStraitMission::GetProtectedZones()
{
    static const TArray<FPaktiaProtectedZoneDef> Zones = {
        MakeZone(TEXT("kharg_oil_terminal"), 2200.0, 0.0, 900.0, 450.0),
        MakeZone(TEXT("kharg_town"), 1500.0, 0.0, 1600.0, 350.0)
    };
    return Zones;
}

bool UPaktiaOpenStraitMission::IsInProtectedZone(double X, double Y, double Z, FString& OutZoneId)
{
    for (const FPaktiaProtectedZoneDef& Zone : GetProtectedZones())
    {
        const double Distance = FMath::Sqrt(FMath::Square(Zone.Position.X - X) + FMath::Square(Zone.Position.Y - Y) + FMath::Square(Zone.Position.Z - Z));
        if (Distance <= Zone.Radius)
        {
            OutZoneId = Zone.Id;
            return true;
        }
    }
    OutZoneId.Reset();
    return false;
}

void UPaktiaOpenStraitMission::CheckEscort()
{
    if (Runner->GetState() != TEXT("E04_TANKER_ESCORT") || !Escort->IsSettled())
    {
        return;
    }
    if (Escort->GetThrough() == 0)
    {
        Runner->Fail();
        return;
    }
    Runner->SetFlag(TEXT("escort_complete"));
    Progress();
}
