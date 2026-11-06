#pragma once

// Builds the Enhanced Input actions and mapping contexts for the playable loop in C++, so the game
// is playable without editor-made assets. Designers can still override with their own assets on the
// Blueprint children (the pawn properties are checked before these are used).
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine
//
// Contexts: Meta (always on: pause, beach choice, drone/Falcon calls, interact, return),
//           Foot (Marine), Drone (Raven), Falcon, Vehicle (boat, ground vehicle, helicopter, recon drone).
// Keys are listed in game/README.md "How to play".

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InputCoreTypes.h"
#include "Player/HavocUITypes.h"
#include "HavocPlayInput.generated.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;
class UHavocPlaySettings;

/** One line of the How to play screen. Keys come from the live mapping contexts. */
struct FHavocControlRow
{
    FString Section;   // "On foot", "Vehicles", "Raven drone", "Falcon helicopter", "Menus"
    FString Action;
    FString Keys;
};

UCLASS()
class MIDNIGHTHAVOC_API UHavocPlayInput : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // ---- Contexts ----
    UPROPERTY() TObjectPtr<UInputMappingContext> MetaContext;
    UPROPERTY() TObjectPtr<UInputMappingContext> FootContext;
    UPROPERTY() TObjectPtr<UInputMappingContext> DroneContext;
    UPROPERTY() TObjectPtr<UInputMappingContext> FalconContext;
    UPROPERTY() TObjectPtr<UInputMappingContext> VehicleContext;

    // ---- Meta actions (bound on the player controller) ----
    UPROPERTY() TObjectPtr<UInputAction> PauseAction;
    UPROPERTY() TObjectPtr<UInputAction> ChooseNorthAction;
    UPROPERTY() TObjectPtr<UInputAction> ChooseWestAction;
    UPROPERTY() TObjectPtr<UInputAction> InteractAction;
    UPROPERTY() TObjectPtr<UInputAction> DeployRavenAction;
    UPROPERTY() TObjectPtr<UInputAction> CallFalconAction;
    UPROPERTY() TObjectPtr<UInputAction> ReturnAction;
    /** F: get into the nearest vehicle, or out of the one being driven. */
    UPROPERTY() TObjectPtr<UInputAction> VehicleAction;
    /** H / gamepad X: dismisses the first-time hint. */
    UPROPERTY() TObjectPtr<UInputAction> DismissHintAction;

    // ---- On foot ----
    UPROPERTY() TObjectPtr<UInputAction> MoveAction;
    UPROPERTY() TObjectPtr<UInputAction> LookAction;
    UPROPERTY() TObjectPtr<UInputAction> JumpAction;
    UPROPERTY() TObjectPtr<UInputAction> SprintAction;
    UPROPERTY() TObjectPtr<UInputAction> CrouchAction;
    UPROPERTY() TObjectPtr<UInputAction> AimAction;
    UPROPERTY() TObjectPtr<UInputAction> FireAction;
    UPROPERTY() TObjectPtr<UInputAction> ReloadAction;

    // ---- Raven (also uses MoveAction / LookAction) ----
    UPROPERTY() TObjectPtr<UInputAction> RiseAction;

    // ---- Vehicles (also use LookAction and RiseAction) ----
    UPROPERTY() TObjectPtr<UInputAction> ThrottleAction;
    UPROPERTY() TObjectPtr<UInputAction> SteerAction;

    // ---- Falcon (assigned to AHavocFalconHelicopter's properties) ----
    UPROPERTY() TObjectPtr<UInputAction> CollectiveAction;
    UPROPERTY() TObjectPtr<UInputAction> CyclicAction;
    UPROPERTY() TObjectPtr<UInputAction> YawAction;

    /** Re-maps the rebindable keys (interact, reload, Raven, Falcon, return) from the saved settings. */
    void ApplyBindings(const UHavocPlaySettings& Settings);

    /** Every binding grouped by section, read from the current mapping contexts (so rebinds show at once). */
    TArray<FHavocControlRow> BuildControlRows() const;

    /** Short readable name for a key ("Mouse", "Start", "Left Ctrl"). */
    static FString KeyName(const FKey& Key);

    /** First-time hint text for a kind of machine, using the live keys. */
    FString HintFor(EHavocHintKind Kind) const;

    /** Adds the Meta context only (idempotent). Used after possessing a pawn that clears all mappings. */
    void EnsureMeta(APlayerController* Controller) const;

    /** Clears every mapping and adds Meta plus the given pawn context (null for Meta only). */
    void ApplyContexts(APlayerController* Controller, UInputMappingContext* PawnContext, int32 PawnPriority = 1) const;

private:
    UInputAction* MakeAction(const TCHAR* Name, int32 ValueType);
    void BuildContexts();
};
