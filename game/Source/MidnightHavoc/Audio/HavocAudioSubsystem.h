#pragma once

// Plays sounds and Niagara effects by game role. Roles come from Config/HavocAssets.ini through
// UHavocAssetSubsystem (item 3), so swapping a Fab pack is config-only. A role that is unmapped or
// not imported is logged once and skipped; there are no generated placeholders (docs/audio-voice-plan.md).
//
// Event hooks (docs/audio-voice-plan.md section 2.5, docs/vfx-plan.md "Event hooks"):
//   hazard:cleared, tanker:damaged, falcon:support  -> payload {"x":..,"y":..,"z":..} in Unreal units.
// The coast ambience loop starts with StartAmbient.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/sound-cue-reference-for-unreal-engine
//       https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-visual-effects-in-unreal-engine

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HavocAudioSubsystem.generated.h"

class UAudioComponent;

UCLASS()
class MIDNIGHTHAVOC_API UHavocAudioSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    /** Plays the sound or spawns the Niagara system mapped to Role. Returns false if nothing was played. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Audio")
    bool PlayRole(FName Role, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);

    /** 2D UI/radio style sound for a role. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Audio")
    bool PlayRole2D(FName Role);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Audio")
    void StartAmbient();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Audio")
    void StopAmbient();

protected:
    UFUNCTION()
    void HandleHavocEvent(FName EventName, const FString& Payload);

private:
    UObject* ResolveRole(FName Role);

    UPROPERTY()
    TMap<FName, TObjectPtr<UObject>> Cache;

    /** Roles that failed once; not retried so the log is not spammed. */
    TSet<FName> Missing;

    UPROPERTY()
    TObjectPtr<UAudioComponent> Ambient;
};
