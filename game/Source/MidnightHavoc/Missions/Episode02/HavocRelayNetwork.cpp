#include "Missions/Episode02/HavocRelayNetwork.h"

#include "Drones/HavocDroneModel.h"
#include "Missions/HavocMissionJson.h"

namespace
{
    const TCHAR* StateToString(EHavocRelayState State)
    {
        switch (State)
        {
        case EHavocRelayState::Online: return TEXT("ONLINE");
        case EHavocRelayState::Jammed: return TEXT("JAMMED");
        default: return TEXT("STOWED");
        }
    }

    EHavocRelayState RelayStateFromString(const FString& Text)
    {
        if (Text == TEXT("ONLINE")) { return EHavocRelayState::Online; }
        if (Text == TEXT("JAMMED")) { return EHavocRelayState::Jammed; }
        return EHavocRelayState::Stowed;
    }
}

UHavocRelayNetwork::UHavocRelayNetwork()
{
    Base.Id = TEXT("echo-base");
    Base.Position = FVector::ZeroVector;
    Base.Range = 1600.0;
    Base.State = EHavocRelayState::Online;
}

FHavocFieldRelay UHavocRelayNetwork::Place(const FString& Id, const FVector& Position, double Range)
{
    FHavocFieldRelay* Existing = Relays.FindByPredicate([&Id](const FHavocFieldRelay& R) { return R.Id == Id; });
    if (Existing)
    {
        Existing->Position = Position;
        Existing->State = EHavocRelayState::Online;
        return *Existing;
    }
    FHavocFieldRelay Relay;
    Relay.Id = Id;
    Relay.Position = Position;
    Relay.Range = Range;
    Relay.State = EHavocRelayState::Online;
    Relays.Add(Relay);
    return Relay;
}

void UHavocRelayNetwork::JamAll(double InInterference)
{
    Interference = InInterference;
    for (FHavocFieldRelay& R : Relays)
    {
        if (R.State == EHavocRelayState::Online)
        {
            R.State = EHavocRelayState::Jammed;
        }
    }
}

bool UHavocRelayNetwork::Restore(const FString& Id, const FVector& SquadPosition)
{
    FHavocFieldRelay* R = Relays.FindByPredicate([&Id](const FHavocFieldRelay& X) { return X.Id == Id; });
    if (!R || R->State != EHavocRelayState::Jammed || FVector::Dist(R->Position, SquadPosition) > RestoreDistance)
    {
        return false;
    }
    R->State = EHavocRelayState::Online;
    if (!Relays.ContainsByPredicate([](const FHavocFieldRelay& X) { return X.State == EHavocRelayState::Jammed; }))
    {
        Interference = 0.0;
    }
    return true;
}

TArray<FHavocFieldRelay> UHavocRelayNetwork::GetOnlineSources() const
{
    TArray<FHavocFieldRelay> Sources;
    if (Base.State == EHavocRelayState::Online)
    {
        Sources.Add(Base);
    }
    for (const FHavocFieldRelay& R : Relays)
    {
        if (R.State == EHavocRelayState::Online)
        {
            Sources.Add(R);
        }
    }
    return Sources;
}

double UHavocRelayNetwork::SignalAt(const FVector& Position) const
{
    double Best = 0.0;
    for (const FHavocFieldRelay& R : GetOnlineSources())
    {
        Best = FMath::Max(Best, FHavocDroneSignalManager::Calculate(Position, R.Position, R.Range, Interference));
    }
    return Best;
}

EHavocSignalState UHavocRelayNetwork::Classify(const FVector& Position) const
{
    return FHavocDroneSignalManager::Classify(SignalAt(Position));
}

int32 UHavocRelayNetwork::GetOnline() const
{
    int32 N = 0;
    for (const FHavocFieldRelay& R : Relays)
    {
        N += R.State == EHavocRelayState::Online ? 1 : 0;
    }
    return N;
}

bool UHavocRelayNetwork::FindRelay(const FString& Id, FHavocFieldRelay& OutRelay) const
{
    const FHavocFieldRelay* R = Relays.FindByPredicate([&Id](const FHavocFieldRelay& X) { return X.Id == Id; });
    if (!R)
    {
        return false;
    }
    OutRelay = *R;
    return true;
}

TSharedRef<FJsonObject> UHavocRelayNetwork::Snapshot() const
{
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const FHavocFieldRelay& R : Relays)
    {
        const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("id"), R.Id);
        const TSharedRef<FJsonObject> Pos = MakeShared<FJsonObject>();
        Pos->SetNumberField(TEXT("x"), R.Position.X);
        Pos->SetNumberField(TEXT("y"), R.Position.Y);
        Pos->SetNumberField(TEXT("z"), R.Position.Z);
        Item->SetObjectField(TEXT("position"), Pos);
        Item->SetNumberField(TEXT("range"), R.Range);
        Item->SetStringField(TEXT("state"), StateToString(R.State));
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("interference"), Interference);
    Json->SetArrayField(TEXT("relays"), Items);
    return Json;
}

void UHavocRelayNetwork::RestoreSnapshot(const TSharedPtr<FJsonObject>& Data)
{
    if (!Data.IsValid())
    {
        return;
    }
    Data->TryGetNumberField(TEXT("interference"), Interference);
    Relays.Reset();
    for (const TSharedPtr<FJsonObject>& Item : HavocMissionJson::ReadObjectArray(*Data, TEXT("relays")))
    {
        FHavocFieldRelay R;
        FString Text;
        if (Item->TryGetStringField(TEXT("id"), Text)) { R.Id = Text; }
        const TSharedPtr<FJsonObject>* Pos = nullptr;
        if (Item->TryGetObjectField(TEXT("position"), Pos) && Pos)
        {
            (*Pos)->TryGetNumberField(TEXT("x"), R.Position.X);
            (*Pos)->TryGetNumberField(TEXT("y"), R.Position.Y);
            (*Pos)->TryGetNumberField(TEXT("z"), R.Position.Z);
        }
        Item->TryGetNumberField(TEXT("range"), R.Range);
        if (Item->TryGetStringField(TEXT("state"), Text)) { R.State = RelayStateFromString(Text); }
        Relays.Add(R);
    }
}
