#pragma once

// Base class of the playable placeholder vehicles for Episode 04 (boat, ground vehicle, helicopter,
// recon drone). Data: stats, seats and the fictional flag come from data/vehicles/<id>.json through
// UHavocDataSubsystem. Art: the mesh comes from a role in Config/HavocAssets.ini through
// UHavocAssetSubsystem (item 3 / 5 roles); a missing asset is logged and the vehicle still works.
// Docs: game/docs/vehicles.md
//
// Movement is a simple arcade model (a swept kinematic step), server authoritative; the owning client
// sends input through ServerSetInput. Chaos Vehicles is the upgrade path for the ground vehicle and boat:
// https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-vehicles-overview-in-unreal-engine
// Enter / exit is server only: Enter(Controller) possesses the vehicle, Exit() hands the controller back
// to the pawn it came from. Input uses Enhanced Input (UHavocPlayInput::VehicleContext).
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Data/HavocVehicleDef.h"
#include "HavocVehicleBase.generated.h"

class AController;
class UBoxComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UInputMappingContext;

/** Damage state of a vehicle; thresholds mirror HelicopterDamageState.ts / ConvoyEscort.ts (integrity 0..1). */
UENUM(BlueprintType)
enum class EHavocVehicleDamage : uint8
{
    Operational,
    Damaged,
    Critical,
    Disabled,
    Destroyed
};

/** Pure movement helpers (unit tested). Units: cm and seconds unless stated. */
namespace HavocVehicleMath
{
    /** Moves Speed toward Throttle * MaxSpeed (Accel per second); with no throttle, Drag per second slows it to 0. */
    MIDNIGHTHAVOC_API double StepSpeed(double Speed, double Throttle, double MaxSpeed, double Accel, double Drag, double Dt);

    /** Yaw change in degrees: full TurnRate at MaxSpeed, none when stopped (surface vehicles). Reversing flips steering like a car. */
    MIDNIGHTHAVOC_API double SurfaceYawDelta(double Steer, double Speed, double MaxSpeed, double TurnRate, double Dt);

    /** Tracked (skid) steering: full TurnRate even when stopped (pivot turn); reversing flips steering. */
    MIDNIGHTHAVOC_API double TrackedYawDelta(double Steer, double Speed, double TurnRate, double Dt);

    /** Damage level from integrity 0..1: <=0 Destroyed, <=0.15 Disabled, <=0.35 Critical, <=0.7 Damaged. */
    MIDNIGHTHAVOC_API EHavocVehicleDamage DamageLevel(double Integrity);

    /** Speed / turn multiplier for a damage level: 1, 0.8, 0.5, then 0 once disabled. */
    MIDNIGHTHAVOC_API double MobilityFactor(EHavocVehicleDamage Level);

    /** Convoy follow: throttle / steer in [-1, 1] to reach a point Gap cm behind the target (ground plane only). */
    MIDNIGHTHAVOC_API void FollowInput(const FVector& SelfLocation, const FVector& SelfForward, const FVector& TargetLocation, double Gap, double& OutThrottle, double& OutSteer);
}

UCLASS(Abstract)
class MIDNIGHTHAVOC_API AHavocVehicleBase : public APawn
{
    GENERATED_BODY()

public:
    AHavocVehicleBase();

    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void PawnClientRestart() override;

