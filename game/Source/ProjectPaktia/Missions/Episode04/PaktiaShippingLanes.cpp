#include "Missions/Episode04/PaktiaShippingLanes.h"

#include "Missions/PaktiaMissionJson.h"

void UPaktiaShippingLanes::Seed(int32 InboundHazards, int32 OutboundHazards)
{
    Lanes.Reset();
    FPaktiaLane Inbound;
    Inbound.Id = FName(TEXT("inbound"));
    Inbound.Hazards = InboundHazards;
    Lanes.Add(Inbound);
    FPaktiaLane Outbound;
    Outbound.Id = FName(TEXT("outbound"));
    Outbound.Hazards = OutboundHazards;
    Lanes.Add(Outbound);
}

FPaktiaLane* UPaktiaShippingLanes::FindLane(FName LaneId)
{
    return Lanes.FindByPredicate([LaneId](const FPaktiaLane& L) { return L.Id == LaneId; });
}

const FPaktiaLane* UPaktiaShippingLanes::FindLane(FName LaneId) const
{
    return Lanes.FindByPredicate([LaneId](const FPaktiaLane& L) { return L.Id == LaneId; });
}

int32 UPaktiaShippingLanes::Reveal(FName LaneId, int32 Count)
{
    FPaktiaLane* Lane = FindLane(LaneId);
    if (!Lane)
    {
        return 0;
    }
    const int32 N = FMath::Min(FMath::Max(0, Count), Lane->Hazards - Lane->Revealed);
    Lane->Revealed += N;
    return N;
}

bool UPaktiaShippingLanes::Clear(FName LaneId)
{
    FPaktiaLane* Lane = FindLane(LaneId);
    if (!Lane || Lane->Cleared >= Lane->Revealed)
    {
        return false;
    }
    Lane->Cleared += 1;
    return true;
}

bool UPaktiaShippingLanes::IsOpen(FName LaneId) const
{
    const FPaktiaLane* Lane = FindLane(LaneId);
    return Lane && Lane->Cleared == Lane->Hazards;
}

bool UPaktiaShippingLanes::AllOpen() const
{
    if (Lanes.Num() == 0)
    {
        return false;
    }
    for (const FPaktiaLane& Lane : Lanes)
    {
        if (Lane.Cleared != Lane.Hazards)
        {
            return false;
        }
    }
    return true;
}

TSharedRef<FJsonObject> UPaktiaShippingLanes::Snapshot() const
{
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const FPaktiaLane& Lane : Lanes)
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

void UPaktiaShippingLanes::Restore(const TSharedPtr<FJsonObject>& Data)
{
    if (!Data.IsValid())
    {
        return;
    }
    Lanes.Reset();
    for (const TSharedPtr<FJsonObject>& Item : PaktiaMissionJson::ReadObjectArray(*Data, TEXT("lanes")))
    {
        FPaktiaLane Lane;
        FString Text;
        if (Item->TryGetStringField(TEXT("id"), Text)) { Lane.Id = FName(*Text); }
        Item->TryGetNumberField(TEXT("hazards"), Lane.Hazards);
        Item->TryGetNumberField(TEXT("revealed"), Lane.Revealed);
        Item->TryGetNumberField(TEXT("cleared"), Lane.Cleared);
        Lanes.Add(Lane);
    }
}
