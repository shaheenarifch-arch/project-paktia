#include "Missions/Episode04/PaktiaOilTerminal.h"

#include "Missions/PaktiaMissionJson.h"
#include "Missions/Episode04/PaktiaE04Math.h"

UPaktiaOilTerminal::UPaktiaOilTerminal()
{
    auto Add = [this](const TCHAR* Id, const TCHAR* Name)
    {
        FPaktiaTerminalFacility Facility;
        Facility.Id = FName(Id);
        Facility.Name = Name;
        Facilities.Add(Facility);
    };
    Add(TEXT("east_jetty"), TEXT("East jetty"));
    Add(TEXT("sea_island_jetty"), TEXT("Sea Island jetty"));
    Add(TEXT("tank_farm"), TEXT("Tank farm"));
    Add(TEXT("pump_station"), TEXT("Pump station"));
}

bool UPaktiaOilTerminal::Damage(FName FacilityId, double Amount)
{
    for (FPaktiaTerminalFacility& Facility : Facilities)
    {
        if (Facility.Id == FacilityId)
        {
            Facility.Integrity = FMath::Max(0.0, PaktiaE04::Round3(Facility.Integrity - FMath::Max(0.0, Amount)));
            return true;
        }
    }
    return false;
}

double UPaktiaOilTerminal::GetIntegrity() const
{
    double Sum = 0.0;
    for (const FPaktiaTerminalFacility& Facility : Facilities)
    {
        Sum += Facility.Integrity;
    }
    return Facilities.Num() > 0 ? PaktiaE04::Round3(Sum / Facilities.Num()) : 0.0;
}

bool UPaktiaOilTerminal::CanExport() const
{
    bool bJetty = false;
    double Pump = 0.0;
    for (const FPaktiaTerminalFacility& Facility : Facilities)
    {
        if (Facility.Id.ToString().EndsWith(TEXT("jetty")) && Facility.Integrity > 0.3)
        {
            bJetty = true;
        }
        if (Facility.Id == FName(TEXT("pump_station")))
        {
            Pump = Facility.Integrity;
        }
    }
    return bJetty && Pump > 0.3;
}

TSharedRef<FJsonObject> UPaktiaOilTerminal::Snapshot() const
{
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const FPaktiaTerminalFacility& Facility : Facilities)
    {
        const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("id"), Facility.Id.ToString());
        Item->SetStringField(TEXT("name"), Facility.Name);
        Item->SetNumberField(TEXT("integrity"), Facility.Integrity);
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetArrayField(TEXT("facilities"), Items);
    return Json;
}

void UPaktiaOilTerminal::Restore(const TSharedPtr<FJsonObject>& Data)
{
    if (!Data.IsValid())
    {
        return;
    }
    Facilities.Reset();
    for (const TSharedPtr<FJsonObject>& Item : PaktiaMissionJson::ReadObjectArray(*Data, TEXT("facilities")))
    {
        FPaktiaTerminalFacility Facility;
        FString Text;
        if (Item->TryGetStringField(TEXT("id"), Text)) { Facility.Id = FName(*Text); }
        if (Item->TryGetStringField(TEXT("name"), Text)) { Facility.Name = Text; }
        Item->TryGetNumberField(TEXT("integrity"), Facility.Integrity);
        Facilities.Add(Facility);
    }
}
