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
            SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(
                this,
                &UTOGameInstance::OnCreateSessionComplete
            );

            SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(
                this,
                &UTOGameInstance::OnJoinSessionComplete
            );

            SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(
                this,
                &UTOGameInstance::OnFindSessionsComplete
            );
        }
    }
}

void UTOGameInstance::CreateMySession(const FString& Password)
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] SessionInterface is invalid, falling back to direct ServerTravel."));
        if (UWorld* World = GetWorld())
        {
            World->ServerTravel(TEXT("/Game/Maps/DetectiveOffice/Levels/L_DetectiveOffice?listen"));
        }
        return;
    }

    auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
    if (ExistingSession != nullptr)
    {
        SessionInterface->DestroySession(NAME_GameSession);
    }

    FOnlineSessionSettings SessionSettings;

    SessionSettings.bIsLANMatch = true;
    SessionSettings.NumPublicConnections = 6;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bUsesPresence = false;
    SessionSettings.bUseLobbiesIfAvailable = false;

    // 비밀번호 존재 여부에 따른 퍼블릭/프라이빗 분기
    if (!Password.IsEmpty())
    {
        SessionSettings.Set(
            FName(TEXT("ROOM_PASSWORD")),
            Password,
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

        SessionSettings.Set(
            FName(TEXT("MATCH_TYPE")),
            FString(TEXT("Private")),
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

        UE_LOG(LogTemp, Log, TEXT("Creating Private Steam Session with Password..."));
    }
    else
    {
        SessionSettings.Set(
            FName(TEXT("MATCH_TYPE")),
            FString(TEXT("Public")),
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

        UE_LOG(LogTemp, Log, TEXT("Creating Public Steam Session..."));
    }

    SessionInterface->CreateSession(
        0,
        NAME_GameSession,
        SessionSettings
    );
}

void UTOGameInstance::OnCreateSessionComplete(
    FName SessionName,
    bool bWasSuccessful
)
{
    if (bWasSuccessful)
    {
        UE_LOG(
            LogTemp,
            Log,
            TEXT("Session Created Successfully: %s"),
            *SessionName.ToString()
        );

        UWorld* World = GetWorld();

        if (World)
        {
            World->ServerTravel(
                TEXT("/Game/Maps/DetectiveOffice/Levels/L_DetectiveOffice?listen")
            );

            UE_LOG(
                LogTemp,
                Log,
                TEXT("Open Level Successfully: %s"),
                *SessionName.ToString()
            );
        }
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Failed to Create Session! Falling back to ServerTravel anyway.")
        );

        if (UWorld* World = GetWorld())
        {
            World->ServerTravel(
                TEXT("/Game/Maps/DetectiveOffice/Levels/L_DetectiveOffice?listen")
            );
        }
    }
}

void UTOGameInstance::JoinMySession()
{
    UE_LOG(
        LogTemp,
        Log,
        TEXT("Join Session Called")
    );
}

void UTOGameInstance::OnJoinSessionComplete(
    FName SessionName,
    EOnJoinSessionCompleteResult::Type Result
)
{
    if (Result == EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(
            LogTemp,
            Log,
            TEXT("Successfully Joined Session: %s"),
            *SessionName.ToString()
        );

        FString Address;

        if (SessionInterface->GetResolvedConnectString(
            SessionName,
            Address
        ))
        {
            if (APlayerController* PC = GetFirstLocalPlayerController())
            {
                PC->ClientTravel(
                    Address,
                    ETravelType::TRAVEL_Absolute
                );
            }
        }
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Failed to Join Session!")
        );
    }
}

void UTOGameInstance::CreateRoom(const FString& Password)
{
    CreateMySession(Password);
}

void UTOGameInstance::JoinRoom()
{
    JoinMySession();
}

void UTOGameInstance::FindRooms()
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("SessionInterface is invalid."));
        return;
    }

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->MaxSearchResults = 100;
    SessionSearch->bIsLanQuery = true;

    SessionSearch->QuerySettings.Set(
        FName(TEXT("SEARCH_PRESENCE")),
        true,
        EOnlineComparisonOp::Equals
    );

    UE_LOG(LogTemp, Log, TEXT("Searching for Public Steam Sessions..."));

    SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
}

