#pragma once

// Ports: src/core/EventBus.ts
// Subscribe/publish by event name. Handlers are UFUNCTIONs taking
// (FName EventName, const FString& Payload).

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HavocEventSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FHavocEventDelegate,
    FName, EventName,
    const FString&, Payload
);

UCLASS()
class MIDNIGHTHAVOC_API UHavocEventSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Bind Object::FunctionName to EventName. Duplicate bindings are ignored. */
    UFUNCTION(BlueprintCallable, Category = "Operation Midnight Havoc|Events")
    void Subscribe(FName EventName, UObject* Object, FName FunctionName);

    UFUNCTION(BlueprintCallable, Category = "Operation Midnight Havoc|Events")
    void Unsubscribe(FName EventName, UObject* Object, FName FunctionName);

    UFUNCTION(BlueprintCallable, Category = "Operation Midnight Havoc|Events")
    void Publish(FName EventName, const FString& Payload);

private:
    TMap<FName, FHavocEventDelegate> Handlers;
};
