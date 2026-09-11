#include "TOGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "UObject/UObjectIterator.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"

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

    // UI 버튼 자동 바인딩을 위한 주기적 틱 등록 (0.25초 간격)
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UTOGameInstance::TickWidgetBindings),
        0.25f
    );
}

void UTOGameInstance::Shutdown()
{
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }

    Super::Shutdown();
}

UUserWidget* UTOGameInstance::FindActiveWidget(const FString& ClassSubstr) const
{
    UWorld* CurrentWorld = GetWorld();
    if (!CurrentWorld) return nullptr;

    for (TObjectIterator<UUserWidget> It; It; ++It)
    {
        UUserWidget* Widget = *It;
        if (IsValid(Widget) && Widget->GetWorld() == CurrentWorld)
        {
            if (Widget->GetClass()->GetName().Contains(ClassSubstr))
            {
                return Widget;
            }
        }
    }
    return nullptr;
}

bool UTOGameInstance::TickWidgetBindings(float DeltaTime)
{
    if (UUserWidget* JoinMenu = FindActiveWidget(TEXT("WBP_JoinRoomMenu")))
    {
        if (BoundJoinMenu.Get() != JoinMenu)
        {
            BoundJoinMenu = JoinMenu;

            // 1. 비공개 방 참가 버튼("Btn_CreateRoom" 또는 "Btn_Join") 자동 바인딩
            UButton* TargetBtn = Cast<UButton>(JoinMenu->GetWidgetFromName(FName(TEXT("Btn_CreateRoom"))));
            if (!TargetBtn)
            {
                TargetBtn = Cast<UButton>(JoinMenu->GetWidgetFromName(FName(TEXT("Btn_Join"))));
            }

            if (TargetBtn)
            {
                TargetBtn->OnClicked.RemoveDynamic(this, &UTOGameInstance::OnPrivateJoinButtonClicked);
                TargetBtn->OnClicked.AddDynamic(this, &UTOGameInstance::OnPrivateJoinButtonClicked);
                UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Bound private join button on WBP_JoinRoomMenu."));
            }

            // 2. 공개 방 검색(새로고침) 버튼("Btn_RefreshServer") 자동 바인딩
            if (UButton* RefreshBtn = Cast<UButton>(JoinMenu->GetWidgetFromName(FName(TEXT("Btn_RefreshServer")))))
            {
                RefreshBtn->OnClicked.RemoveDynamic(this, &UTOGameInstance::OnRefreshServerClicked);
                RefreshBtn->OnClicked.AddDynamic(this, &UTOGameInstance::OnRefreshServerClicked);
                UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Bound Btn_RefreshServer on WBP_JoinRoomMenu."));
            }

            // 3. 공개 방 찾기 페이지 이동 버튼("Btn_PublicSearch") 자동 바인딩 (클릭 시 자동 검색 시작)
            if (UButton* PublicSearchBtn = Cast<UButton>(JoinMenu->GetWidgetFromName(FName(TEXT("Btn_PublicSearch")))))
            {
                PublicSearchBtn->OnClicked.RemoveDynamic(this, &UTOGameInstance::OnPublicSearchButtonClicked);
                PublicSearchBtn->OnClicked.AddDynamic(this, &UTOGameInstance::OnPublicSearchButtonClicked);
                UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Bound Btn_PublicSearch on WBP_JoinRoomMenu."));
            }
        }
    }
    return true;
}

void UTOGameInstance::OnRefreshServerClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] OnRefreshServerClicked triggered!"));
    FindRooms();
}

void UTOGameInstance::OnPublicSearchButtonClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] OnPublicSearchButtonClicked triggered!"));
    FindRooms();
}

