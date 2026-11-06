#include "Game/HavocSmokeExitSubsystem.h"

#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogHavocSmoke, Log, All);

namespace
{
    float SmokeSecondsFromCommandLine()
    {
        float Seconds = 0.f;
        FParse::Value(FCommandLine::Get(), TEXT("HavocSmokeSeconds="), Seconds);
        return Seconds;
    }
}

bool UHavocSmokeExitSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return SmokeSecondsFromCommandLine() > 0.f;
}

void UHavocSmokeExitSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const float Seconds = SmokeSecondsFromCommandLine();
    UE_LOG(LogHavocSmoke, Display, TEXT("Smoke run: the game will exit by itself in %.0f s."), Seconds);
    ExitHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([Seconds](float)
        {
            UE_LOG(LogHavocSmoke, Display, TEXT("Smoke run: %.0f s reached, exiting."), Seconds);
            FPlatformMisc::RequestExit(false, TEXT("HavocSmokeSeconds"));
            return false; // run once
        }),
        Seconds);
}

void UHavocSmokeExitSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(ExitHandle);
    Super::Deinitialize();
}
