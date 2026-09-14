#include "TOGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
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

    UUserWidget* FallbackWidget = nullptr;
    for (TObjectIterator<UUserWidget> It; It; ++It)
    {
        UUserWidget* Widget = *It;
        if (IsValid(Widget) && Widget->GetWorld() == CurrentWorld)
        {
            if (Widget->GetClass()->GetName().Contains(ClassSubstr))
            {
                if (Widget->IsInViewport())
                {
                    return Widget;
                }
                if (!FallbackWidget)
                {
                    FallbackWidget = Widget;
                }
            }
        }
    }
    return FallbackWidget;
}

bool UTOGameInstance::TickWidgetBindings(float DeltaTime)
{
    if (UUserWidget* JoinMenu = FindActiveWidget(TEXT("WBP_JoinRoomMenu")))
    {
        // 1. 비공개 방 참가 버튼 자동 바인딩 ([Btn_CreateRoom], Btn_CreateRoom, Btn_Join 등)
        UButton* TargetBtn = Cast<UButton>(JoinMenu->GetWidgetFromName(FName(TEXT("[Btn_CreateRoom]"))));
        if (!TargetBtn)
        {
            TargetBtn = Cast<UButton>(JoinMenu->GetWidgetFromName(FName(TEXT("Btn_CreateRoom"))));
        }
        if (!TargetBtn)
        {
            TargetBtn = Cast<UButton>(JoinMenu->GetWidgetFromName(FName(TEXT("Btn_Join"))));
        }
        if (!TargetBtn && JoinMenu->WidgetTree)
        {
            TArray<UWidget*> AllWidgets;
            JoinMenu->WidgetTree->GetAllWidgets(AllWidgets);
            for (UWidget* W : AllWidgets)
            {
                if (UButton* B = Cast<UButton>(W))
                {
                    FString BName = B->GetName();
                    if ((BName.Contains(TEXT("CreateRoom")) || BName.Contains(TEXT("Join"))) &&
                        !BName.Contains(TEXT("Search")) && !BName.Contains(TEXT("Back")) && !BName.Contains(TEXT("Refresh")))
                    {
                        TargetBtn = B;
                        break;
                    }
                }
            }
        }

        if (TargetBtn && BoundPrivateJoinBtn.Get() != TargetBtn)
        {
            BoundPrivateJoinBtn = TargetBtn;
            TargetBtn->OnClicked.RemoveDynamic(this, &UTOGameInstance::OnPrivateJoinButtonClicked);
            TargetBtn->OnClicked.AddDynamic(this, &UTOGameInstance::OnPrivateJoinButtonClicked);
            UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Successfully bound private join button '%s' on WBP_JoinRoomMenu."), *TargetBtn->GetName());
        }

        if (BoundJoinMenu.Get() != JoinMenu)
        {
            BoundJoinMenu = JoinMenu;

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

void UTOGameInstance::UpdatePlayerNameFromActiveMenu()
{
    if (UUserWidget* JoinMenu = FindActiveWidget(TEXT("WBP_JoinRoomMenu")))
    {
        if (UEditableTextBox* Box = Cast<UEditableTextBox>(JoinMenu->GetWidgetFromName(FName(TEXT("ETB_PlayerName")))))
        {
            FString InputPName = Box->GetText().ToString().TrimStartAndEnd();
            if (!InputPName.IsEmpty())
            {
                PlayerName = InputPName;
                UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Updated PlayerName from JoinMenu: %s"), *PlayerName);
            }
        }
    }
    if (UUserWidget* CreateMenu = FindActiveWidget(TEXT("WBP_CreateRoomMenu")))
    {
        if (UEditableTextBox* Box = Cast<UEditableTextBox>(CreateMenu->GetWidgetFromName(FName(TEXT("ETB_PlayerName")))))
        {
            FString InputPName = Box->GetText().ToString().TrimStartAndEnd();
            if (!InputPName.IsEmpty())
            {
                PlayerName = InputPName;
                UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Updated PlayerName from CreateMenu: %s"), *PlayerName);
            }
        }
    }
}

void UTOGameInstance::OnPrivateJoinButtonClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] OnPrivateJoinButtonClicked triggered!"));
    UpdatePlayerNameFromActiveMenu();

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

    // 1. IP 주소(하마치 25.x.x.x 또는 LAN IP) 직접 입력 시 세션 검색을 거치지 않고 직접 즉시 접속
    FString TargetIP = TEXT("");
    if (IsIPv4Address(InputRoomName))
    {
        TargetIP = InputRoomName;
    }
    else if (IsIPv4Address(InputPassword))
    {
        TargetIP = InputPassword;
    }

    if (!TargetIP.IsEmpty())
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
                FString::Printf(TEXT("호스트 IP 직접 접속 중: %s"), *TargetIP));
        }
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Direct IP connection triggered to: %s"), *TargetIP);
        JoinServerByIP(TargetIP);
        return;
    }

    // 2. 일반 세션 검색 진행
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Yellow,
            FString::Printf(TEXT("비공개 방 검색 중... [방: %s, 비번: %s]"), 
                InputRoomName.IsEmpty() ? TEXT("(전체)") : *InputRoomName,
                InputPassword.IsEmpty() ? TEXT("(없음)") : *InputPassword));
    }

    FindPrivateRooms(InputPassword, InputRoomName);
}