FString UTOGameInstance::GetBestHostIP() const
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (SocketSubsystem)
    {
        TArray<TSharedPtr<FInternetAddr>> LocalAddrs;
        if (SocketSubsystem->GetLocalAdapterAddresses(LocalAddrs))
        {
            // 1순위: 하마치 가상 어댑터 IP (25.x.x.x)
            for (const auto& Addr : LocalAddrs)
            {
                if (Addr.IsValid())
                {
                    FString IP = Addr->ToString(false);
                    if (IP.StartsWith(TEXT("25.")))
                    {
                        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] GetBestHostIP: Found Hamachi IP: %s"), *IP);
                        return IP;
                    }
                }
            }

            // 2순위: 일반 사설/공인 네트워크 어댑터 IP (127.0.0.1, 169.254.x.x 제외)
            for (const auto& Addr : LocalAddrs)
            {
                if (Addr.IsValid())
                {
                    FString IP = Addr->ToString(false);
                    if (IP != TEXT("127.0.0.1") && !IP.StartsWith(TEXT("0.")) && !IP.StartsWith(TEXT("169.254.")) && !IP.IsEmpty())
                    {
                        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] GetBestHostIP: Found Local IP: %s"), *IP);
                        return IP;
                    }
                }
            }
        }
    }
    return TEXT("");
}

static bool IsIPv4Address(const FString& Str)
{
    FString IP = Str.TrimStartAndEnd();
    if (IP.Contains(TEXT(":")))
    {
        IP.Split(TEXT(":"), &IP, nullptr);
    }
    TArray<FString> Octets;
    IP.ParseIntoArray(Octets, TEXT("."));
    if (Octets.Num() != 4) return false;
    for (const FString& Octet : Octets)
    {
        if (!Octet.IsNumeric()) return false;
        int32 Val = FCString::Atoi(*Octet);
        if (Val < 0 || Val > 255) return false;
    }
    return true;
}

void UTOGameInstance::OnPrivateJoinButtonClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] OnPrivateJoinButtonClicked triggered!"));

    UUserWidget* JoinMenu = FindActiveWidget(TEXT("WBP_JoinRoomMenu"));
    if (!JoinMenu)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] WBP_JoinRoomMenu not found!"));
        return;
    }

    FString InputPlayerName;
    FString InputRoomName;
    FString InputPassword;

    if (UEditableTextBox* Box = Cast<UEditableTextBox>(JoinMenu->GetWidgetFromName(FName(TEXT("ETB_PlayerName")))))
    {
        InputPlayerName = Box->GetText().ToString().TrimStartAndEnd();
        if (!InputPlayerName.IsEmpty())
        {
            PlayerName = InputPlayerName;
        }
    }

    if (UEditableTextBox* Box = Cast<UEditableTextBox>(JoinMenu->GetWidgetFromName(FName(TEXT("ETB_RoomName")))))
    {
        InputRoomName = Box->GetText().ToString().TrimStartAndEnd();
    }

    if (UEditableTextBox* Box = Cast<UEditableTextBox>(JoinMenu->GetWidgetFromName(FName(TEXT("ETB_Password")))))
    {
        InputPassword = Box->GetText().ToString().TrimStartAndEnd();
    }

    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Private Join - Player: %s, Room: %s, Password: %s"),
        *PlayerName, *InputRoomName, *InputPassword);

    // IP 주소(하마치 가상 IP 등) 직접 입력 시 바로 접속
    if (IsIPv4Address(InputRoomName))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
                FString::Printf(TEXT("IP 직접 접속 중: %s"), *InputRoomName));
        }
        JoinServerByIP(InputRoomName);
        return;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Yellow,
            FString::Printf(TEXT("비공개 방 검색 중... [%s]"), *InputRoomName));
    }

    FindPrivateRooms(InputPassword, InputRoomName);
}

