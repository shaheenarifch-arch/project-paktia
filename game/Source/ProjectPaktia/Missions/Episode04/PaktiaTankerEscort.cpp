#include "Missions/Episode04/PaktiaTankerEscort.h"

#include "Missions/PaktiaMissionJson.h"
#include "Missions/Episode04/PaktiaE04Math.h"

namespace
{
    const TCHAR* StateToString(EPaktiaTankerState State)
    {
        switch (State)
        {
        case EPaktiaTankerState::Transiting: return TEXT("TRANSITING");
        case EPaktiaTankerState::Through: return TEXT("THROUGH");
        case EPaktiaTankerState::Lost: return TEXT("LOST");
        default: return TEXT("WAITING");
        }
    }

    EPaktiaTankerState StateFromString(const FString& Text)
    {
        if (Text == TEXT("TRANSITING")) { return EPaktiaTankerState::Transiting; }
        if (Text == TEXT("THROUGH")) { return EPaktiaTankerState::Through; }
        if (Text == TEXT("LOST")) { return EPaktiaTankerState::Lost; }
        return EPaktiaTankerState::Waiting;
    }
}

FPaktiaTanker* UPaktiaTankerEscort::FindTanker(const FString& TankerId)
{
    return Tankers.FindByPredicate([&TankerId](const FPaktiaTanker& T) { return T.Id == TankerId; });
}

void UPaktiaTankerEscort::Assemble(int32 Count)
{
    Tankers.Reset();
    for (int32 i = 0; i < Count; ++i)
    {
        FPaktiaTanker Tanker;
        Tanker.Id = FString::Printf(TEXT("tanker-%d"), i + 1);
        Tankers.Add(Tanker);
    }
}

bool UPaktiaTankerEscort::Begin(const FString& TankerId, FName LaneId, bool bLaneOpen)
{
    FPaktiaTanker* Tanker = FindTanker(TankerId);
    if (!Tanker || Tanker->State != EPaktiaTankerState::Waiting || !bLaneOpen)
    {
        return false;
    }
    Tanker->State = EPaktiaTankerState::Transiting;
    Tanker->Lane = LaneId;
    return true;
}

bool UPaktiaTankerEscort::Arrive(const FString& TankerId)
{
    FPaktiaTanker* Tanker = FindTanker(TankerId);
    if (!Tanker || Tanker->State != EPaktiaTankerState::Transiting)
    {
        return false;
    }
    Tanker->State = EPaktiaTankerState::Through;
    return true;
}

void UPaktiaTankerEscort::Damage(const FString& TankerId, double Amount)
{
    FPaktiaTanker* Tanker = FindTanker(TankerId);
    if (!Tanker || Tanker->State == EPaktiaTankerState::Through || Tanker->State == EPaktiaTankerState::Lost)
    {
        return;
    }
    Tanker->Integrity = FMath::Max(0.0, PaktiaE04::Round3(Tanker->Integrity - FMath::Max(0.0, Amount)));
    if (Tanker->Integrity == 0.0)
    {
        Tanker->State = EPaktiaTankerState::Lost;
    }
}

int32 UPaktiaTankerEscort::GetThrough() const
{
    int32 N = 0;
    for (const FPaktiaTanker& T : Tankers) { N += T.State == EPaktiaTankerState::Through ? 1 : 0; }
    return N;
}

int32 UPaktiaTankerEscort::GetLost() const
{
    int32 N = 0;
    for (const FPaktiaTanker& T : Tankers) { N += T.State == EPaktiaTankerState::Lost ? 1 : 0; }
    return N;
}

bool UPaktiaTankerEscort::IsSettled() const
{
    if (Tankers.Num() == 0)
    {
        return false;
    }
    for (const FPaktiaTanker& T : Tankers)
    {
        if (T.State != EPaktiaTankerState::Through && T.State != EPaktiaTankerState::Lost)
        {
            return false;
        }
    }
    return true;
}

double UPaktiaTankerEscort::GetSupplyFlow() const
{
    return Tankers.Num() > 0 ? PaktiaE04::Round3(static_cast<double>(GetThrough()) / Tankers.Num()) : 0.0;
}

TSharedRef<FJsonObject> UPaktiaTankerEscort::Snapshot() const
{
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const FPaktiaTanker& T : Tankers)
    {
        const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("id"), T.Id);
        Item->SetStringField(TEXT("state"), StateToString(T.State));
        Item->SetNumberField(TEXT("integrity"), T.Integrity);
        if (!T.Lane.IsNone())
        {
            Item->SetStringField(TEXT("lane"), T.Lane.ToString());
        }
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetArrayField(TEXT("tankers"), Items);
    return Json;
}

void UPaktiaTankerEscort::Restore(const TSharedPtr<FJsonObject>& Data)
{
    if (!Data.IsValid())
    {
        return;
    }
    Tankers.Reset();
    for (const TSharedPtr<FJsonObject>& Item : PaktiaMissionJson::ReadObjectArray(*Data, TEXT("tankers")))
    {
        FPaktiaTanker T;
        FString Text;
        if (Item->TryGetStringField(TEXT("id"), Text)) { T.Id = Text; }
        if (Item->TryGetStringField(TEXT("state"), Text)) { T.State = StateFromString(Text); }
        Item->TryGetNumberField(TEXT("integrity"), T.Integrity);
        if (Item->TryGetStringField(TEXT("lane"), Text)) { T.Lane = FName(*Text); }
        Tankers.Add(T);
    }
}
