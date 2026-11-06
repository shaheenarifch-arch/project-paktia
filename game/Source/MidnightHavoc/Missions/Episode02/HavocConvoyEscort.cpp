#include "Missions/Episode02/HavocConvoyEscort.h"

#include "Missions/HavocMissionJson.h"
#include "Missions/Episode04/HavocE04Math.h"

namespace
{
    // Names match the TypeScript snapshot strings.
    const TCHAR* StateToString(EHavocConvoyState State)
    {
        switch (State)
        {
        case EHavocConvoyState::Moving: return TEXT("MOVING");
        case EHavocConvoyState::Halted: return TEXT("HALTED");
        case EHavocConvoyState::Ambushed: return TEXT("AMBUSHED");
        case EHavocConvoyState::Arrived: return TEXT("ARRIVED");
        default: return TEXT("FORMING");
        }
    }

    EHavocConvoyState StateFromString(const FString& Text)
    {
        if (Text == TEXT("MOVING")) { return EHavocConvoyState::Moving; }
        if (Text == TEXT("HALTED")) { return EHavocConvoyState::Halted; }
        if (Text == TEXT("AMBUSHED")) { return EHavocConvoyState::Ambushed; }
        if (Text == TEXT("ARRIVED")) { return EHavocConvoyState::Arrived; }
        return EHavocConvoyState::Forming;
    }

    const TCHAR* RouteToString(EHavocConvoyRoute Route)
    {
        switch (Route)
        {
        case EHavocConvoyRoute::Riverbed: return TEXT("RIVERBED");
        case EHavocConvoyRoute::RiverDrainage: return TEXT("RIVER_DRAINAGE");
        default: return TEXT("MAIN_ROAD");
        }
    }

    EHavocConvoyRoute RouteFromString(const FString& Text)
    {
        if (Text == TEXT("RIVERBED")) { return EHavocConvoyRoute::Riverbed; }
        if (Text == TEXT("RIVER_DRAINAGE")) { return EHavocConvoyRoute::RiverDrainage; }
        return EHavocConvoyRoute::MainRoad;
    }

    const TCHAR* KindToString(EHavocConvoyVehicleKind Kind)
    {
        return Kind == EHavocConvoyVehicleKind::Warden ? TEXT("WARDEN") : TEXT("NOMAD");
    }

    const TCHAR* CargoToString(EHavocConvoyCargo Cargo)
    {
        switch (Cargo)
        {
        case EHavocConvoyCargo::FalconCrew: return TEXT("FALCON_CREW");
        case EHavocConvoyCargo::Supplies: return TEXT("SUPPLIES");
        default: return TEXT("NONE");
        }
    }

    EHavocConvoyCargo CargoFromString(const FString& Text)
    {
        if (Text == TEXT("FALCON_CREW")) { return EHavocConvoyCargo::FalconCrew; }
        if (Text == TEXT("SUPPLIES")) { return EHavocConvoyCargo::Supplies; }
        return EHavocConvoyCargo::None;
    }

    FHavocConvoyVehicle MakeVehicle(const TCHAR* Id, EHavocConvoyVehicleKind Kind, EHavocConvoyCargo Cargo)
    {
        FHavocConvoyVehicle V;
        V.Id = Id;
        V.Kind = Kind;
        V.Cargo = Cargo;
        return V;
    }
}

void UHavocConvoyEscort::Form(bool bWithCrew)
{
    State = EHavocConvoyState::Forming;
    Route = EHavocConvoyRoute::MainRoad;
    Vehicles.Reset();
    Vehicles.Add(MakeVehicle(TEXT("warden-1"), EHavocConvoyVehicleKind::Warden, EHavocConvoyCargo::None));
    Vehicles.Add(MakeVehicle(TEXT("nomad-1"), EHavocConvoyVehicleKind::Nomad, bWithCrew ? EHavocConvoyCargo::FalconCrew : EHavocConvoyCargo::Supplies));
    Vehicles.Add(MakeVehicle(TEXT("nomad-2"), EHavocConvoyVehicleKind::Nomad, EHavocConvoyCargo::Supplies));
}

bool UHavocConvoyEscort::CanUseRoute(EHavocConvoyVehicleKind Kind, EHavocConvoyRoute InRoute)
{
    return InRoute != EHavocConvoyRoute::RiverDrainage;
}

TArray<FHavocConvoyVehicle> UHavocConvoyEscort::GetAlive() const
{
    return Vehicles.FilterByPredicate([](const FHavocConvoyVehicle& V) { return V.Integrity > 0.0; });
}

bool UHavocConvoyEscort::Depart(EHavocConvoyRoute InRoute)
{
    const TArray<FHavocConvoyVehicle> Alive = GetAlive();
    if (State == EHavocConvoyState::Arrived || Alive.Num() == 0)
    {
        return false;
    }
    for (const FHavocConvoyVehicle& V : Alive)
    {
        if (!CanUseRoute(V.Kind, InRoute))
        {
            return false;
        }
    }
    Route = InRoute;
    State = EHavocConvoyState::Moving;
    return true;
}

