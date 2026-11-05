#include "Characters/HavocCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"

AHavocCharacter::AHavocCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);

    MaximumHealth = 100.0f;
    Health = MaximumHealth;
}

void AHavocCharacter::BeginPlay()
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

void AHavocCharacter::SetupPlayerInputComponent(
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
            &AHavocCharacter::Move
        );
    }

    if (LookAction)
    {
        EnhancedInput->BindAction(
            LookAction,
            ETriggerEvent::Triggered,
            this,
            &AHavocCharacter::Look
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

void AHavocCharacter::Move(
    const FInputActionValue& Value
)
{
    const FVector2D Movement =
        Value.Get<FVector2D>();

    AddMovementInput(GetActorForwardVector(), Movement.Y);
    AddMovementInput(GetActorRightVector(), Movement.X);
}

void AHavocCharacter::Look(
    const FInputActionValue& Value
)
{
    const FVector2D LookInput =
        Value.Get<FVector2D>();

    AddControllerYawInput(LookInput.X);
    AddControllerPitchInput(LookInput.Y);
}

void AHavocCharacter::ApplyAuthoritativeDamage(
    float DamageAmount
)
{
    if (!HasAuthority() || DamageAmount <= 0.0f || bIsDead)
    {
        return;
    }

    Health = FMath::Clamp(
        Health - DamageAmount,
        0.0f,
        MaximumHealth
    );

    OnRep_Health();

    if (Health <= 0.0f)
    {
        bIsDead = true;
        HandleDeath();
    }
}

void AHavocCharacter::SetMaximumHealthAndFill(float NewMaximum)
{
    MaximumHealth = FMath::Max(1.0f, NewMaximum);
    Health = MaximumHealth;
}

void AHavocCharacter::HandleDeath()
{
}

void AHavocCharacter::OnRep_Health()
{
    // Presentation only.
    // The server remains authoritative for health and damage.
}

void AHavocCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AHavocCharacter, Health);
}
