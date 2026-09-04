#include "TOGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"

UTOGameInstance::UTOGameInstance()
{
}

void UTOGameInstance::Init()
{
    Super::Init();

    if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
    {
        SessionInterface = Subsystem->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UTOGameInstance::OnCreateSessionComplete);
            SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UTOGameInstance::OnJoinSessionComplete);
        }
    }
}

void UTOGameInstance::CreateMySession()
{
    if (!SessionInterface.IsValid()) return;

    auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
    if (ExistingSession != nullptr)
    {
        SessionInterface->DestroySession(NAME_GameSession);
    }

    FOnlineSessionSettings SessionSettings;
    SessionSettings.bIsLANMatch = true;
    SessionSettings.NumPublicConnections = 4;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bUsesPresence = true;
    SessionSettings.Set(FName("MATCH_TYPE"), FString("FreeForAll"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    SessionInterface->CreateSession(0, NAME_GameSession, SessionSettings);
}

void UTOGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (bWasSuccessful)
    {
        UE_LOG(LogTemp, Log, TEXT("Session Created Successfully: %s"), *SessionName.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to Create Session!"));
    }
}

void UTOGameInstance::JoinMySession()
{
    UE_LOG(LogTemp, Log, TEXT("Join Session Called"));
}

void UTOGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    if (Result == EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogTemp, Log, TEXT("Successfully Joined Session: %s"), *SessionName.ToString());

        FString Address;
        if (SessionInterface->GetResolvedConnectString(SessionName, Address))
        {
            if (APlayerController* PC = GetFirstLocalPlayerController())
            {
                PC->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to Join Session!"));
    }
}