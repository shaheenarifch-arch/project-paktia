#pragma once

// Falcon helicopter pawn.
// Ports: src/gameplay/aircraft/HelicopterController.ts, HelicopterDamageState.ts, CrewState.ts
// and the crash/recovery hooks of HelicopterCrashController.ts (see HavocHelicopterModel.h and
// HavocCrashSequence.h for the pure-data ports these call).
//
// Flight is a simple arcade model on a simulating physics body: collective sets lift, cyclic tilts
// the aircraft toward a target up-vector (so it translates), yaw is a torque about world up.
// Engine condition scales lift, flight-control condition scales cyclic and yaw authority.
// State changes are server-authoritative; clients send input through ServerSetFlightInput.

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Aircraft/HavocAircraftTypes.h"
#include "Aircraft/HavocHelicopterModel.h"
#include "Aircraft/HavocCrashSequence.h"
#include "HavocFalconHelicopter.generated.h"

class UInputAction;
class UInputMappingContext;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHavocHelicopterDamageDelegate, const FHavocHelicopterDamageSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHavocCrashSequenceDelegate, EHavocCrashSequenceState, NewState);

UCLASS()
class MIDNIGHTHAVOC_API AHavocFalconHelicopter : public APawn
{
    GENERATED_BODY()

public:
    AHavocFalconHelicopter();

    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void PawnClientRestart() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ---- Components (assign a mesh in the Blueprint child; physics needs one to move) ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Aircraft")
    TObjectPtr<UStaticMeshComponent> Body;

    // ---- Enhanced Input (assign assets in the Blueprint child) ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Input")
    TObjectPtr<UInputMappingContext> MappingContext;

    /** Axis1D: +1 raises collective, -1 lowers it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Input")
    TObjectPtr<UInputAction> CollectiveAction;

    /** Axis2D: X = roll right, Y = pitch forward. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Input")
    TObjectPtr<UInputAction> CyclicAction;

    /** Axis1D: +1 yaws right. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Input")
    TObjectPtr<UInputAction> YawAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Input")
    int32 MappingPriority = 0;

    // ---- Data ----
    /** Id of the aircraft in data/aircraft (UHavocDataSubsystem). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Aircraft")
    FName AircraftId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Aircraft")
    FString Callsign;

    /** Seats loaded from the aircraft definition in BeginPlay. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    TArray<FString> Seats;

    // ---- Flight tuning (Unreal units: cm, seconds) ----
    /** Collective level at which lift equals gravity (0..1). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Flight", meta = (ClampMin = "0.1", ClampMax = "0.9"))
    double HoverCollective = 0.5;

    /** Collective change per second at full input. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Flight")
    double CollectiveRate = 0.4;

    /** Maximum tilt from level in degrees at full cyclic. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Flight")
    double MaxTiltDegrees = 20.0;

    /** Angular acceleration (rad/s^2 per unit of tilt error) toward the commanded tilt. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Flight")
    double TiltResponse = 6.0;

    /** Yaw angular acceleration at full input (rad/s^2). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Flight")
    double YawAcceleration = 2.0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Flight")
    float LinearDamping = 0.6f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Flight")
    float AngularDamping = 3.0f;

    /** Radius used for IsInRecoveryZone, in Unreal units (TS default 40 m). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Aircraft")
    double RecoveryRadius = 4000.0;

    // ---- State ----
    UPROPERTY(ReplicatedUsing = OnRep_Helicopter, BlueprintReadOnly, Category = "Havoc|Aircraft")
    FHavocHelicopterState Helicopter;

    UPROPERTY(ReplicatedUsing = OnRep_Crash, BlueprintReadOnly, Category = "Havoc|Aircraft")
    FHavocCrashSequence Crash;

    UPROPERTY(BlueprintAssignable, Category = "Havoc|Aircraft")
    FHavocHelicopterDamageDelegate OnDamageChanged;

    UPROPERTY(BlueprintAssignable, Category = "Havoc|Aircraft")
    FHavocCrashSequenceDelegate OnCrashSequenceChanged;

    // ---- Queries ----
    UFUNCTION(BlueprintPure, Category = "Havoc|Aircraft")
    EHavocHelicopterFlightState GetFlightState() const { return Helicopter.FlightState; }

    UFUNCTION(BlueprintPure, Category = "Havoc|Aircraft")
    EHavocHelicopterDamageLevel GetDamageLevel() const { return Helicopter.Damage.GetLevel(); }

    UFUNCTION(BlueprintPure, Category = "Havoc|Aircraft")
    FHavocHelicopterDamageSnapshot GetDamageSnapshot() const { return Helicopter.Damage.Snapshot(); }

    UFUNCTION(BlueprintPure, Category = "Havoc|Aircraft")
    EHavocCrashSequenceState GetCrashSequenceState() const { return Crash.State; }

    UFUNCTION(BlueprintPure, Category = "Havoc|Aircraft")
    TArray<FHavocCrewMember> GetCrew() const { return Crash.Crew.Members; }

    UFUNCTION(BlueprintPure, Category = "Havoc|Aircraft")
    double GetCollectiveLevel() const { return CollectiveLevel; }

    UFUNCTION(BlueprintPure, Category = "Havoc|Aircraft")
    bool IsInRecoveryZone(const FVector& WorldPosition) const;

    // ---- Damage ----
    /** Ports HelicopterDamageState.applyDamage. Server only. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft")
    void ApplyDamage(double FlightControl, double Engine, double Structure);

    // ---- Flight state hooks (HelicopterController.ts) ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft")
    void ReportDistress();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft")
    void AttemptEmergencyLanding(const FVector& WorldPosition);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft")
    void RecoverAircraft();

    // ---- Crash and recovery hooks (HelicopterCrashController.ts). Server only; out-of-order calls are ignored like the TS ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    void BeginInbound(const FVector& WorldPosition);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    void TriggerDistress();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    void LoseSignal();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    void RegisterCrash(const FVector& WorldPosition);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    void BeginSearch();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    void LocateCrashSite(const FVector& WorldPosition);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    void SecureCrashSite();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    void BeginCrewRecovery();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    void StartRecoveryDefense();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    bool CompleteEvacuation();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    void Abandon();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    bool LocateCrew(FName CrewId, EHavocCrewCondition Condition);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Aircraft|Crash")
    bool RecoverCrew(FName CrewId);

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnRep_Helicopter();

    UFUNCTION()
    void OnRep_Crash();

    /** Clients send their input here; the server applies the forces. */
    UFUNCTION(Server, Unreliable)
    void ServerSetFlightInput(double InCollective, FVector2D InCyclic, double InYaw);

private:
    void OnCollective(const FInputActionValue& Value);
    void OnCyclic(const FInputActionValue& Value);
    void OnYaw(const FInputActionValue& Value);
    void OnInputReleased();
    void SendInput();
    void ApplyFlightForces(float DeltaSeconds);

    void NotifyDamage();
    void NotifyCrashState();
    void PublishEvent(const TCHAR* EventName, const FString& Payload) const;
    bool CanMutate() const;

    double CollectiveInput = 0.0;
    FVector2D CyclicInput = FVector2D::ZeroVector;
    double YawInput = 0.0;

    /** 0..1. Starts on the ground at 0. */
    double CollectiveLevel = 0.0;
};