void UHavocConvoyEscort::Halt()
{
    if (State != EHavocConvoyState::Arrived)
    {
        State = EHavocConvoyState::Halted;
    }
}

void UHavocConvoyEscort::Ambush()
{
    if (State == EHavocConvoyState::Moving || State == EHavocConvoyState::Halted)
    {
        State = EHavocConvoyState::Ambushed;
    }
}

bool UHavocConvoyEscort::Resume()
{
    if (State == EHavocConvoyState::Arrived || GetAlive().Num() == 0)
    {
        return false;
    }
    State = EHavocConvoyState::Moving;
    return true;
}

void UHavocConvoyEscort::Arrive()
{
    if (GetAlive().Num() > 0)
    {
        State = EHavocConvoyState::Arrived;
    }
}

bool UHavocConvoyEscort::ApplyDamage(const FString& VehicleId, double Amount)
{
    FHavocConvoyVehicle* V = Vehicles.FindByPredicate([&VehicleId](const FHavocConvoyVehicle& X) { return X.Id == VehicleId; });
    if (!V)
    {
        return false;
    }
    if (V->Integrity > 0.0)
    {
        V->Integrity = FMath::Max(0.0, HavocE04::Round3(V->Integrity - FMath::Max(0.0, Amount)));
    }
    return true;
}

bool UHavocConvoyEscort::Repair(const FString& VehicleId, double Amount)
{
    FHavocConvoyVehicle* V = Vehicles.FindByPredicate([&VehicleId](const FHavocConvoyVehicle& X) { return X.Id == VehicleId; });
    if (!V || V->Integrity <= 0.0)
    {
        return false;
    }
    V->Integrity = FMath::Min(1.0, V->Integrity + FMath::Max(0.0, Amount));
    return true;
}

bool UHavocConvoyEscort::CarriesCrew() const
{
    return Vehicles.ContainsByPredicate([](const FHavocConvoyVehicle& V) { return V.Cargo == EHavocConvoyCargo::FalconCrew; });
}

bool UHavocConvoyEscort::IsCrewSafe() const
{
    return Vehicles.ContainsByPredicate([](const FHavocConvoyVehicle& V) { return V.Cargo == EHavocConvoyCargo::FalconCrew && V.Integrity > 0.0; });
}

bool UHavocConvoyEscort::IsMissionCritical() const
{
    return CarriesCrew() ? !IsCrewSafe() : GetNomadsRemaining() == 0;
}

int32 UHavocConvoyEscort::GetNomadsRemaining() const
{
    int32 N = 0;
    for (const FHavocConvoyVehicle& V : Vehicles)
    {
        N += (V.Kind == EHavocConvoyVehicleKind::Nomad && V.Integrity > 0.0) ? 1 : 0;
    }
    return N;
}

bool UHavocConvoyEscort::IsWardenOperational() const
{
    return Vehicles.ContainsByPredicate([](const FHavocConvoyVehicle& V) { return V.Kind == EHavocConvoyVehicleKind::Warden && V.Integrity > 0.0; });
}

bool UHavocConvoyEscort::IsFullStrength() const
{
    return Vehicles.Num() > 0 && GetAlive().Num() == Vehicles.Num();
}

TSharedRef<FJsonObject> UHavocConvoyEscort::Snapshot() const
{
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const FHavocConvoyVehicle& V : Vehicles)
    {
        const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("id"), V.Id);
        Item->SetStringField(TEXT("kind"), KindToString(V.Kind));
        Item->SetStringField(TEXT("cargo"), CargoToString(V.Cargo));
        Item->SetNumberField(TEXT("integrity"), V.Integrity);
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("state"), StateToString(State));
    Json->SetStringField(TEXT("route"), RouteToString(Route));
    Json->SetArrayField(TEXT("vehicles"), Items);
    return Json;
}

void UHavocConvoyEscort::Restore(const TSharedPtr<FJsonObject>& Data)
{
    if (!Data.IsValid())
    {
        return;
    }
    FString Text;
    if (Data->TryGetStringField(TEXT("state"), Text)) { State = StateFromString(Text); }
    if (Data->TryGetStringField(TEXT("route"), Text)) { Route = RouteFromString(Text); }
    Vehicles.Reset();
    for (const TSharedPtr<FJsonObject>& Item : HavocMissionJson::ReadObjectArray(*Data, TEXT("vehicles")))
    {
        FHavocConvoyVehicle V;
        if (Item->TryGetStringField(TEXT("id"), Text)) { V.Id = Text; }
        if (Item->TryGetStringField(TEXT("kind"), Text)) { V.Kind = Text == TEXT("WARDEN") ? EHavocConvoyVehicleKind::Warden : EHavocConvoyVehicleKind::Nomad; }
        if (Item->TryGetStringField(TEXT("cargo"), Text)) { V.Cargo = CargoFromString(Text); }
        Item->TryGetNumberField(TEXT("integrity"), V.Integrity);
        Vehicles.Add(V);
    }
}
