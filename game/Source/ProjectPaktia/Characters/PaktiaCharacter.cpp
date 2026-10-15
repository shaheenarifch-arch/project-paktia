#include "Characters/PaktiaCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"

APaktiaCharacter::APaktiaCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);

    MaximumHealth = 100.0f;
    Health = MaximumHealth;
}

void APaktiaCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PlayerController =
            Cast<APlayerController>(Controller))
    {
        if (ULocalPlayer* LocalPlayer =
                PlayerController->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                    LocalPlayer->GetSubsystem<
                        UEnhancedInputLocalPlayerSubsystem
                    >())
            {
                if (DefaultInputContext)
                {
                    Subsystem->AddMappingContext(
                        DefaultInputContext,
                        0
                    );
                }
            }
        }
    }
}

void APaktiaCharacter::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent
)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput =
        CastChecked<UEnhancedInputComponent>(
            PlayerInputComponent
        );

    if (MoveAction)
    {
        EnhancedInput->BindAction(
            MoveAction,
            ETriggerEvent::Triggered,
            this,
            &APaktiaCharacter::Move
        );
    }

    if (LookAction)
    {
        EnhancedInput->BindAction(
            LookAction,
            ETriggerEvent::Triggered,
            this,
            &APaktiaCharacter::Look
        );
    }

    if (JumpAction)
    {
        EnhancedInput->BindAction(
            JumpAction,
            ETriggerEvent::Started,
            this,
            &ACharacter::Jump
        );

        EnhancedInput->BindAction(
            JumpAction,
            ETriggerEvent::Completed,
            this,
            &ACharacter::StopJumping
        );
    }
}

void APaktiaCharacter::Move(
    const FInputActionValue& Value
)
{
    const FVector2D Movement =
        Value.Get<FVector2D>();

    AddMovementInput(GetActorForwardVector(), Movement.Y);
    AddMovementInput(GetActorRightVector(), Movement.X);
}

void APaktiaCharacter::Look(
    const FInputActionValue& Value
)
{
    const FVector2D LookInput =
        Value.Get<FVector2D>();

    AddControllerYawInput(LookInput.X);
    AddControllerPitchInput(LookInput.Y);
}

void APaktiaCharacter::ApplyAuthoritativeDamage(
    float DamageAmount
)
{
    if (!HasAuthority() || DamageAmount <= 0.0f)
    {
        return;
    }

    Health = FMath::Clamp(
        Health - DamageAmount,
        0.0f,
        MaximumHealth
    );

    OnRep_Health();
}

void APaktiaCharacter::OnRep_Health()
{
    // Presentation only.
    // The server remains authoritative for health and damage.
}

void APaktiaCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(APaktiaCharacter, Health);
}
