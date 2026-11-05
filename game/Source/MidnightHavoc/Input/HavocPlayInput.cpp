#include "Input/HavocPlayInput.h"

#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"

namespace
{
    // 0 = bool, 1 = axis1D, 2 = axis2D
    EInputActionValueType ToValueType(int32 V)
    {
        return V == 0 ? EInputActionValueType::Boolean : V == 1 ? EInputActionValueType::Axis1D : EInputActionValueType::Axis2D;
    }

    UInputModifierNegate* Negate(UObject* Outer, bool bX, bool bY, bool bZ)
    {
        UInputModifierNegate* M = NewObject<UInputModifierNegate>(Outer);
        M->bX = bX; M->bY = bY; M->bZ = bZ;
        return M;
    }

    UInputModifierSwizzleAxis* SwizzleYXZ(UObject* Outer)
    {
        UInputModifierSwizzleAxis* M = NewObject<UInputModifierSwizzleAxis>(Outer);
        M->Order = EInputAxisSwizzle::YXZ;
        return M;
    }

    /** WASD into a 2D axis (X = right, Y = forward). */
    void MapWasd(UInputMappingContext* C, const UInputAction* Action, const FKey& Up, const FKey& Down, const FKey& Left, const FKey& Right)
    {
        C->MapKey(Action, Right);
        C->MapKey(Action, Left).Modifiers.Add(Negate(C, true, true, true));
        FEnhancedActionKeyMapping& Fwd = C->MapKey(Action, Up);
        Fwd.Modifiers.Add(SwizzleYXZ(C));
        FEnhancedActionKeyMapping& Back = C->MapKey(Action, Down);
        Back.Modifiers.Add(SwizzleYXZ(C));
        Back.Modifiers.Add(Negate(C, true, true, true));
    }

    void MapMouseLook(UInputMappingContext* C, const UInputAction* Action)
    {
        // Mouse up gives +Y; AddControllerPitchInput treats + as look down, so negate Y.
        C->MapKey(Action, EKeys::Mouse2D).Modifiers.Add(Negate(C, false, true, false));
    }
}

UInputAction* UHavocPlayInput::MakeAction(const TCHAR* Name, int32 ValueType)
{
    UInputAction* Action = NewObject<UInputAction>(this, Name);
    Action->ValueType = ToValueType(ValueType);
    return Action;
}

void UHavocPlayInput::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    PauseAction = MakeAction(TEXT("IA_Pause"), 0);
    ChooseNorthAction = MakeAction(TEXT("IA_ChooseNorth"), 0);
    ChooseWestAction = MakeAction(TEXT("IA_ChooseWest"), 0);
    InteractAction = MakeAction(TEXT("IA_Interact"), 0);
    DeployRavenAction = MakeAction(TEXT("IA_DeployRaven"), 0);
    CallFalconAction = MakeAction(TEXT("IA_CallFalcon"), 0);
    ReturnAction = MakeAction(TEXT("IA_Return"), 0);

    MoveAction = MakeAction(TEXT("IA_Move"), 2);
    LookAction = MakeAction(TEXT("IA_Look"), 2);
    JumpAction = MakeAction(TEXT("IA_Jump"), 0);
    SprintAction = MakeAction(TEXT("IA_Sprint"), 0);
    CrouchAction = MakeAction(TEXT("IA_Crouch"), 0);
    AimAction = MakeAction(TEXT("IA_Aim"), 0);
    FireAction = MakeAction(TEXT("IA_Fire"), 0);
    ReloadAction = MakeAction(TEXT("IA_Reload"), 0);

    RiseAction = MakeAction(TEXT("IA_Rise"), 1);

    CollectiveAction = MakeAction(TEXT("IA_Collective"), 1);
    CyclicAction = MakeAction(TEXT("IA_Cyclic"), 2);
    YawAction = MakeAction(TEXT("IA_FalconYaw"), 1);

    BuildContexts();
}

void UHavocPlayInput::BuildContexts()
{
    MetaContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Meta"));
    MetaContext->MapKey(PauseAction, EKeys::Escape);
    MetaContext->MapKey(PauseAction, EKeys::P);
    MetaContext->MapKey(ChooseNorthAction, EKeys::One);
    MetaContext->MapKey(ChooseWestAction, EKeys::Two);
    MetaContext->MapKey(InteractAction, EKeys::E);
    MetaContext->MapKey(DeployRavenAction, EKeys::T);
    MetaContext->MapKey(CallFalconAction, EKeys::G);
    MetaContext->MapKey(ReturnAction, EKeys::Tab);

    FootContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Foot"));
    MapWasd(FootContext, MoveAction, EKeys::W, EKeys::S, EKeys::A, EKeys::D);
    MapMouseLook(FootContext, LookAction);
    FootContext->MapKey(JumpAction, EKeys::SpaceBar);
    FootContext->MapKey(SprintAction, EKeys::LeftShift);
    FootContext->MapKey(CrouchAction, EKeys::C);
    FootContext->MapKey(AimAction, EKeys::RightMouseButton);
    FootContext->MapKey(FireAction, EKeys::LeftMouseButton);
    FootContext->MapKey(ReloadAction, EKeys::R);

    DroneContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Drone"));
    MapWasd(DroneContext, MoveAction, EKeys::W, EKeys::S, EKeys::A, EKeys::D);
    MapMouseLook(DroneContext, LookAction);
    DroneContext->MapKey(RiseAction, EKeys::SpaceBar);
    DroneContext->MapKey(RiseAction, EKeys::LeftControl).Modifiers.Add(Negate(DroneContext, true, true, true));

    FalconContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Falcon"));
    FalconContext->MapKey(CollectiveAction, EKeys::SpaceBar);
    FalconContext->MapKey(CollectiveAction, EKeys::LeftControl).Modifiers.Add(Negate(FalconContext, true, true, true));
    MapWasd(FalconContext, CyclicAction, EKeys::W, EKeys::S, EKeys::A, EKeys::D);
    FalconContext->MapKey(YawAction, EKeys::E);
    // Q is not used by Meta, so it is free for yaw left on the Falcon.
    FalconContext->MapKey(YawAction, EKeys::Q).Modifiers.Add(Negate(FalconContext, true, true, true));
}

void UHavocPlayInput::ApplyContexts(APlayerController* Controller, UInputMappingContext* PawnContext, int32 PawnPriority) const
{
    const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
    UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    if (!Subsystem)
    {
        return;
    }
    Subsystem->ClearAllMappings();
    Subsystem->AddMappingContext(MetaContext, 0);
    if (PawnContext)
    {
        Subsystem->AddMappingContext(PawnContext, PawnPriority);
    }
}

void UHavocPlayInput::EnsureMeta(APlayerController* Controller) const
{
    const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
    {
        Subsystem->AddMappingContext(MetaContext, 0);
    }
}
