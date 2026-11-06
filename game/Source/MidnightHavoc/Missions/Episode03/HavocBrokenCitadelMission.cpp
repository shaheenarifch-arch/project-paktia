#include "Missions/Episode03/HavocBrokenCitadelMission.h"

#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionRunner.h"

namespace
{
    // Which Gardun sectors each phase fights over (docs/gardun-map.md).
    TArray<FString> PhaseSectors(int32 Order)
    {
        switch (Order)
        {
        case 1: return { TEXT("airfield"), TEXT("industrial") };
        case 2: return { TEXT("west_ridge"), TEXT("government") };
        case 3: return { TEXT("old_gardun") };
        case 4: return { TEXT("mountain_fort"), TEXT("east_power") };
        case 5: return { TEXT("citadel") };
        default: return {};
        }
    }

    FString PhaseStateId(int32 Order)
    {
        return FString::Printf(TEXT("E03_PHASE_%d"), Order);
    }

    void AddUnique(TArray<FString>& Set, const FString& Value)
    {
        Set.AddUnique(Value);
    }
}

bool UHavocBrokenCitadelMission::BuildDefinition(const FHavocMissionDef& Episode, const FHavocMissionDef& Stronghold,
    FHavocMissionDef& OutDefinition, FString& OutError)
{
    if (Episode.Stronghold != Stronghold.Id)
    {
        OutError = FString::Printf(TEXT("Episode 03 expects %s, got %s"), *Episode.Stronghold, *Stronghold.Id);
        return false;
    }
    if (Stronghold.Phases.Num() == 0)
    {
        OutError = TEXT("Episode 03: the stronghold definition has no phases");
        return false;
    }
    TArray<FHavocMissionPhaseDef> Sorted = Stronghold.Phases;
    Sorted.Sort([](const FHavocMissionPhaseDef& A, const FHavocMissionPhaseDef& B) { return A.Order < B.Order; });

    FHavocMissionDef Def;
    Def.Id = Episode.Id;
    Def.Episode = Episode.Episode;
    Def.Name = Episode.Name;
    Def.bFictional = true;
    Def.Setting = TEXT("fictional");
    Def.Map = Episode.Map;
    Def.Start = TEXT("E03_BRIEFING");
    Def.FailState = TEXT("E03_ASSAULT_FAILED");

    {
        FHavocMissionStateDef S;
        S.Id = TEXT("E03_BRIEFING");
        S.Objective = Episode.Briefing;
        S.Next = TEXT("E03_STAGING");
        Def.States.Add(S);
    }
    {
        FHavocMissionStateDef S;
        S.Id = TEXT("E03_STAGING");
        S.Objective = TEXT("Stage at South Gate and assign squads to the first objectives.");
        S.Requires.Add(TEXT("staging_ready"));
        S.bCheckpoint = true;
        S.Next = TEXT("E03_PHASE_1");
        Def.States.Add(S);
    }
    Def.Sectors.Add(TEXT("south_gate"));
    for (int32 i = 0; i < Sorted.Num(); ++i)
    {
        const FHavocMissionPhaseDef& P = Sorted[i];
        FHavocMissionStateDef S;
        S.Id = PhaseStateId(P.Order);
        const FString* Custom = Episode.PhaseObjectives.Find(FString::FromInt(P.Order));
        S.Objective = Custom ? *Custom : FString::Printf(TEXT("%s: %s"), *P.Name, *FString::Join(P.Objectives, TEXT(", ")));
        S.Requires.Add(FString::Printf(TEXT("phase_%d_complete"), P.Order));
        S.bCheckpoint = true;
        if (P.Order == Episode.SurpriseEventPhase)
        {
            S.Effects.Add(TEXT("surprise.trigger"));
        }
        S.Next = i + 1 < Sorted.Num() ? PhaseStateId(Sorted[i + 1].Order) : FString(TEXT("E03_CITADEL_SECURED"));
        Def.States.Add(S);
        for (const FString& Sector : PhaseSectors(P.Order))
        {
            Def.Sectors.Add(Sector);
        }
    }
    {
        FHavocMissionStateDef S;
        S.Id = TEXT("E03_CITADEL_SECURED");
        S.Objective = TEXT("Gardun Citadel secured. Operation Broken Citadel complete.");
        S.bTerminal = true;
        S.Outcome = TEXT("SUCCESS");
        Def.States.Add(S);
    }
    {
        FHavocMissionStateDef S;
        S.Id = TEXT("E03_ASSAULT_FAILED");
        S.Objective = TEXT("Reinforcements exhausted. Regroup at South Gate and try again from the last checkpoint.");
        S.bTerminal = true;
        S.Outcome = TEXT("FAILED");
        Def.States.Add(S);
    }
    OutDefinition = MoveTemp(Def);
    return true;
}

