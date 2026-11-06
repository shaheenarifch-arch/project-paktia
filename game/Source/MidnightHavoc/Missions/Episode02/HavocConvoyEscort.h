#pragma once

// Ports: src/gameplay/missions/episode02/ConvoyEscort.ts
// The Episode 02 convoy: Warden-1 leads, Nomad-1 carries the recovered Falcon-2 crew, Nomad-2 carries
// supplies. Integrity is 0..1; at 0 a vehicle is destroyed and its cargo is lost. Vehicles stay out of
// tunnels (data/maps/gardun-city.json undergroundRoutes). Abstract game rules only.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HavocConvoyEscort.generated.h"

class FJsonObject;

UENUM(BlueprintType)
enum class EHavocConvoyVehicleKind : uint8
{
    Warden,
    Nomad
};

UENUM(BlueprintType)
enum class EHavocConvoyCargo : uint8
{
    FalconCrew,
    Supplies,
    None
};

UENUM(BlueprintType)
enum class EHavocConvoyState : uint8
{
    Forming,
    Moving,
    Halted,
    Ambushed,
    Arrived
};

UENUM(BlueprintType)
enum class EHavocConvoyRoute : uint8
{
    MainRoad,
    Riverbed,
    RiverDrainage
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocConvoyVehicle
{
    GENERATED_BODY()

    /** "warden-1", "nomad-1", "nomad-2". */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    EHavocConvoyVehicleKind Kind = EHavocConvoyVehicleKind::Nomad;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    EHavocConvoyCargo Cargo = EHavocConvoyCargo::None;

    /** 0..1 */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    double Integrity = 1.0;
};

UCLASS()
class MIDNIGHTHAVOC_API UHavocConvoyEscort : public UObject
{
    GENERATED_BODY()

public:
    /** bWithCrew is false when Mission 01 abandoned the crash recovery: Nomad-1 carries supplies instead. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    void Form(bool bWithCrew = true);

    /** The drainage tunnel is an infantry route: no vehicle fits. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    static bool CanUseRoute(EHavocConvoyVehicleKind Kind, EHavocConvoyRoute Route);

    /** The whole convoy moves on a route every surviving vehicle can use. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool Depart(EHavocConvoyRoute InRoute);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    void Halt();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    void Ambush();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool Resume();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    void Arrive();

    /** Returns false for an unknown vehicle. A destroyed vehicle is left as it is. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool ApplyDamage(const FString& VehicleId, double Amount);

    /** Destroyed vehicles cannot be repaired in the field. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool Repair(const FString& VehicleId, double Amount);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    TArray<FHavocConvoyVehicle> GetVehicles() const { return Vehicles; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    TArray<FHavocConvoyVehicle> GetAlive() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    EHavocConvoyState GetState() const { return State; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    EHavocConvoyRoute GetRoute() const { return Route; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool CarriesCrew() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool IsCrewSafe() const;

    /** The convoy has failed: the crew transport is gone, or (without crew) every Nomad is. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool IsMissionCritical() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    int32 GetNomadsRemaining() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool IsWardenOperational() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool IsFullStrength() const;

    TSharedRef<FJsonObject> Snapshot() const;
    void Restore(const TSharedPtr<FJsonObject>& Data);

private:
    UPROPERTY()
    EHavocConvoyState State = EHavocConvoyState::Forming;

    UPROPERTY()
    EHavocConvoyRoute Route = EHavocConvoyRoute::MainRoad;

    UPROPERTY()
    TArray<FHavocConvoyVehicle> Vehicles;
};
