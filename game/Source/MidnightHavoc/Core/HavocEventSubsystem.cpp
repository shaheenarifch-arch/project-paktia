#include "Core/HavocEventSubsystem.h"

void UHavocEventSubsystem::Subscribe(FName EventName, UObject* Object, FName FunctionName)
{
    if (!Object || EventName.IsNone() || FunctionName.IsNone())
    {
        return;
    }

    FScriptDelegate Delegate;
    Delegate.BindUFunction(Object, FunctionName);
    Handlers.FindOrAdd(EventName).AddUnique(Delegate);
}

void UHavocEventSubsystem::Unsubscribe(FName EventName, UObject* Object, FName FunctionName)
{
    if (!Object)
    {
        return;
    }

    if (FHavocEventDelegate* Delegate = Handlers.Find(EventName))
    {
        Delegate->Remove(Object, FunctionName);
    }
}

void UHavocEventSubsystem::Publish(FName EventName, const FString& Payload)
{
    if (FHavocEventDelegate* Delegate = Handlers.Find(EventName))
    {
        Delegate->Broadcast(EventName, Payload);
    }
}
