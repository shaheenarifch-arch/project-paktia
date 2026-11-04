#pragma once

// Ports: src/core/EventBus.ts
// Subscribe/publish by event name. Handlers are UFUNCTIONs taking
// (FName EventName, const FString& Payload).

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PaktiaEventSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FPaktiaEventDelegate,
    FName, EventName,
    const FString&, Payload
);

UCLASS()
class PROJECTPAKTIA_API UPaktiaEventSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Bind Object::FunctionName to EventName. Duplicate bindings are ignored. */
    UFUNCTION(BlueprintCallable, Category = "Project Paktia|Events")
    void Subscribe(FName EventName, UObject* Object, FName FunctionName);

    UFUNCTION(BlueprintCallable, Category = "Project Paktia|Events")
    void Unsubscribe(FName EventName, UObject* Object, FName FunctionName);

    UFUNCTION(BlueprintCallable, Category = "Project Paktia|Events")
    void Publish(FName EventName, const FString& Payload);

private:
    TMap<FName, FPaktiaEventDelegate> Handlers;
};
