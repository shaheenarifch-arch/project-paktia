#include "Input/HavocPlayInput.h"

#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Player/HavocPlaySettings.h"

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
    Collection.InitializeDependency<UHavocPlaySettings>();
    Super::Initialize(Collection);

    PauseAction = MakeAction(TEXT("IA_Pause"), 0);
    ChooseNorthAction = MakeAction(TEXT("IA_ChooseNorth"), 0);
    ChooseWestAction = MakeAction(TEXT("IA_ChooseWest"), 0);
    InteractAction = MakeAction(TEXT("IA_Interact"), 0);
    DeployRavenAction = MakeAction(TEXT("IA_DeployRaven"), 0);
    CallFalconAction = MakeAction(TEXT("IA_CallFalcon"), 0);
    ReturnAction = MakeAction(TEXT("IA_Return"), 0);
    VehicleAction = MakeAction(TEXT("IA_Vehicle"), 0);
    DismissHintAction = MakeAction(TEXT("IA_DismissHint"), 0);

    MoveAction = MakeAction(TEXT("IA_Move"), 2);
    LookAction = MakeAction(TEXT("IA_Look"), 2);
    JumpAction = MakeAction(TEXT("IA_Jump"), 0);
    SprintAction = MakeAction(TEXT("IA_Sprint"), 0);
    CrouchAction = MakeAction(TEXT("IA_Crouch"), 0);
    AimAction = MakeAction(TEXT("IA_Aim"), 0);
    FireAction = MakeAction(TEXT("IA_Fire"), 0);
    ReloadAction = MakeAction(TEXT("IA_Reload"), 0);

    RiseAction = MakeAction(TEXT("IA_Rise"), 1);
    ThrottleAction = MakeAction(TEXT("IA_Throttle"), 1);
    SteerAction = MakeAction(TEXT("IA_Steer"), 1);

    CollectiveAction = MakeAction(TEXT("IA_Collective"), 1);
    CyclicAction = MakeAction(TEXT("IA_Cyclic"), 2);
    YawAction = MakeAction(TEXT("IA_FalconYaw"), 1);

    BuildContexts();
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (const UHavocPlaySettings* Settings = GI->GetSubsystem<UHavocPlaySettings>())
        {
            ApplyBindings(*Settings);
        }
    }
}

void UHavocPlayInput::ApplyBindings(const UHavocPlaySettings& Settings)
{
    auto Remap = [](UInputMappingContext* Context, const UInputAction* Action, const FKey& Key)
    {
        if (Context && Action && Key.IsValid())
        {
            Context->UnmapAllKeysFromAction(Action);
            Context->MapKey(Action, Key);
        }
    };
    Remap(MetaContext, InteractAction, Settings.InteractKey);
    Remap(MetaContext, DeployRavenAction, Settings.RavenKey);
    Remap(MetaContext, CallFalconAction, Settings.FalconKey);
    Remap(MetaContext, ReturnAction, Settings.ReturnKey);
    Remap(FootContext, ReloadAction, Settings.ReloadKey);

    // Live players pick the new keys up on the next rebuild.
    if (const UGameInstance* GI = GetGameInstance())
    {
        for (ULocalPlayer* LocalPlayer : GI->GetLocalPlayers())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
            {
                Subsystem->RequestRebuildControlMappings();
            }
        }
    }
}

