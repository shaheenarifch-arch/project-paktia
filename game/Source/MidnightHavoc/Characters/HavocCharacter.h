#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HavocCharacter.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class MIDNIGHTHAVOC_API AHavocCharacter
    : public ACharacter
{
    GENERATED_BODY()

public:
    AHavocCharacter();

    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent
    ) override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UPROPERTY(
        BlueprintReadOnly,
        ReplicatedUsing = OnRep_Health,
        Category = "Operation Midnight Havoc|Combat"
    )
    float Health;

    UPROPERTY(
        BlueprintReadOnly,
        EditDefaultsOnly,
        Category = "Operation Midnight Havoc|Combat"
    )
    float MaximumHealth;

    UFUNCTION(BlueprintCallable)
    void ApplyAuthoritativeDamage(float DamageAmount);

    UFUNCTION(BlueprintPure, Category = "Operation Midnight Havoc|Combat")
    bool IsDead() const { return bIsDead || Health <= 0.0f; }

    /** Server: sets health (clamped to 0..MaximumHealth), e.g. when a unit definition changes it. */
    void SetMaximumHealthAndFill(float NewMaximum);

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultInputContext;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> JumpAction;

    virtual void Move(const struct FInputActionValue& Value);
    virtual void Look(const struct FInputActionValue& Value);

    /** Server: called once when health reaches zero. Base class only records it. */
    virtual void HandleDeath();

    bool bIsDead = false;

    UFUNCTION()
    void OnRep_Health();
};