void UTOGameInstance::CreateMySession(const FString& Password, const FString& InRoomName)
{
    UpdatePlayerNameFromActiveMenu();

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

    // 방 제목 및 비밀번호 결정
    // WBP_CreateRoomMenu 위젯이 활성화되어 있다면, 사용자가 UI에 직접 입력한 텍스트를 최우선(Ground Truth)으로 사용!
    FString FinalRoomName = TEXT("");
    FString FinalPassword = TEXT("");

    if (UUserWidget* CreateMenu = FindActiveWidget(TEXT("WBP_CreateRoomMenu")))
    {
        if (UEditableTextBox* Box = Cast<UEditableTextBox>(CreateMenu->GetWidgetFromName(FName(TEXT("ETB_RoomName")))))
        {
            FinalRoomName = Box->GetText().ToString().TrimStartAndEnd();
        }
        if (UEditableTextBox* Box = Cast<UEditableTextBox>(CreateMenu->GetWidgetFromName(FName(TEXT("ETB_Password")))))
        {
            FinalPassword = Box->GetText().ToString().TrimStartAndEnd();
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

    // 위젯에서 값을 가져오지 못한 경우 (위젯 외부 호출 등) 인자 및 프로퍼티로 폴백
    if (FinalRoomName.IsEmpty())
    {
        FinalRoomName = InRoomName.TrimStartAndEnd();
    }
    if (FinalRoomName.IsEmpty())
    {
        FinalRoomName = RoomName.TrimStartAndEnd();
    }
    // 만약 InRoomName은 비어있고 Password만 들어왔는데 WBP_CreateRoomMenu가 없었다면:
    // Blueprint에서 RoomName을 첫 번째 핀에 꽂았을 가능성이 높으므로 방 이름으로 폴백
    if (FinalRoomName.IsEmpty() && !Password.TrimStartAndEnd().IsEmpty())
    {
        FinalRoomName = Password.TrimStartAndEnd();
    }
    else if (FinalPassword.IsEmpty())
    {
        // Password 인자가 FinalRoomName과 같지 않을 때만 비밀번호로 인정
        FString CleanPass = Password.TrimStartAndEnd();
        if (!CleanPass.IsEmpty() && !CleanPass.Equals(FinalRoomName, ESearchCase::CaseSensitive))
        {
            FinalPassword = CleanPass;
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
    if (!FinalPassword.IsEmpty())
    {
        SessionSettings.Set(
            FName(TEXT("ROOM_PASSWORD")),
            FinalPassword,
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

        SessionSettings.Set(
            FName(TEXT("MATCH_TYPE")),
            FString(TEXT("Private")),
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Creating Private Session '%s' with Password '%s'..."), *FinalRoomName, *FinalPassword);
    }
    else
    {
        SessionSettings.Set(
            FName(TEXT("ROOM_PASSWORD")),
            FString(TEXT("")),
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

        SessionSettings.Set(
            FName(TEXT("MATCH_TYPE")),
            FString(TEXT("Public")),
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Creating Public Session '%s'..."), *FinalRoomName);
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
            AdvertisedIP = AdvertisedIP.TrimStartAndEnd();
            if (!AdvertisedIP.Contains(TEXT(":")))
            {
                ConnectAddress = FString::Printf(TEXT("%s:7777"), *AdvertisedIP);
            }
            else
            {
                ConnectAddress = AdvertisedIP;
            }
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
        FString Reason;
        switch (Result)
        {
        case EOnJoinSessionCompleteResult::SessionIsFull:
            Reason = TEXT("방이 가득 찼습니다.");
            break;
        case EOnJoinSessionCompleteResult::SessionDoesNotExist:
            Reason = TEXT("존재하지 않는 방입니다.");
            break;
        case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
            Reason = TEXT("서버 주소를 가져올 수 없습니다.");
            break;
        case EOnJoinSessionCompleteResult::AlreadyInSession:
            Reason = TEXT("이미 세션에 참가 중입니다.");
            break;
        case EOnJoinSessionCompleteResult::UnknownError:
        default:
            Reason = TEXT("알 수 없는 이유로 실패했습니다.");
            break;
        }

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Failed to Join Session! Reason: %s (%d)"),
            *Reason,
            (int32)Result
        );

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                FString::Printf(TEXT("세션 참가 실패! 사유: %s"), *Reason));
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
    UpdatePlayerNameFromActiveMenu();

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
    UpdatePlayerNameFromActiveMenu();

    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("SessionInterface is invalid."));
        return;
    }

    FString CleanPass = SearchPassword.TrimStartAndEnd();
    FString CleanRoom = SearchRoomName.TrimStartAndEnd();

    // 비밀번호와 방 이름이 모두 비어있는 경우 (메뉴 탭 전환 시 불필요하게 자동 호출된 경우 등)
    // 아무 방이나 자동 매칭되어 입장해버리는 것을 방지하고 사용자 입력을 대기합니다.
    if (CleanPass.IsEmpty() && CleanRoom.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] FindPrivateRooms: Both Password and RoomName are empty. Waiting for user input."));
        return;
    }

    bIsSearchingPrivate = true;
    TargetPassword = CleanPass;
    TargetRoomName = CleanRoom;

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
    if (!bWasSuccessful || !SessionSearch.IsValid() || SessionSearch->SearchResults.Num() == 0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Failed to find sessions or 0 sessions found.")
        );

        if (bIsSearchingPrivate)
        {
            bIsSearchingPrivate = false;
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
                    TEXT("방을 찾지 못했습니다.\n[하마치 이용 시] '방 이름' 칸에 호스트의 하마치 IP(25.x.x.x)를 입력하면 즉시 접속됩니다!"));
            }
        }
        else
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Yellow, 
                    TEXT("네트워크에서 방을 발견하지 못했습니다. (0개 발견)\n[확인] 호스트가 방을 개설했는지 또는 방화벽/하마치 연결을 확인해주세요."));
            }
        }

        OnRoomsFound.Broadcast();
        PopulateServerList();
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

        FString CleanTargetPassword = TargetPassword.TrimStartAndEnd();
        FString CleanTargetRoomName = TargetRoomName.TrimStartAndEnd();

        // 1차 패스: 방 이름과 비밀번호가 모두 일치하는 세션 탐색
        for (int32 i = 0; i < Results.Num(); ++i)
        {
            const FOnlineSessionSearchResult& Result = Results[i];
            FString RName, Pass;
            Result.Session.SessionSettings.Get(FName(TEXT("ROOM_NAME")), RName);
            Result.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), Pass);

            FString CleanPass = Pass.TrimStartAndEnd();
            FString CleanRName = RName.TrimStartAndEnd();

            bool bPassMatches = false;
            if (!CleanPass.IsEmpty())
            {
                bPassMatches = !CleanTargetPassword.IsEmpty() && CleanPass.Equals(CleanTargetPassword, ESearchCase::CaseSensitive);
            }
            else
            {
                bPassMatches = CleanTargetPassword.IsEmpty();
            }

            bool bNameMatches = false;
            if (!CleanTargetRoomName.IsEmpty())
            {
                bNameMatches = CleanRName.Equals(CleanTargetRoomName, ESearchCase::IgnoreCase) ||
                               CleanRName.Contains(CleanTargetRoomName, ESearchCase::IgnoreCase);
            }
            else
            {
                bNameMatches = true;
            }

            if (bNameMatches && bPassMatches)
            {
                MatchIndex = i;
                break;
            }
        }

        // 2차 패스: 방 이름이 달라도 비밀번호가 일치하는 방이 있는 경우
        if (MatchIndex == -1 && !CleanTargetPassword.IsEmpty())
        {
            for (int32 i = 0; i < Results.Num(); ++i)
            {
                const FOnlineSessionSearchResult& Result = Results[i];
                FString Pass;
                Result.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), Pass);
                if (Pass.TrimStartAndEnd().Equals(CleanTargetPassword, ESearchCase::CaseSensitive))
                {
                    MatchIndex = i;
                    break;
                }
            }
        }

        // 3차 패스: 검색된 방이 1개뿐이고 비밀번호가 일치하는 경우
        if (MatchIndex == -1 && Results.Num() == 1 && !CleanTargetPassword.IsEmpty())
        {
            const FOnlineSessionSearchResult& Result = Results[0];
            FString Pass;
            Result.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), Pass);
            if (Pass.TrimStartAndEnd().Equals(CleanTargetPassword, ESearchCase::CaseSensitive))
            {
                MatchIndex = 0;
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
                GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
                    TEXT("일치하는 방을 찾지 못했습니다.\n[하마치 이용 시] '방 이름' 칸에 호스트의 하마치 IP(25.x.x.x)를 입력하면 즉시 접속됩니다!"));
            }
            return;
        }
    }

    // 2) 공개 방 검색(서버 목록) 모드:
    // 검색된 세션을 공개 방 우선으로 정렬하여 서버 목록(Scroll_ServerList)에 표시합니다.
    // 비공개 방도 삭제하지 않고 목록에 표시하여 게스트가 접속할 수 있도록 지원합니다.
    Results.Sort([](const FOnlineSessionSearchResult& A, const FOnlineSessionSearchResult& B) {
        FString MatchTypeA, MatchTypeB;
        A.Session.SessionSettings.Get(FName(TEXT("MATCH_TYPE")), MatchTypeA);
        B.Session.SessionSettings.Get(FName(TEXT("MATCH_TYPE")), MatchTypeB);
        bool bIsPrivateA = MatchTypeA.Equals(TEXT("Private"), ESearchCase::IgnoreCase);
        bool bIsPrivateB = MatchTypeB.Equals(TEXT("Private"), ESearchCase::IgnoreCase);
        return (!bIsPrivateA && bIsPrivateB); // 공개 방이 위쪽에 오도록 정렬
    });

    int32 PublicCount = 0;
    int32 PrivateCount = 0;
    for (const auto& Res : Results)
    {
        FString MType;
        Res.Session.SessionSettings.Get(FName(TEXT("MATCH_TYPE")), MType);
        if (MType.Equals(TEXT("Private"), ESearchCase::IgnoreCase))
        {
            PrivateCount++;
        }
        else
        {
            PublicCount++;
        }
    }

    if (GEngine)
    {
        if (Results.Num() > 0)
        {
            if (PublicCount > 0 && PrivateCount > 0)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
                    FString::Printf(TEXT("방 %d개를 찾았습니다! (공개: %d, 비공개: %d)"), Results.Num(), PublicCount, PrivateCount));
            }
            else if (PublicCount > 0)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
                    FString::Printf(TEXT("공개 방 %d개를 찾았습니다!"), PublicCount));
            }
            else
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
                    FString::Printf(TEXT("비공개 방 %d개를 찾았습니다! (목록에서 선택하거나 비공개 탭 이용)"), PrivateCount));
            }
        }
        else
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
                TEXT("현재 열려 있는 방이 없습니다."));
        }
    }

    // 1. 먼저 블루프린트의 OnRoomsFound_Event 호출 (블루프린트의 ClearChildren() 등이 먼저 실행되도록 함)
    OnRoomsFound.Broadcast();

    // 2. 블루프린트 처리가 끝난 후 C++에서 UI Scroll_ServerList에 검색된 공개 방 목록을 최종 등록
    PopulateServerList();
}

