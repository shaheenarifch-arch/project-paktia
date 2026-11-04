#include "World/HavocSectorVolume.h"
#include "World/HavocWorldLog.h"
#include "Core/HavocEventSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

AHavocSectorVolume::AHavocSectorVolume()
{
}

void AHavocSectorVolume::BeginPlay()
{
    Super::BeginPlay();
    OnActorBeginOverlap.AddDynamic(this, &AHavocSectorVolume::HandleBeginOverlap);
    OnActorEndOverlap.AddDynamic(this, &AHavocSectorVolume::HandleEndOverlap);
}

void AHavocSectorVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    OnActorBeginOverlap.RemoveDynamic(this, &AHavocSectorVolume::HandleBeginOverlap);
    OnActorEndOverlap.RemoveDynamic(this, &AHavocSectorVolume::HandleEndOverlap);
    Super::EndPlay(EndPlayReason);
}

TArray<APawn*> AHavocSectorVolume::GetOccupants() const
{
    TArray<APawn*> Result;
    TArray<AActor*> Overlapping;
    GetOverlappingActors(Overlapping, APawn::StaticClass());
    for (AActor* Actor : Overlapping)
    {
        if (APawn* Pawn = Cast<APawn>(Actor))
        {
            Result.Add(Pawn);
        }
    }
    return Result;
}

void AHavocSectorVolume::HandleBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
    PublishSectorEvent(TEXT("sector:enter"), OtherActor);
}

void AHavocSectorVolume::HandleEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
    PublishSectorEvent(TEXT("sector:exit"), OtherActor);
}

void AHavocSectorVolume::PublishSectorEvent(const TCHAR* EventName, AActor* OtherActor) const
{
    const APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || SectorId.IsNone())
    {
        return;
    }

    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    UHavocEventSubsystem* Events = GameInstance ? GameInstance->GetSubsystem<UHavocEventSubsystem>() : nullptr;
    if (!Events)
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Sector %s: no UHavocEventSubsystem, %s not published"), *SectorId.ToString(), EventName);
        return;
    }

    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("sectorId"), SectorId.ToString());
    Json->SetStringField(TEXT("actor"), Pawn->GetName());
    Json->SetBoolField(TEXT("isPlayer"), Pawn->IsPlayerControlled());

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Json, Writer);

    Events->Publish(FName(EventName), Payload);
}
