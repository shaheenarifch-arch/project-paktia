#pragma once

// Ports: src/gameplay/missions/episode02/RelayNetwork.ts
// Field relays that carry the drone link down the corridor. Signal at a point is the best link through
// any working relay (FHavocDroneSignalManager), minus the current interference. A communications
// outage jams every relay until a squad restores it in person. Positions are game-space metres
// from Checkpoint Echo, as in the TS.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Drones/HavocDroneTypes.h"
#include "HavocRelayNetwork.generated.h"

class FJsonObject;

UENUM(BlueprintType)
enum class EHavocRelayState : uint8
{
    Stowed,
    Online,
    Jammed
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocFieldRelay
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    double Range = 1600.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    EHavocRelayState State = EHavocRelayState::Online;
};

UCLASS()
class MIDNIGHTHAVOC_API UHavocRelayNetwork : public UObject
{
    GENERATED_BODY()

public:
    /** TS RELAY_RESTORE_DISTANCE: how close (metres) a squad must stand to bring a jammed relay back. */
    static constexpr double RestoreDistance = 30.0;

    /** Resets to the base relay "echo-base" at the origin, range 1600, online. */
    UHavocRelayNetwork();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    FHavocFieldRelay Place(const FString& Id, const FVector& Position, double Range = 1600.0);

    /** Communications outage: every online field relay is jammed. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    void JamAll(double InInterference = 0.35);

    /** A squad standing at the relay brings it back. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool Restore(const FString& Id, const FVector& SquadPosition);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    double SignalAt(const FVector& Position) const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    EHavocSignalState Classify(const FVector& Position) const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    int32 GetOnline() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    double GetInterference() const { return Interference; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    TArray<FHavocFieldRelay> GetRelays() const { return Relays; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    FHavocFieldRelay GetBase() const { return Base; }

    /** The base plus every online field relay: what the drone link can use. */
    TArray<FHavocFieldRelay> GetOnlineSources() const;

    bool FindRelay(const FString& Id, FHavocFieldRelay& OutRelay) const;

    TSharedRef<FJsonObject> Snapshot() const;
    void RestoreSnapshot(const TSharedPtr<FJsonObject>& Data);

private:
    UPROPERTY()
    TArray<FHavocFieldRelay> Relays;

    UPROPERTY()
    FHavocFieldRelay Base;

    UPROPERTY()
    double Interference = 0.0;
};
