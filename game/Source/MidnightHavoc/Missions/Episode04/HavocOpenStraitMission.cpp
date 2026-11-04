#include "Missions/Episode04/HavocOpenStraitMission.h"

#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/Episode04/HavocOilTerminal.h"
#include "Missions/Episode04/HavocShippingLanes.h"
#include "Missions/Episode04/HavocTankerEscort.h"

namespace
{
    const TCHAR* const RealWorldSetting = TEXT("real_world_hypothetical");

    FHavocProtectedZoneDef MakeZone(const TCHAR* Id, double X, double Y, double Z, double Radius)
    {
        FHavocProtectedZoneDef Zone;
        Zone.Id = Id;
        Zone.Type = TEXT("PROTECTED_LOCATION");
        Zone.Position.X = X;
        Zone.Position.Y = Y;
        Zone.Position.Z = Z;
        Zone.Radius = Radius;
        return Zone;
    }
}

bool UHavocOpenStraitMission::Initialize(const FHavocMissionDef& Definition, const FString& Episode3Outcome,
    UHavocEventSubsystem* Events, FString& OutError)
{
    if (Definition.Setting != RealWorldSetting)
    {
        OutError = TEXT("Episode 04 must be declared a real-world hypothetical scenario");
        UE_LOG(LogHavocMission, Error, TEXT("%s"), *OutError);
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

    Terminal = NewObject<UHavocOilTerminal>(this);
    Lanes = NewObject<UHavocShippingLanes>(this);
    Escort = NewObject<UHavocTankerEscort>(this);
    Runner = NewObject<UHavocMissionRunner>(this);

    TMap<FString, FHavocEffectHandler> Handlers;
    Handlers.Add(TEXT("landing.north"), [this](const FHavocMissionStateDef&) { Reinforcements = FMath::Max(0, Reinforcements - NorthDefences); });
    Handlers.Add(TEXT("landing.west"), [this](const FHavocMissionStateDef&) { Reinforcements = FMath::Max(0, Reinforcements - WestDefences); });
    // Protected zones are fixed (GetProtectedZones); kept as a named step for UI and audio.
    Handlers.Add(TEXT("terminal.protect"), [](const FHavocMissionStateDef&) {});
    Handlers.Add(TEXT("lanes.seed"), [this](const FHavocMissionStateDef&) { if (Lanes->NumLanes() == 0) { Lanes->Seed(4, 3); } });
    Handlers.Add(TEXT("tankers.assemble"), [this](const FHavocMissionStateDef&) { if (Escort->NumTankers() == 0) { Escort->Assemble(Rules.Tankers); } });

    return Runner->Initialize(Definition, Handlers, Events, this, OutError);
}

FString UHavocOpenStraitMission::SnapshotSystems() const
{
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetObjectField(TEXT("terminal"), Terminal->Snapshot());
    Json->SetObjectField(TEXT("lanes"), Lanes->Snapshot());
    Json->SetObjectField(TEXT("escort"), Escort->Snapshot());
    Json->SetNumberField(TEXT("reinforcements"), Reinforcements);
    Json->SetBoolField(TEXT("airSupport"), bAirSupport);
    return HavocMissionJson::ToString(Json);
}

void UHavocOpenStraitMission::RestoreSystems(const FString& SnapshotJson)
{
    const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(SnapshotJson);
    const TSharedPtr<FJsonObject>* Child = nullptr;
    if (Json->TryGetObjectField(TEXT("terminal"), Child) && Child) { Terminal->Restore(*Child); }
    if (Json->TryGetObjectField(TEXT("lanes"), Child) && Child) { Lanes->Restore(*Child); }
    if (Json->TryGetObjectField(TEXT("escort"), Child) && Child) { Escort->Restore(*Child); }
    Json->TryGetNumberField(TEXT("reinforcements"), Reinforcements);
    Json->TryGetBoolField(TEXT("airSupport"), bAirSupport);
}

FString UHavocOpenStraitMission::GetState() const
{
    return Runner ? Runner->GetState() : FString();
}

FString UHavocOpenStraitMission::Progress()
{
    FString Before;
    do
    {
        Before = Runner->GetState();
        Runner->Advance();
    } while (Runner->GetState() != Before);
    return Runner->GetState();
}

bool UHavocOpenStraitMission::ScoutBeaches(int32 North, int32 West)
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

FString UHavocOpenStraitMission::ChooseBeach(const FString& BeachBranch)
{
    Progress();
    return Runner->Choose(BeachBranch);
}

void UHavocOpenStraitMission::SecureBeachhead()
{
    if (Runner->GetState().EndsWith(TEXT("_LANDING"), ESearchCase::CaseSensitive))
    {
        Runner->SetFlag(TEXT("beachhead_secure"));
    }
}

void UHavocOpenStraitMission::SecureAirstrip()
{
    if (!Runner->GetState().Contains(TEXT("AIRSTRIP"), ESearchCase::CaseSensitive))
    {
        return;
    }
    Runner->SetFlag(TEXT("airstrip_secure"));
    bAirSupport = true;
}

void UHavocOpenStraitMission::SecureTerminal()
{
    if (Runner->GetState().Contains(TEXT("TERMINAL"), ESearchCase::CaseSensitive))
    {
        Runner->SetFlag(TEXT("terminal_secure"));
    }
}

bool UHavocOpenStraitMission::DamageTerminal(FName FacilityId, double Amount)
{
    return Terminal->Damage(FacilityId, Amount);
}

int32 UHavocOpenStraitMission::SpendReinforcements(int32 Count)
{
    Reinforcements = FMath::Max(0, Reinforcements - FMath::Max(0, Count));
    if (Reinforcements == 0 && !Runner->HasFlag(TEXT("terminal_secure")))
    {
        Runner->Fail();
    }
    return Reinforcements;
}

int32 UHavocOpenStraitMission::RevealHazards(FName LaneId, int32 Count)
{
    return Runner->GetState() == TEXT("E04_LANE_CLEARANCE") ? Lanes->Reveal(LaneId, Count) : 0;
}

bool UHavocOpenStraitMission::ClearHazard(FName LaneId)
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

bool UHavocOpenStraitMission::StartTransit(const FString& TankerId, FName LaneId)
{
    if (Runner->GetState() != TEXT("E04_TANKER_ESCORT"))
    {
        return false;
    }
    return Escort->Begin(TankerId, LaneId, Lanes->IsOpen(LaneId));
}

void UHavocOpenStraitMission::TankerArrived(const FString& TankerId)
{
    Escort->Arrive(TankerId);
    CheckEscort();
}

void UHavocOpenStraitMission::DamageTanker(const FString& TankerId, double Amount)
{
    Escort->Damage(TankerId, Amount);
    CheckEscort();
}

FString UHavocOpenStraitMission::Finish()
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

FHavocEpisode04Result UHavocOpenStraitMission::GetResult() const
{
    const TArray<FString> History = Runner->GetHistory();
    FHavocEpisode04Result Result;
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

FString UHavocOpenStraitMission::GetSummaryJson() const
{
    const FHavocEpisode04Result R = GetResult();
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
    return HavocMissionJson::ToString(Json);
}

const TArray<FHavocProtectedZoneDef>& UHavocOpenStraitMission::GetProtectedZones()
{
    static const TArray<FHavocProtectedZoneDef> Zones = {
        MakeZone(TEXT("kharg_oil_terminal"), 2200.0, 0.0, 900.0, 450.0),
        MakeZone(TEXT("kharg_town"), 1500.0, 0.0, 1600.0, 350.0)
    };
    return Zones;
}

bool UHavocOpenStraitMission::IsInProtectedZone(double X, double Y, double Z, FString& OutZoneId)
{
    for (const FHavocProtectedZoneDef& Zone : GetProtectedZones())
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

void UHavocOpenStraitMission::CheckEscort()
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