void UTOGameInstance::PopulateServerList()
{
    UUserWidget* JoinMenu = FindActiveWidget(TEXT("WBP_JoinRoomMenu"));
    if (!JoinMenu)
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] PopulateServerList: WBP_JoinRoomMenu not active."));
        return;
    }

    UScrollBox* ScrollList = Cast<UScrollBox>(JoinMenu->GetWidgetFromName(FName(TEXT("Scroll_ServerList"))));
    if (!ScrollList)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] PopulateServerList: Scroll_ServerList not found."));
        return;
    }

    ScrollList->ClearChildren();

    if (!SessionSearch.IsValid() || SessionSearch->SearchResults.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] PopulateServerList: No sessions to display."));
        return;
    }

    UClass* EntryClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, TEXT("/Game/UI/SubWidget/WBP_ServerEntry.WBP_ServerEntry_C"));
    if (!EntryClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] PopulateServerList: Failed to load WBP_ServerEntry class!"));
        return;
    }

    for (int32 i = 0; i < SessionSearch->SearchResults.Num(); ++i)
    {
        UUserWidget* EntryWidget = CreateWidget<UUserWidget>(this, EntryClass);
        if (!EntryWidget) continue;

        // 1. SessionIndex 프로퍼티 설정
        if (FIntProperty* Prop = CastField<FIntProperty>(EntryWidget->GetClass()->FindPropertyByName(FName(TEXT("SessionIndex")))))
        {
            Prop->SetPropertyValue_InContainer(EntryWidget, i);
        }

        // 2. 방 제목 텍스트 설정
        if (UTextBlock* NameText = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(FName(TEXT("Text_ServerName")))))
        {
            NameText->SetText(FText::FromString(GetFoundRoomName(i)));
        }

        // 3. 인원 수 텍스트 설정
        if (UTextBlock* CountText = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(FName(TEXT("Text_PlayerCount")))))
        {
            CountText->SetText(FText::FromString(FString::Printf(TEXT("%d / 6"), GetFoundRoomPlayerCount(i))));
        }

        // 4. ScrollList에 엔트리 추가
        ScrollList->AddChild(EntryWidget);
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Added server entry [%d]: '%s'"), i, *GetFoundRoomName(i));
    }
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

    // 기존 세션이 남아있다면 반드시 먼저 정리 (두 번째 세션 참가 시 AlreadyInSession 오류 방지)
    auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
    if (ExistingSession != nullptr)
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Destroying existing session before joining new session..."));
        SessionInterface->DestroySession(NAME_GameSession);
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

    FString MatchType;
    Result.Session.SessionSettings.Get(FName(TEXT("MATCH_TYPE")), MatchType);
    FString Pass;
    Result.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), Pass);

    if (MatchType.Equals(TEXT("Private"), ESearchCase::IgnoreCase) || !Pass.IsEmpty())
    {
        return FString::Printf(TEXT("[비공개] %s"), *FoundRoomName);
    }
    else
    {
        return FString::Printf(TEXT("[공개] %s"), *FoundRoomName);
    }
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
    UpdatePlayerNameFromActiveMenu();

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