void UTOGameInstance::CreateMySession(const FString& Password, const FString& InRoomName)
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

    // 호스트 IP(하마치 25.x.x.x 또는 로컬 IP)를 세션 세팅에 광고
    FString HostIP = GetBestHostIP();
    if (!HostIP.IsEmpty())
    {
        SessionSettings.Set(
            FName(TEXT("HOST_IP")),
            HostIP,
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Advertising HOST_IP: %s"), *HostIP);
    }

    // 방 제목 결정 (인자 -> 프로퍼티 -> WBP_CreateRoomMenu 위젯 순서로 탐색)
    FString FinalRoomName = InRoomName;
    if (FinalRoomName.IsEmpty())
    {
        FinalRoomName = RoomName;
    }
    if (FinalRoomName.IsEmpty())
    {
        if (UUserWidget* CreateMenu = FindActiveWidget(TEXT("WBP_CreateRoomMenu")))
        {
            if (UEditableTextBox* Box = Cast<UEditableTextBox>(CreateMenu->GetWidgetFromName(FName(TEXT("ETB_RoomName")))))
            {
                FinalRoomName = Box->GetText().ToString().TrimStartAndEnd();
            }
            if (UEditableTextBox* Box = Cast<UEditableTextBox>(CreateMenu->GetWidgetFromName(FName(TEXT("ETB_PlayerName")))))
            {
                FString PName = Box->GetText().ToString().TrimStartAndEnd();
                if (!PName.IsEmpty())
                {
                    PlayerName = PName;
                }
            }
        }
    }
    if (FinalRoomName.IsEmpty())
    {
        FinalRoomName = PlayerName + TEXT("'s Room");
    }
    RoomName = FinalRoomName;

    SessionSettings.Set(
        FName(TEXT("ROOM_NAME")),
        FinalRoomName,
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
    );

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

        UE_LOG(LogTemp, Log, TEXT("Creating Private Session '%s' with Password..."), *FinalRoomName);
    }
    else
    {
        SessionSettings.Set(
            FName(TEXT("MATCH_TYPE")),
            FString(TEXT("Public")),
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

        UE_LOG(LogTemp, Log, TEXT("Creating Public Session '%s'..."), *FinalRoomName);
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

        if (GEngine)
        {
            FString HostIP = GetBestHostIP();
            GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green,
                FString::Printf(TEXT("방 생성 성공! [호스트 IP: %s:7777]"), HostIP.IsEmpty() ? TEXT("Local") : *HostIP));
        }

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

        FString ConnectAddress;
        FString AdvertisedIP;

        // 1. SessionSettings에 호스트가 기록한 HOST_IP가 있는지 확인 (하마치 가상 IP 등)
        if (LastJoinedSearchResult.Session.SessionSettings.Get(FName(TEXT("HOST_IP")), AdvertisedIP) && !AdvertisedIP.IsEmpty())
        {
            ConnectAddress = FString::Printf(TEXT("%s:7777"), *AdvertisedIP);
            UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Using Advertised HOST_IP for travel: %s"), *ConnectAddress);
        }
        else
        {
            // 2. 없으면 기본 GetResolvedConnectString 사용
            SessionInterface->GetResolvedConnectString(
                SessionName,
                ConnectAddress
            );
            UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Using ResolvedConnectString: %s"), *ConnectAddress);
        }

        if (!ConnectAddress.IsEmpty())
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
                    FString::Printf(TEXT("서버로 이동 중: %s"), *ConnectAddress));
            }

            if (APlayerController* PC = GetFirstLocalPlayerController())
            {
                PC->ClientTravel(
                    ConnectAddress,
                    ETravelType::TRAVEL_Absolute
                );
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] Failed to get FirstLocalPlayerController for ClientTravel!"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] ConnectAddress is empty!"));
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("서버 접속 주소를 가져오지 못했습니다."));
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

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("세션 참가 실패!"));
        }
    }
}

void UTOGameInstance::CreateRoom(const FString& Password, const FString& InRoomName)
{
    CreateMySession(Password, InRoomName);
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

    bIsSearchingPrivate = false;

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->MaxSearchResults = 100;
    SessionSearch->bIsLanQuery = true;

    UE_LOG(LogTemp, Log, TEXT("Searching for Public Sessions..."));
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Yellow, TEXT("공개 방 목록 검색 중..."));
    }

    SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
}

