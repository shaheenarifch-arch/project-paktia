#pragma once

// A facility of the Kharg oil terminal: east_jetty, sea_island_jetty, tank_farm, pump_station.
// Damage is forwarded to the Episode 04 mission (UHavocOpenStraitMission::DamageTerminal),
// which owns the integrity numbers. Abstract game rules only.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HavocTerminalPart.generated.h"

UCLASS(Blueprintable)
class MIDNIGHTHAVOC_API AHavocTerminalPart : public AActor
{
    GENERATED_BODY()

public:
    AHavocTerminalPart();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Terminal",
        meta = (ToolTip = "Facility id: east_jetty, sea_island_jetty, tank_farm or pump_station."))
    FName PartId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Terminal")
    TObjectPtr<USceneComponent> Root;

    /** Applies damage to this facility in the mission. Returns false if there is no Episode 04 mission or PartId is unknown. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Terminal",
        meta = (ToolTip = "Forward damage to the Episode 04 oil terminal. Returns false if no mission is running or PartId is unknown."))
    bool ApplyTerminalDamage(float Amount);

    /** Fired after damage was applied to the mission. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|Terminal",
        meta = (ToolTip = "Use for smoke, fire or damaged-mesh swaps."))
    void OnTerminalDamaged(float Amount);

    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
};