void UHavocPlayInput::BuildContexts()
{
    MetaContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Meta"));
    MetaContext->MapKey(PauseAction, EKeys::Escape);
    MetaContext->MapKey(PauseAction, EKeys::P);
    MetaContext->MapKey(PauseAction, EKeys::Gamepad_Special_Right); // Start / Menu
    MetaContext->MapKey(ChooseNorthAction, EKeys::One);
    MetaContext->MapKey(ChooseWestAction, EKeys::Two);
    MetaContext->MapKey(InteractAction, EKeys::E);
    MetaContext->MapKey(DeployRavenAction, EKeys::T);
    MetaContext->MapKey(CallFalconAction, EKeys::G);
    MetaContext->MapKey(ReturnAction, EKeys::Tab);
    MetaContext->MapKey(VehicleAction, EKeys::F);
    MetaContext->MapKey(DismissHintAction, EKeys::H);
    MetaContext->MapKey(DismissHintAction, EKeys::Gamepad_FaceButton_Left); // X

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

    // Vehicles: W/S throttle, D/A steer (or yaw), Space/Ctrl up and down (flyers), mouse orbits the camera.
    VehicleContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Vehicle"));
    VehicleContext->MapKey(ThrottleAction, EKeys::W);
    VehicleContext->MapKey(ThrottleAction, EKeys::S).Modifiers.Add(Negate(VehicleContext, true, true, true));
    VehicleContext->MapKey(SteerAction, EKeys::D);
    VehicleContext->MapKey(SteerAction, EKeys::A).Modifiers.Add(Negate(VehicleContext, true, true, true));
    VehicleContext->MapKey(RiseAction, EKeys::SpaceBar);
    VehicleContext->MapKey(RiseAction, EKeys::LeftControl).Modifiers.Add(Negate(VehicleContext, true, true, true));
    MapMouseLook(VehicleContext, LookAction);
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

FString UHavocPlayInput::KeyName(const FKey& Key)
{
    if (Key == EKeys::Mouse2D) { return TEXT("Mouse"); }
    if (Key == EKeys::Gamepad_Special_Right) { return TEXT("Start"); }
    if (Key == EKeys::Gamepad_FaceButton_Left) { return TEXT("Gamepad X"); }
    if (Key == EKeys::LeftControl) { return TEXT("Left Ctrl"); }
    if (Key == EKeys::LeftShift) { return TEXT("Left Shift"); }
    if (Key == EKeys::SpaceBar) { return TEXT("Space"); }
    if (Key == EKeys::Escape) { return TEXT("Esc"); }
    return Key.GetDisplayName().ToString();
}

namespace
{
    /** The keys mapped to an action, in mapping order, joined with " / ". Empty if nothing is mapped. */
    TArray<FString> MappedKeys(const UInputMappingContext* Context, const UInputAction* Action)
    {
        TArray<FString> Out;
        if (Context && Action)
        {
            for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
            {
                if (Mapping.Action == Action)
                {
                    Out.AddUnique(UHavocPlayInput::KeyName(Mapping.Key));
                }
            }
        }
        return Out;
    }

    FString Joined(const UInputMappingContext* Context, const UInputAction* Action)
    {
        const TArray<FString> Keys = MappedKeys(Context, Action);
        return Keys.Num() ? FString::Join(Keys, TEXT(" / ")) : FString(TEXT("(unbound)"));
    }

    /** A 2D movement action mapped by MapWasd (right, left, forward, back) shown as forward / left / back / right. */
    FString FourWay(const UInputMappingContext* Context, const UInputAction* Action)
    {
        const TArray<FString> K = MappedKeys(Context, Action);
        return K.Num() == 4 ? FString::Printf(TEXT("%s / %s / %s / %s"), *K[2], *K[1], *K[3], *K[0]) : Joined(Context, Action);
    }
}

TArray<FHavocControlRow> UHavocPlayInput::BuildControlRows() const
{
    TArray<FHavocControlRow> Rows;
    const auto Add = [&Rows](const TCHAR* Section, const TCHAR* Action, const FString& Keys)
    {
        Rows.Add({ Section, Action, Keys });
    };

    const TCHAR* Foot = TEXT("On foot (Marine)");
    Add(Foot, TEXT("Move forward / left / back / right"), FourWay(FootContext, MoveAction));
    Add(Foot, TEXT("Look"), Joined(FootContext, LookAction));
    Add(Foot, TEXT("Jump"), Joined(FootContext, JumpAction));
    Add(Foot, TEXT("Sprint (hold)"), Joined(FootContext, SprintAction));
    Add(Foot, TEXT("Crouch (toggle)"), Joined(FootContext, CrouchAction));
    Add(Foot, TEXT("Aim"), Joined(FootContext, AimAction));
    Add(Foot, TEXT("Fire (hold for automatic)"), Joined(FootContext, FireAction));
    Add(Foot, TEXT("Reload"), Joined(FootContext, ReloadAction));
    Add(Foot, TEXT("Interact: call a resupply drop"), Joined(MetaContext, InteractAction));
    Add(Foot, TEXT("Get into the nearest vehicle / get out"), Joined(MetaContext, VehicleAction));
    Add(Foot, TEXT("Choose north / west landing beach"), FString::Printf(TEXT("%s / %s"), *Joined(MetaContext, ChooseNorthAction), *Joined(MetaContext, ChooseWestAction)));

    const TCHAR* Veh = TEXT("Vehicles (boat, ground vehicle, helicopter, recon drone)");
    Add(Veh, TEXT("Throttle forward / reverse"), Joined(VehicleContext, ThrottleAction));
    Add(Veh, TEXT("Steer right / left (or yaw)"), Joined(VehicleContext, SteerAction));
    Add(Veh, TEXT("Climb / descend (flying vehicles)"), Joined(VehicleContext, RiseAction));
    Add(Veh, TEXT("Camera"), Joined(VehicleContext, LookAction));
    Add(Veh, TEXT("Get out"), Joined(MetaContext, VehicleAction));

    const TCHAR* Raven = TEXT("Raven drone");
    Add(Raven, TEXT("Launch or take control of the Raven"), Joined(MetaContext, DeployRavenAction));
    Add(Raven, TEXT("Fly forward / left / back / right"), FourWay(DroneContext, MoveAction));
    Add(Raven, TEXT("Climb / descend"), Joined(DroneContext, RiseAction));
    Add(Raven, TEXT("Turn and tilt the camera"), Joined(DroneContext, LookAction));
    Add(Raven, TEXT("Mark a revealed lane hazard"), Joined(MetaContext, InteractAction));
    Add(Raven, TEXT("Return to the Marine"), Joined(MetaContext, ReturnAction));

    const TCHAR* Falcon = TEXT("Falcon helicopter");
    Add(Falcon, TEXT("Call Falcon support"), Joined(MetaContext, CallFalconAction));
    Add(Falcon, TEXT("Climb / descend (collective)"), Joined(FalconContext, CollectiveAction));
    Add(Falcon, TEXT("Tilt forward / left / back / right (cyclic)"), FourWay(FalconContext, CyclicAction));
    Add(Falcon, TEXT("Yaw right / left"), Joined(FalconContext, YawAction));
    Add(Falcon, TEXT("Return to the Marine (the Falcon keeps covering)"), Joined(MetaContext, ReturnAction));

    const TCHAR* Menu = TEXT("Menus and game");
    Add(Menu, TEXT("Pause menu"), Joined(MetaContext, PauseAction));
    Add(Menu, TEXT("Dismiss a first-time hint"), Joined(MetaContext, DismissHintAction));
    Add(Menu, TEXT("Menus: move, choose, back"), TEXT("Mouse click, or Up / Down and Enter / Esc, or gamepad D-pad / stick, A / B"));
    return Rows;
}

FString UHavocPlayInput::HintFor(EHavocHintKind Kind) const
{
    const FString Return = Joined(MetaContext, ReturnAction);
    switch (Kind)
    {
    case EHavocHintKind::Drone:
        return FString::Printf(TEXT("RAVEN DRONE: %s fly, Space / Left Ctrl climb and descend, mouse turns. %s marks a revealed hazard. %s returns to the Marine."),
            *FourWay(DroneContext, MoveAction), *Joined(MetaContext, InteractAction), *Return);
    case EHavocHintKind::Vehicle:
        return FString::Printf(TEXT("VEHICLE: %s throttle, %s steer, %s climb and descend (flying vehicles only), mouse moves the camera. %s gets out. Full list: Pause > How to play."),
            *Joined(VehicleContext, ThrottleAction), *Joined(VehicleContext, SteerAction), *Joined(VehicleContext, RiseAction), *Joined(MetaContext, VehicleAction));
    default:
        return FString::Printf(TEXT("FALCON HELICOPTER: %s climb and descend, %s tilt, %s yaw. %s returns to the Marine. Full list: Pause > How to play."),
            *Joined(FalconContext, CollectiveAction), *FourWay(FalconContext, CyclicAction), *Joined(FalconContext, YawAction), *Return);
    }
}
