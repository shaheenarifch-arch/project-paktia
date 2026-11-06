#include "Missions/Episode02/HavocSilentCorridorMission.h"

#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/Episode02/HavocConvoyEscort.h"
#include "Missions/Episode02/HavocRelayNetwork.h"

namespace
{
    const TCHAR* RouteName(const TArray<FString>& History)
    {
        return History.Contains(TEXT("E02_RIVERBED_DETOUR")) ? TEXT("RIVERBED")
            : History.Contains(TEXT("E02_DRAINAGE_INFILTRATION")) ? TEXT("RIVER_DRAINAGE") : TEXT("MAIN_ROAD");
    }
}

bool UHavocSilentCorridorMission::Initialize(const FHavocMissionDef& Definition, bool bInCrewRecovered, UHavocEventSubsystem* Events, FString& OutError)
{
    bCrewRecovered = bInCrewRecovered;
    Contacts.Reset();
    AmbushContacts.Reset();

    Convoy = NewObject<UHavocConvoyEscort>(this);
    Relays = NewObject<UHavocRelayNetwork>(this);
    Runner = NewObject<UHavocMissionRunner>(this);

    TMap<FString, FHavocEffectHandler> Handlers;
    Handlers.Add(TEXT("convoy.form"), [this](const FHavocMissionStateDef&) { Convoy->Form(bCrewRecovered); });
    // Relay Kilo is placed by the player: see PlaceRelayKilo().
    Handlers.Add(TEXT("relays.prepare"), [](const FHavocMissionStateDef&) {});
    Handlers.Add(TEXT("convoy.depart"), [this](const FHavocMissionStateDef&) { Convoy->Depart(EHavocConvoyRoute::MainRoad); });
    Handlers.Add(TEXT("convoy.halt"), [this](const FHavocMissionStateDef&) { Convoy->Halt(); });
    Handlers.Add(TEXT("event.communications_outage"), [this](const FHavocMissionStateDef&) { Relays->JamAll(); });
    Handlers.Add(TEXT("event.bridge_obstruction"), [this](const FHavocMissionStateDef&) { Runner->ClearFlag(TEXT("bridge_cleared")); });
    Handlers.Add(TEXT("convoy.detour_riverbed"), [this](const FHavocMissionStateDef&) { Convoy->Depart(EHavocConvoyRoute::Riverbed); });
    Handlers.Add(TEXT("convoy.ambush"), [this](const FHavocMissionStateDef&) { StartAmbush(); });
    Handlers.Add(TEXT("route.drainage"), [this](const FHavocMissionStateDef&) { Convoy->Halt(); });
    Handlers.Add(TEXT("convoy.resume"), [this](const FHavocMissionStateDef&) { Convoy->Resume(); });
    Handlers.Add(TEXT("staging.establish"), [this](const FHavocMissionStateDef&) { Convoy->Arrive(); });

    return Runner->Initialize(Definition, Handlers, Events, this, OutError);
}

FString UHavocSilentCorridorMission::SnapshotSystems() const
{
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetObjectField(TEXT("convoy"), Convoy->Snapshot());
    Json->SetObjectField(TEXT("relays"), Relays->Snapshot());
    Json->SetArrayField(TEXT("ambush"), HavocMissionJson::StringArray(AmbushContacts));
    return HavocMissionJson::ToString(Json);
}

void UHavocSilentCorridorMission::RestoreSystems(const FString& SnapshotJson)
{
    const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(SnapshotJson);
    const TSharedPtr<FJsonObject>* Child = nullptr;
    if (Json->TryGetObjectField(TEXT("convoy"), Child) && Child) { Convoy->Restore(*Child); }
    if (Json->TryGetObjectField(TEXT("relays"), Child) && Child) { Relays->RestoreSnapshot(*Child); }
    AmbushContacts = HavocMissionJson::ReadStringArray(*Json, TEXT("ambush"));
}

FString UHavocSilentCorridorMission::GetState() const
{
    return Runner ? Runner->GetState() : FString();
}

FString UHavocSilentCorridorMission::Progress()
{
    FString Before;
    do
    {
        Before = Runner->GetState();
        Runner->Advance();
    } while (Runner->GetState() != Before);
    return Runner->GetState();
}

bool UHavocSilentCorridorMission::PlaceRelayKilo(const FVector& DronePosition)
{
    if (Runner->GetState() != TEXT("E02_RELAY_KILO") || Relays->Classify(DronePosition) == EHavocSignalState::Lost)
    {
        return false;
    }
    Relays->Place(TEXT("relay-kilo"), DronePosition);
    Runner->SetFlag(TEXT("relay_kilo_online"));
    return true;
}

bool UHavocSilentCorridorMission::ScoutCorridor(const TArray<FHavocE02Contact>& Observed, const FVector& At, double Now)
{
    if (Runner->GetState() != TEXT("E02_CORRIDOR_SCOUT"))
    {
        return false;
    }
    const EHavocSignalState Link = Relays->Classify(At);
    if (Link == EHavocSignalState::Lost || Link == EHavocSignalState::Critical)
    {
        return false;
    }
    for (const FHavocE02Contact& C : Observed)
    {
        UpsertContact(C.Id, C.Position, Relays->SignalAt(At), Now);
    }
    Runner->SetFlag(TEXT("corridor_scouted"));
    return true;
}

