#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PaktiaCharacter.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class PROJECTPAKTIA_API APaktiaCharacter
    : public ACharacter
{
    GENERATED_BODY()

public:
    APaktiaCharacter();

    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent
    ) override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UPROPERTY(
        BlueprintReadOnly,
        ReplicatedUsing = OnRep_Health,
        Category = "Project Paktia|Combat"
    )
    float Health;

    UPROPERTY(
        BlueprintReadOnly,
        EditDefaultsOnly,
        Category = "Project Paktia|Combat"
    )
    float MaximumHealth;

    UFUNCTION(BlueprintCallable)
    void ApplyAuthoritativeDamage(float DamageAmount);

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

    void Move(const struct FInputActionValue& Value);
    void Look(const struct FInputActionValue& Value);

    UFUNCTION()
    void OnRep_Health();
};