    // ---- Components ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Vehicle")
    TObjectPtr<UBoxComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Vehicle")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Vehicle")
    TObjectPtr<USkeletalMeshComponent> SkeletalMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Vehicle")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Vehicle")
    TObjectPtr<UCameraComponent> Camera;

    // ---- Data ----
    /** Id in data/vehicles. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Vehicle")
    FName VehicleId;

    /** Role in Config/HavocAssets.ini whose asset is the visual. Empty or unmapped: no visual. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Vehicle")
    FName AssetRole;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Vehicle")
    FHavocVehicleStats Stats;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Vehicle")
    TArray<FString> Seats;

    // ---- Enhanced Input (optional override; the default comes from UHavocPlayInput) ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Input")
    TObjectPtr<UInputMappingContext> MappingContext;

    /** Distance in cm within which a controlled pawn can get in. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Vehicle")
    float EnterRange = 450.f;

    /** Where the previous pawn is put on exit, relative to the vehicle, in cm. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Vehicle")
    FVector ExitOffset = FVector(0.0, 350.0, 100.0);

    /** Convoy id used in the "convoy:vehicle" event, matching ConvoyEscort ids ("warden-1", "nomad-1", ...). Empty: no event. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Vehicle")
    FName ConvoyId;

    // ---- Damage (server) ----
    /** 0..1; 0 is destroyed. Same scale as ConvoyEscort integrity. */
    UFUNCTION(BlueprintPure, Category = "Havoc|Vehicle")
    float GetIntegrity() const { return Integrity; }

    UFUNCTION(BlueprintPure, Category = "Havoc|Vehicle")
    EHavocVehicleDamage GetDamageLevel() const { return HavocVehicleMath::DamageLevel(Integrity); }

    UFUNCTION(BlueprintPure, Category = "Havoc|Vehicle")
    bool IsDestroyed() const { return Integrity <= 0.f; }

    /** 1 at full speed down to 0 when disabled or destroyed. */
    UFUNCTION(BlueprintPure, Category = "Havoc|Vehicle")
    double GetMobilityFactor() const { return HavocVehicleMath::MobilityFactor(GetDamageLevel()); }

    /** Reduces integrity by Amount (0..1 scale). A destroyed vehicle throws its driver out. Publishes "convoy:vehicle" when ConvoyId is set. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Vehicle")
    void ApplyVehicleDamage(float Amount);

    /** Restores integrity; a destroyed vehicle cannot be repaired in the field. False if destroyed. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Vehicle")
    bool RepairVehicle(float Amount);

    // ---- Queries ----
    UFUNCTION(BlueprintPure, Category = "Havoc|Vehicle")
    bool IsOccupied() const { return Driver != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Havoc|Vehicle")
    AController* GetDriver() const { return Driver; }

    UFUNCTION(BlueprintPure, Category = "Havoc|Vehicle")
    bool CanEnter(const AController* InController) const;

    UFUNCTION(BlueprintPure, Category = "Havoc|Vehicle")
    double GetForwardSpeedCm() const { return Speed; }

    // ---- Enter / exit (server only) ----
    /** Possesses the vehicle. False if occupied, out of range, or the controller has no pawn to return to. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Vehicle")
    bool Enter(AController* InController);

    /** Returns the driver to the pawn they came from. False if nobody is inside. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Vehicle")
    bool Exit();

    /** Applies stats and seats from a definition (BeginPlay does this from the data subsystem; tests call it directly). */
    void ApplyDefinition(const FHavocVehicleDef& Def);

    /** Nearest enterable vehicle in range of the controller's pawn, or null. */
    static AHavocVehicleBase* FindEnterable(const AController* InController);

    /** Sets input in [-1, 1]: throttle (W/S), steer (D/A), lift (Space/Ctrl). Authority only; clients use ServerSetInput. */
    void SetInput(double InThrottle, double InSteer, double InLift);

    /** One movement step (server). Public so tests can step without a ticking world. */
    virtual void StepMovement(float Dt) PURE_VIRTUAL(AHavocVehicleBase::StepMovement, );

protected:
    virtual void BeginPlay() override;

    /** Someone else took the controller (e.g. a checkpoint restore): free the seat and show the previous pawn again. */
    virtual void UnPossessed() override;

    /** Moves by Delta with a sweep so the vehicle stops at blocking geometry, and sets the new rotation. */
    void MoveSwept(const FVector& Delta, const FQuat& NewRotation);

    UFUNCTION(Server, Unreliable)
    void ServerSetInput(double InThrottle, double InSteer, double InLift);

    double Throttle = 0.0;
    double Steer = 0.0;
    double Lift = 0.0;
    /** Forward speed, cm/s. */
    double Speed = 0.0;
    float Integrity = 1.f;

private:
    void PublishDamage() const;
    void LoadVisual();
    void OnThrottle(const FInputActionValue& Value);
    void OnSteer(const FInputActionValue& Value);
    void OnLift(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnReleased();
    void SendInput();

    UPROPERTY() TObjectPtr<AController> Driver;
    UPROPERTY() TObjectPtr<APawn> PreviousPawn;
};