bool UHavocBrokenCitadelMission::Initialize(const FHavocMissionDef& Episode, const FHavocMissionDef& Stronghold,
    const TArray<FString>& SurpriseEvents, const FHavocEpisode02Carryover& InCarryover, const FString& Seed,
    UHavocEventSubsystem* Events, FString& OutError)
{
    FHavocMissionDef Def;
    if (!BuildDefinition(Episode, Stronghold, Def, OutError))
    {
        UE_LOG(LogHavocMission, Error, TEXT("%s"), *OutError);
        return false;
    }
    FString Planned;
    if (!FHavocSurpriseEventDirector::Pick(SurpriseEvents, Seed, Planned, OutError))
    {
        UE_LOG(LogHavocMission, Error, TEXT("%s"), *OutError);
        return false;
    }

    EpisodeDef = Episode;
    Phases = Stronghold.Phases;
    Phases.Sort([](const FHavocMissionPhaseDef& A, const FHavocMissionPhaseDef& B) { return A.Order < B.Order; });
    Zones = Episode.ProtectedZones;
    Carryover = InCarryover;
    Captured.Reset();
    Unlocked.Reset();
    Surprise = FHavocSurpriseEventDirector();
    Surprise.SetPlanned(Planned);
    // Episode 02 decides the starting strength: each surviving Nomad brings more troops.
    Reinforcements = Episode.BaseReinforcements + Episode.ReinforcementsPerNomad * FMath::Max(0, Carryover.NomadsRemaining);

    Runner = NewObject<UHavocMissionRunner>(this);
    TMap<FString, FHavocEffectHandler> Handlers;
    Handlers.Add(TEXT("surprise.trigger"), [this](const FHavocMissionStateDef&) { Surprise.Trigger(); });
    return Runner->Initialize(Def, Handlers, Events, this, OutError);
}

FString UHavocBrokenCitadelMission::SnapshotSystems() const
{
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetArrayField(TEXT("captured"), HavocMissionJson::StringArray(Captured));
    Json->SetArrayField(TEXT("unlocked"), HavocMissionJson::StringArray(Unlocked));
    Json->SetNumberField(TEXT("reinforcements"), Reinforcements);
    const TSharedRef<FJsonObject> SurpriseJson = MakeShared<FJsonObject>();
    SurpriseJson->SetBoolField(TEXT("fired"), Surprise.HasFired());
    if (Surprise.IsActive())
    {
        SurpriseJson->SetStringField(TEXT("active"), Surprise.GetActive());
    }
    Json->SetObjectField(TEXT("surprise"), SurpriseJson);
    return HavocMissionJson::ToString(Json);
}

void UHavocBrokenCitadelMission::RestoreSystems(const FString& SnapshotJson)
{
    const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(SnapshotJson);
    Captured = HavocMissionJson::ReadStringArray(*Json, TEXT("captured"));
    Unlocked = HavocMissionJson::ReadStringArray(*Json, TEXT("unlocked"));
    Json->TryGetNumberField(TEXT("reinforcements"), Reinforcements);
    bool bFired = false;
    FString Active;
    const TSharedPtr<FJsonObject>* SurpriseJson = nullptr;
    if (Json->TryGetObjectField(TEXT("surprise"), SurpriseJson) && SurpriseJson && SurpriseJson->IsValid())
    {
        (*SurpriseJson)->TryGetBoolField(TEXT("fired"), bFired);
        (*SurpriseJson)->TryGetStringField(TEXT("active"), Active);
    }
    Surprise.Restore(bFired, Active);
}

FString UHavocBrokenCitadelMission::GetState() const
{
    return Runner ? Runner->GetState() : FString();
}

int32 UHavocBrokenCitadelMission::GetPhase() const
{
    const FString State = GetState();
    const FString Prefix(TEXT("E03_PHASE_"));
    return State.StartsWith(Prefix, ESearchCase::CaseSensitive) ? FCString::Atoi(*State.Mid(Prefix.Len())) : 0;
}

const FHavocMissionPhaseDef* UHavocBrokenCitadelMission::FindPhase(int32 Order) const
{
    return Phases.FindByPredicate([Order](const FHavocMissionPhaseDef& P) { return P.Order == Order; });
}

TArray<FString> UHavocBrokenCitadelMission::GetPhaseObjectives(int32 Phase) const
{
    const FHavocMissionPhaseDef* P = FindPhase(Phase);
    return P ? P->Objectives : TArray<FString>();
}

TArray<FString> UHavocBrokenCitadelMission::GetOpenObjectives() const
{
    TArray<FString> Open;
    if (const FHavocMissionPhaseDef* P = FindPhase(GetPhase()))
    {
        for (const FString& Objective : P->Objectives)
        {
            if (!Captured.Contains(Objective))
            {
                Open.Add(Objective);
            }
        }
    }
    return Open;
}

FString UHavocBrokenCitadelMission::BeginAssault()
{
    Runner->Advance();                          // briefing -> staging
    Runner->SetFlag(TEXT("staging_ready"));
    return Runner->Advance();                   // staging -> phase 1
}

