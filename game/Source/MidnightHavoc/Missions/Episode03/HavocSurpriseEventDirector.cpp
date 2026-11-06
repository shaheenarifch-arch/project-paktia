#include "Missions/Episode03/HavocSurpriseEventDirector.h"

bool FHavocSurpriseEventDirector::IsKnownEvent(const FString& Event)
{
    return Event == TEXT("sandstorm") || Event == TEXT("communications_outage") || Event == TEXT("tunnel_discovery")
        || Event == TEXT("bridge_obstruction") || Event == TEXT("nightfall");
}

bool FHavocSurpriseEventDirector::Pick(const TArray<FString>& Events, const FString& Seed, FString& OutEvent, FString& OutError)
{
    if (Events.Num() == 0)
    {
        OutError = TEXT("No surprise events defined for this map");
        return false;
    }
    // FNV-1a, 32 bit, same constants as the TS (Math.imul wraps to 32 bits).
    uint32 Hash = 2166136261u;
    for (int32 i = 0; i < Seed.Len(); ++i)
    {
        Hash = (Hash ^ static_cast<uint32>(Seed[i])) * 16777619u;
    }
    const FString& Pick = Events[Hash % static_cast<uint32>(Events.Num())];
    if (!IsKnownEvent(Pick))
    {
        OutError = FString::Printf(TEXT("Unknown surprise event %s"), *Pick);
        return false;
    }
    OutEvent = Pick;
    return true;
}

FHavocSurpriseModifiers FHavocSurpriseEventDirector::ModifiersFor(const FString& Event)
{
    FHavocSurpriseModifiers M;
    if (Event == TEXT("sandstorm")) { M.DroneSensorRangeScale = 0.5; M.VisibilityScale = 0.6; }
    else if (Event == TEXT("communications_outage")) { M.RelayInterference = 0.35; }
    else if (Event == TEXT("tunnel_discovery")) { M.OpenedRoutes.Add(TEXT("old_gardun_cellars")); }
    else if (Event == TEXT("bridge_obstruction")) { M.BlockedRoutes.Add(TEXT("riverbed_bridge")); }
    else if (Event == TEXT("nightfall")) { M.VisibilityScale = 0.4; }
    return M;
}

FString FHavocSurpriseEventDirector::Trigger()
{
    if (bFired)
    {
        return FString();
    }
    bFired = true;
    Active = Planned;
    return Active;
}

void FHavocSurpriseEventDirector::Restore(bool bInFired, const FString& InActive)
{
    bFired = bInFired;
    Active = IsKnownEvent(InActive) ? InActive : FString();
}
