#pragma once

// Small JSON string helpers shared by the mission framework and Episode 04
// (checkpoint payloads, system snapshots, event payloads).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace HavocMissionJson
{
    inline FString ToString(const TSharedRef<FJsonObject>& Object)
    {
        FString Out;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
        FJsonSerializer::Serialize(Object, Writer);
        return Out;
    }

    /** Returns an empty object if Text is not a JSON object. */
    inline TSharedRef<FJsonObject> FromString(const FString& Text)
    {
        TSharedPtr<FJsonObject> Object;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
        {
            Object = MakeShared<FJsonObject>();
        }
        return Object.ToSharedRef();
    }

    inline TArray<TSharedPtr<FJsonValue>> StringArray(const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (const FString& Value : Values)
        {
            Out.Add(MakeShared<FJsonValueString>(Value));
        }
        return Out;
    }

    inline TArray<FString> ReadStringArray(const FJsonObject& Object, const TCHAR* Key)
    {
        TArray<FString> Out;
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (Object.TryGetArrayField(Key, Values) && Values)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                FString Text;
                if (Value.IsValid() && Value->TryGetString(Text))
                {
                    Out.Add(Text);
                }
            }
        }
        return Out;
    }

    /** Array of objects under Key; invalid entries are skipped. */
    inline TArray<TSharedPtr<FJsonObject>> ReadObjectArray(const FJsonObject& Object, const TCHAR* Key)
    {
        TArray<TSharedPtr<FJsonObject>> Out;
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (Object.TryGetArrayField(Key, Values) && Values)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject>* Child = nullptr;
                if (Value.IsValid() && Value->TryGetObject(Child) && Child && Child->IsValid())
                {
                    Out.Add(*Child);
                }
            }
        }
        return Out;
    }
}
