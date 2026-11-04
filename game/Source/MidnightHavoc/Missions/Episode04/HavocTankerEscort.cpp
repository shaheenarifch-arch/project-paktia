#include "Missions/Episode04/HavocTankerEscort.h"

#include "Missions/HavocMissionJson.h"
#include "Missions/Episode04/HavocE04Math.h"

namespace
{
    const TCHAR* StateToString(EHavocTankerState State)
    {
        switch (State)
        {
        case EHavocTankerState::Transiting: return TEXT("TRANSITING");
        case EHavocTankerState::Through: return TEXT("THROUGH");
        case EHavocTankerState::Lost: return TEXT("LOST");
        default: return TEXT("WAITING");
        }
    }

    EHavocTankerState StateFromString(const FString& Text)
    {
        if (Text == TEXT("TRANSITING")) { return EHavocTankerState::Transiting; }
        if (Text == TEXT("THROUGH")) { return EHavocTankerState::Through; }
        if (Text == TEXT("LOST")) { return EHavocTankerState::Lost; }
        return EHavocTankerState::Waiting;
    }
}

FHavocTanker* UHavocTankerEscort::FindTanker(const FString& TankerId)
{
    return Tankers.FindByPredicate([&TankerId](const FHavocTanker& T) { return T.Id == TankerId; });
}

void UHavocTankerEscort::Assemble(int32 Count)
{
    Tankers.Reset();
    for (int32 i = 0; i < Count; ++i)
    {
        FHavocTanker Tanker;
        Tanker.Id = FString::Printf(TEXT("tanker-%d"), i + 1);
        Tankers.Add(Tanker);
    }
}

bool UHavocTankerEscort::Begin(const FString& TankerId, FName LaneId, bool bLaneOpen)
{
    FHavocTanker* Tanker = FindTanker(TankerId);
    if (!Tanker || Tanker->State != EHavocTankerState::Waiting || !bLaneOpen)
    {
        return false;
    }
    Tanker->State = EHavocTankerState::Transiting;
    Tanker->Lane = LaneId;
    return true;
}

bool UHavocTankerEscort::Arrive(const FString& TankerId)
{
    FHavocTanker* Tanker = FindTanker(TankerId);
    if (!Tanker || Tanker->State != EHavocTankerState::Transiting)
    {
        return false;
    }
    Tanker->State = EHavocTankerState::Through;
    return true;
}

void UHavocTankerEscort::Damage(const FString& TankerId, double Amount)
{
    FHavocTanker* Tanker = FindTanker(TankerId);
    if (!Tanker || Tanker->State == EHavocTankerState::Through || Tanker->State == EHavocTankerState::Lost)
    {
        return;
    }
    Tanker->Integrity = FMath::Max(0.0, HavocE04::Round3(Tanker->Integrity - FMath::Max(0.0, Amount)));
    if (Tanker->Integrity == 0.0)
    {
        Tanker->State = EHavocTankerState::Lost;
    }
}

int32 UHavocTankerEscort::GetThrough() const
{
    int32 N = 0;
    for (const FHavocTanker& T : Tankers) { N += T.State == EHavocTankerState::Through ? 1 : 0; }
    return N;
}

int32 UHavocTankerEscort::GetLost() const
{
    int32 N = 0;
    for (const FHavocTanker& T : Tankers) { N += T.State == EHavocTankerState::Lost ? 1 : 0; }
    return N;
}

bool UHavocTankerEscort::IsSettled() const
{
    if (Tankers.Num() == 0)
    {
        return false;
    }
    for (const FHavocTanker& T : Tankers)
    {
        if (T.State != EHavocTankerState::Through && T.State != EHavocTankerState::Lost)
        {
            return false;
        }
    }
    return true;
}

double UHavocTankerEscort::GetSupplyFlow() const
{
    return Tankers.Num() > 0 ? HavocE04::Round3(static_cast<double>(GetThrough()) / Tankers.Num()) : 0.0;
}

TSharedRef<FJsonObject> UHavocTankerEscort::Snapshot() const
{
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const FHavocTanker& T : Tankers)
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

void UHavocTankerEscort::Restore(const TSharedPtr<FJsonObject>& Data)
{
    if (!Data.IsValid())
    {
        return;
    }
    Tankers.Reset();
    for (const TSharedPtr<FJsonObject>& Item : HavocMissionJson::ReadObjectArray(*Data, TEXT("tankers")))
    {
        FHavocTanker T;
        FString Text;
        if (Item->TryGetStringField(TEXT("id"), Text)) { T.Id = Text; }
        if (Item->TryGetStringField(TEXT("state"), Text)) { T.State = StateFromString(Text); }
        Item->TryGetNumberField(TEXT("integrity"), T.Integrity);
        if (Item->TryGetStringField(TEXT("lane"), Text)) { T.Lane = FName(*Text); }
        Tankers.Add(T);
    }
}