bool UHavocSilentCorridorMission::RestoreRelayKilo(const FVector& SquadPosition)
{
    if (!Relays->Restore(TEXT("relay-kilo"), SquadPosition))
    {
        return false;
    }
    Runner->SetFlag(TEXT("relay_kilo_restored"));
    return true;
}

FString UHavocSilentCorridorMission::ChooseRoute(const FString& RouteBranch)
{
    return Runner->Choose(RouteBranch);
}

void UHavocSilentCorridorMission::DamageVehicle(const FString& VehicleId, double Amount)
{
    Convoy->ApplyDamage(VehicleId, Amount);
    if (Convoy->IsMissionCritical())
    {
        Runner->Fail();
    }
}

bool UHavocSilentCorridorMission::ClearAmbushContact(const FString& ContactId, double Now)
{
    if (!AmbushContacts.Contains(ContactId))
    {
        return false;
    }
    AmbushContacts.Remove(ContactId);
    for (FHavocE02Contact& C : Contacts)
    {
        if (C.Id == ContactId)
        {
            C.bTracked = false;
            C.Time = Now;
        }
    }
    if (AmbushContacts.Num() == 0 && Runner->GetState() == TEXT("E02_RIVERBED_DETOUR"))
    {
        Runner->SetFlag(TEXT("ambush_repelled"));
        Convoy->Resume();
    }
    return true;
}

void UHavocSilentCorridorMission::ClearFarBank()
{
    if (Runner->GetState() == TEXT("E02_DRAINAGE_INFILTRATION"))
    {
        Runner->SetFlag(TEXT("far_bank_clear"));
    }
}

void UHavocSilentCorridorMission::ClearBridge()
{
    if (Runner->GetState() == TEXT("E02_BRIDGE_CLEARED") && Convoy->IsWardenOperational())
    {
        Runner->SetFlag(TEXT("bridge_cleared"));
    }
}

void UHavocSilentCorridorMission::ClearStaging()
{
    if (Runner->GetState() == TEXT("E02_SOUTH_GATE_APPROACH"))
    {
        Runner->SetFlag(TEXT("staging_clear"));
    }
}

FString UHavocSilentCorridorMission::Finish()
{
    if (Runner->GetState() != TEXT("E02_STAGING_ESTABLISHED"))
    {
        return Runner->GetState();
    }
    return Runner->Choose(Convoy->IsFullStrength() ? TEXT("full_strength") : TEXT("with_losses"));
}

FHavocEpisode02Carryover UHavocSilentCorridorMission::GetCarryover() const
{
    FHavocEpisode02Carryover C;
    C.Outcome = Runner->GetOutcome();
    C.bCrewDelivered = Convoy->IsCrewSafe() && Convoy->GetState() == EHavocConvoyState::Arrived && C.Outcome != TEXT("FAILED");
    C.NomadsRemaining = Convoy->GetNomadsRemaining();
    C.bWardenOperational = Convoy->IsWardenOperational();
    C.Route = RouteName(Runner->GetHistory());
    C.RelaysOnline = Relays->GetOnline();
    return C;
}

FString UHavocSilentCorridorMission::GetSummaryJson() const
{
    const FHavocEpisode02Carryover C = GetCarryover();
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("mission"), TEXT("Operation Silent Corridor"));
    Json->SetStringField(TEXT("state"), Runner->GetState());
    Json->SetStringField(TEXT("objective"), Runner->GetObjective());
    Json->SetArrayField(TEXT("flags"), HavocMissionJson::StringArray(Runner->GetFlags()));
    Json->SetObjectField(TEXT("convoy"), Convoy->Snapshot());
    Json->SetObjectField(TEXT("relays"), Relays->Snapshot());
    Json->SetArrayField(TEXT("ambush"), HavocMissionJson::StringArray(AmbushContacts));
    Json->SetNumberField(TEXT("episode"), C.Episode);
    if (!C.Outcome.IsEmpty()) { Json->SetStringField(TEXT("outcome"), C.Outcome); }
    Json->SetBoolField(TEXT("crewDelivered"), C.bCrewDelivered);
    Json->SetNumberField(TEXT("nomadsRemaining"), C.NomadsRemaining);
    Json->SetBoolField(TEXT("wardenOperational"), C.bWardenOperational);
    Json->SetStringField(TEXT("route"), C.Route);
    Json->SetNumberField(TEXT("relaysOnline"), C.RelaysOnline);
    return HavocMissionJson::ToString(Json);
}

void UHavocSilentCorridorMission::StartAmbush()
{
    Convoy->Ambush();
    const FVector P = HavocE02::AmbushNarrows();
    AmbushContacts = { TEXT("e02-ambush-1"), TEXT("e02-ambush-2"), TEXT("e02-ambush-3") };
    for (int32 i = 0; i < AmbushContacts.Num(); ++i)
    {
        UpsertContact(AmbushContacts[i], FVector(P.X + i * 40.0, P.Y, P.Z - i * 25.0), 0.8, 0.0);
    }
}

void UHavocSilentCorridorMission::UpsertContact(const FString& Id, const FVector& Position, double Quality, double Time)
{
    FHavocE02Contact* Existing = Contacts.FindByPredicate([&Id](const FHavocE02Contact& C) { return C.Id == Id; });
    FHavocE02Contact& C = Existing ? *Existing : Contacts.AddDefaulted_GetRef();
    C.Id = Id;
    C.Position = Position;
    C.Quality = Quality;
    C.Time = Time;
    C.bTracked = true;
}
