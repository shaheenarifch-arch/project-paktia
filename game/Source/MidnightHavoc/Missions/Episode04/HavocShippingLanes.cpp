#include "Missions/Episode04/HavocShippingLanes.h"

#include "Missions/HavocMissionJson.h"

void UHavocShippingLanes::Seed(int32 InboundHazards, int32 OutboundHazards)
{
    Lanes.Reset();
    FHavocLane Inbound;
    Inbound.Id = FName(TEXT("inbound"));
    Inbound.Hazards = InboundHazards;
    Lanes.Add(Inbound);
    FHavocLane Outbound;
    Outbound.Id = FName(TEXT("outbound"));
    Outbound.Hazards = OutboundHazards;
    Lanes.Add(Outbound);
}

FHavocLane* UHavocShippingLanes::FindLane(FName LaneId)
{
    return Lanes.FindByPredicate([LaneId](const FHavocLane& L) { return L.Id == LaneId; });
}

const FHavocLane* UHavocShippingLanes::FindLane(FName LaneId) const
{
    return Lanes.FindByPredicate([LaneId](const FHavocLane& L) { return L.Id == LaneId; });
}

int32 UHavocShippingLanes::Reveal(FName LaneId, int32 Count)
{
    FHavocLane* Lane = FindLane(LaneId);
    if (!Lane)
    {
        return 0;
    }
    const int32 N = FMath::Min(FMath::Max(0, Count), Lane->Hazards - Lane->Revealed);
    Lane->Revealed += N;
    return N;
}

bool UHavocShippingLanes::Clear(FName LaneId)
{
    FHavocLane* Lane = FindLane(LaneId);
    if (!Lane || Lane->Cleared >= Lane->Revealed)
    {
        return false;
    }
    Lane->Cleared += 1;
    return true;
}

bool UHavocShippingLanes::IsOpen(FName LaneId) const
{
    const FHavocLane* Lane = FindLane(LaneId);
    return Lane && Lane->Cleared == Lane->Hazards;
}

bool UHavocShippingLanes::AllOpen() const
{
    if (Lanes.Num() == 0)
    {
        return false;
    }
    for (const FHavocLane& Lane : Lanes)
    {
        if (Lane.Cleared != Lane.Hazards)
        {
            return false;
        }
    }
    return true;
}

TSharedRef<FJsonObject> UHavocShippingLanes::Snapshot() const
{
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const FHavocLane& Lane : Lanes)
    {
        const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("id"), Lane.Id.ToString());
        Item->SetNumberField(TEXT("hazards"), Lane.Hazards);
        Item->SetNumberField(TEXT("revealed"), Lane.Revealed);
        Item->SetNumberField(TEXT("cleared"), Lane.Cleared);
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetArrayField(TEXT("lanes"), Items);
    return Json;
}

void UHavocShippingLanes::Restore(const TSharedPtr<FJsonObject>& Data)
{
    if (!Data.IsValid())
    {
        return;
    }
    Lanes.Reset();
    for (const TSharedPtr<FJsonObject>& Item : HavocMissionJson::ReadObjectArray(*Data, TEXT("lanes")))
    {
        FHavocLane Lane;
        FString Text;
        if (Item->TryGetStringField(TEXT("id"), Text)) { Lane.Id = FName(*Text); }
        Item->TryGetNumberField(TEXT("hazards"), Lane.Hazards);
        Item->TryGetNumberField(TEXT("revealed"), Lane.Revealed);
        Item->TryGetNumberField(TEXT("cleared"), Lane.Cleared);
        Lanes.Add(Lane);
    }
}
