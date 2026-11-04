#pragma once

// Ports: src/gameplay/missions/episode04/TankerEscort.ts
// Tankers waiting to transit. Each one through counts toward the supply flow the world
// sees; a tanker at 0 integrity is lost.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PaktiaTankerEscort.generated.h"

class FJsonObject;

UENUM(BlueprintType)
enum class EPaktiaTankerState : uint8
{
    Waiting,
    Transiting,
    Through,
    Lost
};

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaTanker
{
    GENERATED_BODY()

    /** "tanker-1" .. "tanker-N". */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Episode04")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Episode04")
    EPaktiaTankerState State = EPaktiaTankerState::Waiting;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Episode04")
    double Integrity = 1.0;

    /** Lane in use once transiting; None before. */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Episode04")
    FName Lane;
};

UCLASS()
class PROJECTPAKTIA_API UPaktiaTankerEscort : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    void Assemble(int32 Count);

    /** Starts a transit; only a waiting tanker on an open lane may begin. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    bool Begin(const FString& TankerId, FName LaneId, bool bLaneOpen);

    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    bool Arrive(const FString& TankerId);

    /** Damages a tanker that is not already through or lost; 0 integrity means lost. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    void Damage(const FString& TankerId, double Amount);

    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    int32 GetThrough() const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    int32 GetLost() const;

    /** True once every tanker is through or lost (and there is at least one). */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    bool IsSettled() const;

    /** Share of planned tankers that got through, 0..1 (3 decimals). */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    double GetSupplyFlow() const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    TArray<FPaktiaTanker> GetTankers() const { return Tankers; }

    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    int32 NumTankers() const { return Tankers.Num(); }

    TSharedRef<FJsonObject> Snapshot() const;
    void Restore(const TSharedPtr<FJsonObject>& Data);

private:
    FPaktiaTanker* FindTanker(const FString& TankerId);

    UPROPERTY()
    TArray<FPaktiaTanker> Tankers;
};
