#include "Core/PaktiaEventSubsystem.h"

void UPaktiaEventSubsystem::Subscribe(FName EventName, UObject* Object, FName FunctionName)
{
    if (!Object || EventName.IsNone() || FunctionName.IsNone())
    {
        return;
    }

    FScriptDelegate Delegate;
    Delegate.BindUFunction(Object, FunctionName);
    Handlers.FindOrAdd(EventName).AddUnique(Delegate);
}

void UPaktiaEventSubsystem::Unsubscribe(FName EventName, UObject* Object, FName FunctionName)
{
    if (!Object)
    {
        return;
    }

    if (FPaktiaEventDelegate* Delegate = Handlers.Find(EventName))
    {
        Delegate->Remove(Object, FunctionName);
    }
}

void UPaktiaEventSubsystem::Publish(FName EventName, const FString& Payload)
{
    if (FPaktiaEventDelegate* Delegate = Handlers.Find(EventName))
    {
        Delegate->Broadcast(EventName, Payload);
    }
}