void UTOGameInstance::FindPrivateRooms(const FString& SearchPassword, const FString& SearchRoomName)
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("SessionInterface is invalid."));
        return;
    }

    bIsSearchingPrivate = true;
    TargetPassword = SearchPassword;
    TargetRoomName = SearchRoomName;

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->MaxSearchResults = 100;
    SessionSearch->bIsLanQuery = true;

    UE_LOG(LogTemp, Log, TEXT("Searching for Private Sessions (Target Room: '%s', Pass: '%s')..."),
        *TargetRoomName, *TargetPassword);

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

        if (bIsSearchingPrivate)
        {
            bIsSearchingPrivate = false;
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("방 검색 실패: 세션을 찾을 수 없습니다."));
            }
        }
        else
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("공개 방 검색 실패: 세션을 찾을 수 없습니다."));
            }
        }

        return;
    }

    TArray<FOnlineSessionSearchResult>& Results =
        SessionSearch->SearchResults;

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Found %d sessions total."),
        Results.Num()
    );

    for (int32 i = 0; i < Results.Num(); ++i)
    {
        const FOnlineSessionSearchResult& Result = Results[i];
        FString RName, Pass, MType, HostIP;
        Result.Session.SessionSettings.Get(FName(TEXT("ROOM_NAME")), RName);
        Result.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), Pass);
        Result.Session.SessionSettings.Get(FName(TEXT("MATCH_TYPE")), MType);
        Result.Session.SessionSettings.Get(FName(TEXT("HOST_IP")), HostIP);

        UE_LOG(
            LogTemp,
            Log,
            TEXT("Session [%d] - ID: %s, Name: '%s', MatchType: '%s', HostIP: '%s', HasPass: %s"),
            i,
            *Result.GetSessionIdStr(),
            *RName,
            *MType,
            *HostIP,
            Pass.IsEmpty() ? TEXT("No") : TEXT("Yes")
        );
    }

    // 1) 비공개 방 자동 접속 모드
    if (bIsSearchingPrivate)
    {
        bIsSearchingPrivate = false;
        int32 MatchIndex = -1;

        for (int32 i = 0; i < Results.Num(); ++i)
        {
            const FOnlineSessionSearchResult& Result = Results[i];
            FString RName, Pass;
            Result.Session.SessionSettings.Get(FName(TEXT("ROOM_NAME")), RName);
            Result.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), Pass);

            bool bNameMatches = TargetRoomName.IsEmpty() || RName.Equals(TargetRoomName, ESearchCase::IgnoreCase);
            bool bPassMatches = Pass.Equals(TargetPassword, ESearchCase::CaseSensitive);

            if (bNameMatches && bPassMatches)
            {
                MatchIndex = i;
                break;
            }
        }

        if (MatchIndex != -1)
        {
            UE_LOG(LogTemp, Log, TEXT("Matching private session found at index %d! Joining..."), MatchIndex);
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("비공개 방을 찾았습니다! 접속 중..."));
            }
            JoinFoundSession(MatchIndex);
            return;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("No matching private session found for Room: '%s', Pass: '%s'"),
                *TargetRoomName, *TargetPassword);
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("일치하는 방 제목 또는 비밀번호의 방을 찾을 수 없습니다."));
            }
            return;
        }
    }

    // 2) 공개 방 검색 모드: 비공개 방(Private)은 검색 목록에서 필터링하여 제외
    Results.RemoveAll([](const FOnlineSessionSearchResult& Result) {
        FString MatchType;
        Result.Session.SessionSettings.Get(FName(TEXT("MATCH_TYPE")), MatchType);
        return MatchType.Equals(TEXT("Private"), ESearchCase::IgnoreCase);
    });

    if (GEngine)
    {
        if (Results.Num() > 0)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
                FString::Printf(TEXT("공개 방 %d개를 찾았습니다!"), Results.Num()));
        }
        else
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
                TEXT("현재 열려 있는 공개 방이 없습니다."));
        }
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

    // 공개 방 목록에서 참가 시에도 ETB_PlayerName 확인하여 PlayerName 업데이트
    if (UUserWidget* JoinMenu = FindActiveWidget(TEXT("WBP_JoinRoomMenu")))
    {
        if (UEditableTextBox* Box = Cast<UEditableTextBox>(JoinMenu->GetWidgetFromName(FName(TEXT("ETB_PlayerName")))))
        {
            FString InputPName = Box->GetText().ToString().TrimStartAndEnd();
            if (!InputPName.IsEmpty())
            {
                PlayerName = InputPName;
                UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Updated PlayerName for Join: %s"), *PlayerName);
            }
        }
    }

    const FOnlineSessionSearchResult& Result =
        SessionSearch->SearchResults[Index];

    LastJoinedSearchResult = Result;

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

    FString FoundRoomName;

    Result.Session.SessionSettings.Get(
        FName(TEXT("ROOM_NAME")),
        FoundRoomName
    );

    if (FoundRoomName.IsEmpty())
    {
        FoundRoomName = FString::Printf(TEXT("Room #%d"), Index + 1);
    }

    return FoundRoomName;
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