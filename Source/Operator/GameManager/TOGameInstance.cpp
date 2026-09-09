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

void UTOGameInstance::CreateMySession()
{
    if (!SessionInterface.IsValid())
    {
        return;
    }

    auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);

    if (ExistingSession != nullptr)
    {
        SessionInterface->DestroySession(NAME_GameSession);
    }

    FOnlineSessionSettings SessionSettings;

    // Steam 인터넷 세션
    SessionSettings.bIsLANMatch = false;

    SessionSettings.NumPublicConnections = 4;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bUsesPresence = true;
    SessionSettings.bUseLobbiesIfAvailable = true;

    SessionSettings.Set(
        FName(TEXT("MATCH_TYPE")),
        FString(TEXT("FreeForAll")),
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
    );

    UE_LOG(LogTemp, Log, TEXT("Creating Steam Session..."));

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
            TEXT("Failed to Create Session!")
        );
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

void UTOGameInstance::CreateRoom()
{
    CreateMySession();
}

void UTOGameInstance::JoinRoom()
{
    JoinMySession();
}

void UTOGameInstance::FindRooms()
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

    SessionSearch = MakeShareable(
        new FOnlineSessionSearch()
    );

    SessionSearch->MaxSearchResults = 100;

    // Steam 인터넷 검색
    SessionSearch->bIsLanQuery = false;

    // Steam Presence 세션 검색
    SessionSearch->QuerySettings.Set(
        FName(TEXT("SEARCH_PRESENCE")),
        true,
        EOnlineComparisonOp::Equals
    );

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Searching for Steam Sessions...")
    );

    SessionInterface->FindSessions(
        0,
        SessionSearch.ToSharedRef()
    );
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

    // 블루프린트에 검색 완료 알림
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