void UTOGameInstance::FindPrivateRooms(const FString& SearchPassword)
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("SessionInterface is invalid."));
        return;
    }

    TargetPassword = SearchPassword;

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->MaxSearchResults = 100;
    SessionSearch->bIsLanQuery = true;

    SessionSearch->QuerySettings.Set(
        FName(TEXT("SEARCH_PRESENCE")),
        true,
        EOnlineComparisonOp::Equals
    );

    UE_LOG(LogTemp, Log, TEXT("Searching for Private Steam Sessions with Password..."));

    SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
}

void UTOGameInstance::OnFindSessionsComplete(
    bool bWasSuccessful
)
{
    if (!bWasSuccessful || !SessionSearch.IsValid())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Failed to find sessions.")
        );

        return;
    }

    const TArray<FOnlineSessionSearchResult>& Results =
        SessionSearch->SearchResults;

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Found %d Steam sessions."),
        Results.Num()
    );

    for (int32 i = 0; i < Results.Num(); ++i)
    {
        const FOnlineSessionSearchResult& Result =
            Results[i];

        UE_LOG(
            LogTemp,
            Log,
            TEXT("Session [%d] - %s"),
            i,
            *Result.GetSessionIdStr()
        );
    }

    OnRoomsFound.Broadcast();
}

void UTOGameInstance::JoinFoundSession(int32 Index)
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("SessionInterface is invalid.")
        );

        return;
    }

    if (!SessionSearch.IsValid())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("No session search results available.")
        );

        return;
    }

    if (Index < 0 || Index >= SessionSearch->SearchResults.Num())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Invalid session index: %d"),
            Index
        );

        return;
    }

    const FOnlineSessionSearchResult& Result =
        SessionSearch->SearchResults[Index];

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Joining session at index: %d"),
        Index
    );

    SessionInterface->JoinSession(
        0,
        NAME_GameSession,
        Result
    );
}

int32 UTOGameInstance::GetFoundRoomCount() const
{
    if (!SessionSearch.IsValid())
    {
        return 0;
    }

    return SessionSearch->SearchResults.Num();
}

FString UTOGameInstance::GetFoundRoomName(int32 Index) const
{
    if (!SessionSearch.IsValid())
    {
        return TEXT("");
    }

    if (!SessionSearch->SearchResults.IsValidIndex(Index))
    {
        return TEXT("");
    }

    const FOnlineSessionSearchResult& Result =
        SessionSearch->SearchResults[Index];

    FString RoomName;

    Result.Session.SessionSettings.Get(
        FName(TEXT("ROOM_NAME")),
        RoomName
    );

    return RoomName;
}

int32 UTOGameInstance::GetFoundRoomPlayerCount(int32 Index) const
{
    if (!SessionSearch.IsValid())
    {
        return 0;
    }

    if (!SessionSearch->SearchResults.IsValidIndex(Index))
    {
        return 0;
    }

    const FOnlineSessionSearchResult& Result =
        SessionSearch->SearchResults[Index];

    const int32 CurrentPlayers =
        Result.Session.SessionSettings.NumPublicConnections -
        Result.Session.NumOpenPublicConnections;

    return CurrentPlayers;
}

void UTOGameInstance::JoinServerByIP(const FString& IPAddress)
{
    FString CleanAddress = IPAddress.TrimStartAndEnd();
    if (CleanAddress.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] JoinServerByIP: IP Address is empty."));
        return;
    }

    if (!CleanAddress.Contains(TEXT(":")))
    {
        CleanAddress += TEXT(":7777");
    }

    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] JoinServerByIP: Connecting to %s"), *CleanAddress);

    if (APlayerController* PC = GetFirstLocalPlayerController())
    {
        PC->ClientTravel(CleanAddress, ETravelType::TRAVEL_Absolute);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] JoinServerByIP: Failed to get FirstLocalPlayerController."));
    }
}