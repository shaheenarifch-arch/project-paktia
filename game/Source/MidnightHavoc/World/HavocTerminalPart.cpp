#include "World/HavocTerminalPart.h"
#include "World/HavocWorldLog.h"
#include "Missions/HavocMissionSubsystem.h"
#include "Missions/Episode04/HavocOpenStraitMission.h"
#include "Engine/World.h"

AHavocTerminalPart::AHavocTerminalPart()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
}

bool AHavocTerminalPart::ApplyTerminalDamage(float Amount)
{
    UWorld* World = GetWorld();
    UHavocMissionSubsystem* Missions = World ? World->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    UHavocOpenStraitMission* Mission = Missions ? Missions->GetOpenStrait() : nullptr;
    if (!Mission)
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Terminal part '%s': no Episode 04 mission active, damage ignored"), *PartId.ToString());
        return false;
    }
    if (!Mission->DamageTerminal(PartId, static_cast<double>(Amount)))
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Terminal part '%s': unknown facility id"), *PartId.ToString());
        return false;
    }
    OnTerminalDamaged(Amount);
    return true;
}

float AHavocTerminalPart::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (Actual > 0.f)
    {
        ApplyTerminalDamage(Actual);
    }
    return Actual;
}
