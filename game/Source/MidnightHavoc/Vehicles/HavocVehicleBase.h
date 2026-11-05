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

/** Pure movement helpers (unit tested). Units: cm and seconds unless stated. */
namespace HavocVehicleMath
{
    /** Moves Speed toward Throttle * MaxSpeed (Accel per second); with no throttle, Drag per second slows it to 0. */
    MIDNIGHTHAVOC_API double StepSpeed(double Speed, double Throttle, double MaxSpeed, double Accel, double Drag, double Dt);

    /** Yaw change in degrees: full TurnRate at MaxSpeed, none when stopped (surface vehicles). Reversing flips steering like a car. */
    MIDNIGHTHAVOC_API double SurfaceYawDelta(double Steer, double Speed, double MaxSpeed, double TurnRate, double Dt);
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

private:
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