bool UHavocBrokenCitadelMission::CaptureObjective(const FString& ObjectiveId)
{
    const int32 Phase = GetPhase();
    const FHavocMissionPhaseDef* P = FindPhase(Phase);
    if (!P || !P->Objectives.Contains(ObjectiveId) || Captured.Contains(ObjectiveId))
    {
        return false;
    }
    AddUnique(Captured, ObjectiveId);
    ApplyUnlock(ObjectiveId);

    const FString* Rule = EpisodeDef.PhaseRules.Find(FString::FromInt(Phase));
    const bool bAny = Rule && *Rule == TEXT("any");
    bool bDone = !bAny;
    for (const FString& Objective : P->Objectives)
    {
        const bool bTaken = Captured.Contains(Objective);
        if (bAny && bTaken) { bDone = true; break; }
        if (!bAny && !bTaken) { bDone = false; break; }
    }
    if (bDone)
    {
        Runner->SetFlag(FString::Printf(TEXT("phase_%d_complete"), Phase));
        Runner->Advance();
        // A new phase ends the previous phase's surprise event, except the one it started.
        if (GetPhase() != EpisodeDef.SurpriseEventPhase && Surprise.IsActive())
        {
            Surprise.End();
        }
    }
    return true;
}

int32 UHavocBrokenCitadelMission::SpendReinforcements(int32 Count)
{
    Reinforcements = FMath::Max(0, Reinforcements - FMath::Max(0, Count));
    if (Reinforcements == 0)
    {
        Runner->Fail();
    }
    return Reinforcements;
}

bool UHavocBrokenCitadelMission::HasAirSupport() const
{
    return Unlocked.Contains(TEXT("falcon_air_support")) && Carryover.bCrewDelivered;
}

double UHavocBrokenCitadelMission::GetCounterattackStrength() const
{
    return Captured.Contains(TEXT("west_ridge_radio")) && Captured.Contains(TEXT("government_relay")) ? 0.6 : 1.0;
}

FString UHavocBrokenCitadelMission::GetUnlockFor(const FString& ObjectiveId) const
{
    const FString* Unlock = EpisodeDef.Unlocks.Find(ObjectiveId);
    return Unlock ? *Unlock : FString();
}

void UHavocBrokenCitadelMission::ApplyUnlock(const FString& Objective)
{
    if (const FString* Unlock = EpisodeDef.Unlocks.Find(Objective))
    {
        AddUnique(Unlocked, *Unlock);
    }
}

FHavocEpisode03Result UHavocBrokenCitadelMission::GetResult() const
{
    FHavocEpisode03Result R;
    R.Outcome = Runner ? Runner->GetOutcome() : FString();
    R.ReinforcementsLeft = Reinforcements;
    R.ObjectivesCaptured = Captured.Num();
    const TArray<FString> History = Runner ? Runner->GetHistory() : TArray<FString>();
    for (const FHavocMissionPhaseDef& P : Phases)
    {
        if (History.Contains(PhaseStateId(P.Order)))
        {
            R.PhaseReached = FMath::Max(R.PhaseReached, P.Order);
        }
    }
    R.bAegisOnline = IsAegisOnline();
    R.bMedicalStationProtected = IsMedicalStationProtected();
    R.SurpriseEvent = Surprise.GetPlanned();
    return R;
}

FString UHavocBrokenCitadelMission::GetSummaryJson() const
{
    const FHavocEpisode03Result R = GetResult();
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("mission"), EpisodeDef.Name);
    Json->SetStringField(TEXT("state"), GetState());
    Json->SetStringField(TEXT("objective"), Runner->GetObjective());
    Json->SetNumberField(TEXT("episode"), R.Episode);
    if (GetPhase() > 0) { Json->SetNumberField(TEXT("phase"), GetPhase()); }
    if (!R.Outcome.IsEmpty()) { Json->SetStringField(TEXT("outcome"), R.Outcome); }
    Json->SetArrayField(TEXT("openObjectives"), HavocMissionJson::StringArray(GetOpenObjectives()));
    Json->SetArrayField(TEXT("captured"), HavocMissionJson::StringArray(Captured));
    Json->SetArrayField(TEXT("unlocks"), HavocMissionJson::StringArray(Unlocked));
    Json->SetNumberField(TEXT("reinforcements"), Reinforcements);
    Json->SetBoolField(TEXT("airSupport"), HasAirSupport());
    Json->SetNumberField(TEXT("counterattackStrength"), GetCounterattackStrength());
    Json->SetBoolField(TEXT("aegisOnline"), R.bAegisOnline);
    Json->SetBoolField(TEXT("medicalStationProtected"), R.bMedicalStationProtected);
    const TSharedRef<FJsonObject> SurpriseJson = MakeShared<FJsonObject>();
    SurpriseJson->SetStringField(TEXT("planned"), Surprise.GetPlanned());
    if (Surprise.IsActive()) { SurpriseJson->SetStringField(TEXT("active"), Surprise.GetActive()); }
    Json->SetObjectField(TEXT("surpriseEvent"), SurpriseJson);
    return HavocMissionJson::ToString(Json);
}

bool UHavocBrokenCitadelMission::IsInProtectedZone(double X, double Y, double Z, FString& OutZoneId) const
{
    for (const FHavocProtectedZoneDef& Zone : Zones)
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
