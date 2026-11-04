#pragma once

// A sector of a map (data/maps/*.json "sectors[].id") placed in the level.
// Publishes "sector:enter" / "sector:exit" through UHavocEventSubsystem when a Pawn crosses it.
// Payload is a JSON string: {"sectorId":"...","actor":"...","isPlayer":true|false}.

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "HavocSectorVolume.generated.h"

UCLASS(Blueprintable)
class MIDNIGHTHAVOC_API AHavocSectorVolume : public ATriggerBox
{
    GENERATED_BODY()

public:
    AHavocSectorVolume();

    /** Must match a sector id in the map JSON exactly, e.g. kharg_north_beach. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Sector",
        meta = (ToolTip = "Sector id from the map JSON (data/maps/kharg-hormuz.json). Must match exactly."))
    FName SectorId;

    /** Returns the Pawns currently inside the sector. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Sector",
        meta = (ToolTip = "Pawns (player or unit) currently inside this sector."))
    TArray<APawn*> GetOccupants() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void HandleBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

    UFUNCTION()
    void HandleEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

private:
    void PublishSectorEvent(const TCHAR* EventName, AActor* OtherActor) const;
};
