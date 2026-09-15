#include "TOGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
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
#include "Misc/ConfigCacheIni.h"
#include "Misc/Base64.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineExternalUIInterface.h"

namespace
{
    // Steam OSS에서 다국어(한글 등) 방 이름이 ANSI 변환으로 인해 깨지는 현상을 방지하기 위한 Base64 헬퍼
    FString EncodeRoomNameForSession(const FString& InRoomName)
    {
        return TEXT("B64_") + FBase64::Encode(InRoomName);
    }

    FString DecodeRoomNameFromSession(const FString& InRawName)
    {
        if (InRawName.StartsWith(TEXT("B64_")))
        {
            FString Decoded;
            if (FBase64::Decode(InRawName.RightChop(4), Decoded))
            {
                return Decoded;
            }
        }
        return InRawName;
    }
}

UTOGameInstance::UTOGameInstance()
{
}

void UTOGameInstance::Init()
{
    Super::Init();

    if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
    {
        SessionInterface = Subsystem->GetSessionInterface();
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] OnlineSubsystem Name: %s"), *Subsystem->GetSubsystemName().ToString());

        FString SavedName;
        if (GConfig && GConfig->GetString(TEXT("Operator"), TEXT("SavedPlayerName"), SavedName, GGameIni))
        {
            if (!SavedName.TrimStartAndEnd().IsEmpty())
            {
                PlayerName = SavedName.TrimStartAndEnd();
                UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Restored saved PlayerName: %s"), *PlayerName);
            }
        }

        if (IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface())
        {
            FString SteamNickname = Identity->GetPlayerNickname(0);
            if (!SteamNickname.IsEmpty() && (PlayerName.IsEmpty() || PlayerName.Equals(TEXT("Player"))))
            {
                PlayerName = SteamNickname;
                UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Initialized PlayerName from Steam: %s"), *PlayerName);
            }
        }

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

            SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(
                this,
                &UTOGameInstance::OnDestroySessionComplete
            );

            SessionInterface->OnSessionUserInviteAcceptedDelegates.AddUObject(
                this,
                &UTOGameInstance::OnSessionUserInviteAccepted
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
    // 1. 대기실(WBP_WaitingGame) 나가기 버튼 및 스팀 초대 버튼 자동 바인딩
    if (UUserWidget* WaitingGame = FindActiveWidget(TEXT("WBP_WaitingGame")))
    {
        UButton* ExitBtn = Cast<UButton>(WaitingGame->GetWidgetFromName(FName(TEXT("ExitButton"))));
        if (!ExitBtn)
        {
            ExitBtn = Cast<UButton>(WaitingGame->GetWidgetFromName(FName(TEXT("Btn_Exit"))));
        }
        if (ExitBtn && BoundWaitingGameExitBtn.Get() != ExitBtn)
        {
            BoundWaitingGameExitBtn = ExitBtn;
            ExitBtn->OnClicked.RemoveDynamic(this, &UTOGameInstance::OnWaitingGameExitClicked);
            ExitBtn->OnClicked.AddDynamic(this, &UTOGameInstance::OnWaitingGameExitClicked);
            UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Successfully bound ExitButton on WBP_WaitingGame to OnWaitingGameExitClicked."));
        }

        // 스팀 친구 초대 버튼 자동 바인딩 (InviteButton, Btn_Invite, Btn_SteamInvite 등)
        UButton* InviteBtn = Cast<UButton>(WaitingGame->GetWidgetFromName(FName(TEXT("InviteButton"))));
        if (!InviteBtn)
        {
            InviteBtn = Cast<UButton>(WaitingGame->GetWidgetFromName(FName(TEXT("Btn_Invite"))));
        }
        if (!InviteBtn && WaitingGame->WidgetTree)
        {
            TArray<UWidget*> AllWidgets;
            WaitingGame->WidgetTree->GetAllWidgets(AllWidgets);
            for (UWidget* W : AllWidgets)
            {
                if (UButton* B = Cast<UButton>(W))
                {
                    FString BName = B->GetName();
                    if (BName.Contains(TEXT("Invite"), ESearchCase::IgnoreCase))
                    {
                        InviteBtn = B;
                        break;
                    }
                }
            }
        }
        if (InviteBtn)
        {
            InviteBtn->OnClicked.RemoveDynamic(this, &UTOGameInstance::OpenSteamInviteUI);
            InviteBtn->OnClicked.AddDynamic(this, &UTOGameInstance::OpenSteamInviteUI);
        }
    }

    if (UUserWidget* JoinMenu = FindActiveWidget(TEXT("WBP_JoinRoomMenu")))
    {
        // 2. 비공개 방 참가 버튼 자동 바인딩 ([Btn_CreateRoom], Btn_CreateRoom, Btn_Join 등)
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

            // 메뉴 최초 진입 시 기존 닉네임이 있다면 텍스트 박스 자동 프리필
            if (!PlayerName.IsEmpty() && !PlayerName.Equals(TEXT("Player")))
            {
                if (UEditableTextBox* Box = Cast<UEditableTextBox>(JoinMenu->GetWidgetFromName(FName(TEXT("Input_Nickname")))))
                {
                    if (Box->GetText().IsEmpty()) Box->SetText(FText::FromString(PlayerName));
                }
                if (UEditableTextBox* Box = Cast<UEditableTextBox>(JoinMenu->GetWidgetFromName(FName(TEXT("ETB_PlayerName")))))
                {
                    if (Box->GetText().IsEmpty()) Box->SetText(FText::FromString(PlayerName));
                }
            }
        }
    }

    if (UUserWidget* CreateMenu = FindActiveWidget(TEXT("WBP_CreateRoomMenu")))
    {
        if (BoundCreateMenu.Get() != CreateMenu)
        {
            BoundCreateMenu = CreateMenu;

            if (!PlayerName.IsEmpty() && !PlayerName.Equals(TEXT("Player")))
            {
                if (UEditableTextBox* Box = Cast<UEditableTextBox>(CreateMenu->GetWidgetFromName(FName(TEXT("ETB_PlayerName")))))
                {
                    if (Box->GetText().IsEmpty()) Box->SetText(FText::FromString(PlayerName));
                }
            }
        }
    }

    // 활성 메뉴 위젯에서 닉네임 입력값을 실시간 동기화
    UpdatePlayerNameFromActiveMenu();

    return true;
}

void UTOGameInstance::CleanUpExistingSession()
{
    if (!SessionInterface.IsValid()) return;

    if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] CleanUpExistingSession: Destroying active session '%s'..."), *FName(NAME_GameSession).ToString());
        SessionInterface->DestroySession(NAME_GameSession);
    }
}

void UTOGameInstance::LeaveSessionAndReturnToMenu()
{
    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] LeaveSessionAndReturnToMenu called."));
    CleanUpExistingSession();

    if (APlayerController* PC = GetFirstLocalPlayerController())
    {
        PC->ClientTravel(TEXT("/Game/UI/MainMenu/Level/OP_MainMenu"), ETravelType::TRAVEL_Absolute);
    }
    else if (UWorld* World = GetWorld())
    {
        World->ServerTravel(TEXT("/Game/UI/MainMenu/Level/OP_MainMenu"));
    }
}

void UTOGameInstance::OnWaitingGameExitClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] OnWaitingGameExitClicked: Gracefully leaving lobby to main menu..."));
    LeaveSessionAndReturnToMenu();
}

void UTOGameInstance::OpenSteamInviteUI()
{
    if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
    {
        if (IOnlineExternalUIPtr ExternalUI = Subsystem->GetExternalUIInterface())
        {
            ExternalUI->ShowInviteUI(0, NAME_GameSession);
            UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] OpenSteamInviteUI: Opened Steam friend invite dialog."));
            return;
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] OpenSteamInviteUI: ExternalUI interface not available."));
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

bool UTOGameInstance::IsUsingSteamSubsystem() const
{
    if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
    {
        return Subsystem->GetSubsystemName() == FName(TEXT("Steam"));
    }
    return false;
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

void UTOGameInstance::SavePlayerNameToConfig()
{
    if (GConfig && !PlayerName.IsEmpty() && !PlayerName.Equals(TEXT("Player")))
    {
        GConfig->SetString(TEXT("Operator"), TEXT("SavedPlayerName"), *PlayerName, GGameIni);
        GConfig->Flush(false, GGameIni);
    }
}

static FString ExtractPlayerNameFromWidget(UUserWidget* Widget)
{
    if (!Widget) return TEXT("");

    // 1. 공용 위젯 명칭 우선 확인 (Input_Nickname, ETB_PlayerName, ETB_Nickname, Nickname 등)
    const TArray<FName> KnownNames = {
        FName(TEXT("Input_Nickname")),
        FName(TEXT("ETB_PlayerName")),
        FName(TEXT("ETB_Nickname")),
        FName(TEXT("NicknameTextBox")),
        FName(TEXT("PlayerNameTextBox")),
        FName(TEXT("Text_Nickname")),
        FName(TEXT("Text_PlayerName"))
    };

    for (const FName& WName : KnownNames)
    {
        if (UEditableTextBox* Box = Cast<UEditableTextBox>(Widget->GetWidgetFromName(WName)))
        {
            FString Text = Box->GetText().ToString().TrimStartAndEnd();
            if (!Text.IsEmpty())
            {
                return Text;
            }
        }
    }

    // 2. 위젯 트리 전체 순회하여 이름에 Nickname이나 PlayerName이 포함된 EditableTextBox 탐색
    if (Widget->WidgetTree)
    {
        TArray<UWidget*> AllWidgets;
        Widget->WidgetTree->GetAllWidgets(AllWidgets);
        for (UWidget* W : AllWidgets)
        {
            if (UEditableTextBox* Box = Cast<UEditableTextBox>(W))
            {
                FString BoxName = Box->GetName();
                if ((BoxName.Contains(TEXT("Nick"), ESearchCase::IgnoreCase) || 
                     BoxName.Contains(TEXT("PlayerName"), ESearchCase::IgnoreCase)) &&
                    !BoxName.Contains(TEXT("Room"), ESearchCase::IgnoreCase) &&
                    !BoxName.Contains(TEXT("Pass"), ESearchCase::IgnoreCase))
                {
                    FString Text = Box->GetText().ToString().TrimStartAndEnd();
                    if (!Text.IsEmpty())
                    {
                        return Text;
                    }
                }
            }
        }
    }

    return TEXT("");
}

void UTOGameInstance::UpdatePlayerNameFromActiveMenu()
{
    if (UUserWidget* JoinMenu = FindActiveWidget(TEXT("WBP_JoinRoomMenu")))
    {
        FString FoundName = ExtractPlayerNameFromWidget(JoinMenu);
        if (!FoundName.IsEmpty())
        {
            PlayerName = FoundName;
            UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Updated PlayerName from JoinMenu: %s"), *PlayerName);
            SavePlayerNameToConfig();
        }
    }
    if (UUserWidget* CreateMenu = FindActiveWidget(TEXT("WBP_CreateRoomMenu")))
    {
        FString FoundName = ExtractPlayerNameFromWidget(CreateMenu);
        if (!FoundName.IsEmpty())
        {
            PlayerName = FoundName;
            UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Updated PlayerName from CreateMenu: %s"), *PlayerName);
            SavePlayerNameToConfig();
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

    FString InputRoomName;
    FString InputPassword;

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

    FOnlineSessionSettings SessionSettings;

    bool bIsSteam = IsUsingSteamSubsystem();
    SessionSettings.bIsLANMatch = !bIsSteam;
    SessionSettings.NumPublicConnections = 6;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bUsesPresence = bIsSteam;
    SessionSettings.bUseLobbiesIfAvailable = bIsSteam;
    SessionSettings.bAllowJoinViaPresence = bIsSteam;
    SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;
    SessionSettings.bAllowInvites = true;

    // Steam AppID 480 환경에서 우리 프로젝트 게임 방만 식별하기 위한 고유 키
    SessionSettings.Set(
        FName(TEXT("OPERATOR_APP")),
        FString(TEXT("Operator_Team6_CH4")),
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
    );

    // 호스트 IP(LAN / 하마치 환경 폴백용)
    if (!bIsSteam)
    {
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
    }

    // 방 제목 및 비밀번호 결정
    // WBP_CreateRoomMenu 위젯이 활성화되어 있다면, 사용자가 UI에 직접 입력한 텍스트를 최우선(Ground Truth)으로 사용!
    bool bCreateMenuFound = false;
    FString FinalRoomName = TEXT("");
    FString FinalPassword = TEXT("");

    if (UUserWidget* CreateMenu = FindActiveWidget(TEXT("WBP_CreateRoomMenu")))
    {
        bCreateMenuFound = true;
        if (UEditableTextBox* Box = Cast<UEditableTextBox>(CreateMenu->GetWidgetFromName(FName(TEXT("ETB_RoomName")))))
        {
            FinalRoomName = Box->GetText().ToString().TrimStartAndEnd();
        }
        if (UEditableTextBox* Box = Cast<UEditableTextBox>(CreateMenu->GetWidgetFromName(FName(TEXT("ETB_Password")))))
        {
            FinalPassword = Box->GetText().ToString().TrimStartAndEnd();
        }
        FString FoundPName = ExtractPlayerNameFromWidget(CreateMenu);
        if (!FoundPName.IsEmpty())
        {
            PlayerName = FoundPName;
            SavePlayerNameToConfig();
        }
    }

    // WBP_CreateRoomMenu가 활성화되어 있었던 경우:
    // UI의 비밀번호 입력창(ETB_Password) 상태가 절대적 기준입니다.
    // 팀원 의도대로 사용자가 비밀번호를 비워두었으면(공백), 공개방 개설 의도이므로 FinalPassword는 빈 문자열로 확정합니다!
    if (!bCreateMenuFound)
    {
        // UI가 없을 때만 인자(InRoomName, Password)에서 추론
        FString ArgRoom = InRoomName.TrimStartAndEnd();
        FString ArgPass = Password.TrimStartAndEnd();

        if (!ArgRoom.IsEmpty())
        {
            FinalRoomName = ArgRoom;
            FinalPassword = ArgPass;
        }
        else if (!ArgPass.IsEmpty())
        {
            // 인자가 하나만 들어온 경우 방 이름으로 취급 (비밀번호 없음 -> 공개방)
            FinalRoomName = ArgPass;
            FinalPassword = TEXT("");
        }
    }

    if (FinalRoomName.IsEmpty())
    {
        FinalRoomName = RoomName.TrimStartAndEnd();
    }
    if (FinalRoomName.IsEmpty())
    {
        FinalRoomName = PlayerName + TEXT("'s Room");
    }
    RoomName = FinalRoomName;

    SessionSettings.Set(
        FName(TEXT("ROOM_NAME")),
        EncodeRoomNameForSession(FinalRoomName),
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
        // Steam OSS는 빈 문자열 세션 설정에 대해 경고를 출력하고 로비 메타데이터에 등록하지 않으므로 "NONE"으로 설정
        SessionSettings.Set(
            FName(TEXT("ROOM_PASSWORD")),
            FString(TEXT("NONE")),
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

        SessionSettings.Set(
            FName(TEXT("MATCH_TYPE")),
            FString(TEXT("Public")),
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Creating Public Session '%s' (Password is blank/empty)..."), *FinalRoomName);
    }

    // 기존 세션이 존재하면 먼저 비동기로 파기 후 파기 완료 콜백에서 세션 생성 실행!
    auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
    if (ExistingSession != nullptr)
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Existing session found. Destroying session first before creating a new one..."));
        PendingSessionAction = EPendingSessionAction::Create;
        PendingSessionSettings = SessionSettings;
        if (!SessionInterface->DestroySession(NAME_GameSession))
        {
            UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] DestroySession call failed synchronously. Creating session anyway..."));
            PendingSessionAction = EPendingSessionAction::None;
            SessionInterface->CreateSession(0, NAME_GameSession, SessionSettings);
        }
        return;
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
            if (IsUsingSteamSubsystem())
            {
                GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green,
                    FString::Printf(TEXT("방 생성 성공! [스팀 로비 개설 완료: '%s']"), *RoomName));
            }
            else
            {
                FString HostIP = GetBestHostIP();
                GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green,
                    FString::Printf(TEXT("방 생성 성공! [호스트 IP: %s:7777]"), HostIP.IsEmpty() ? TEXT("Local") : *HostIP));
            }
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
            if (ConnectAddress.IsEmpty() && LastJoinedSearchResult.IsValid())
            {
                SessionInterface->GetResolvedConnectString(
                    LastJoinedSearchResult,
                    NAME_GamePort,
                    ConnectAddress
                );
            }
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
                FString TravelURL = ConnectAddress;
                if (!PlayerName.IsEmpty())
                {
                    TravelURL += FString::Printf(TEXT("?Name=%s"), *PlayerName);
                }
                UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] ClientTravel to: %s (PlayerName: %s)"), *TravelURL, *PlayerName);
                PC->ClientTravel(
                    TravelURL,
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

    if (bIsSessionSearchInProgress || (SessionSearch.IsValid() && SessionSearch->SearchState == EOnlineAsyncTaskState::InProgress))
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] FindRooms: Search already in progress, ignoring duplicate call."));
        return;
    }

    bIsSessionSearchInProgress = true;
    bIsSearchingPrivate = false;

    bool bIsSteam = IsUsingSteamSubsystem();
    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->MaxSearchResults = 100;
    SessionSearch->bIsLanQuery = !bIsSteam;

    if (bIsSteam)
    {
        SessionSearch->QuerySettings.Set(
            SEARCH_LOBBIES,
            true,
            EOnlineComparisonOp::Equals
        );
        SessionSearch->QuerySettings.Set(
            FName(TEXT("OPERATOR_APP")),
            FString(TEXT("Operator_Team6_CH4")),
            EOnlineComparisonOp::Equals
        );
    }

    UE_LOG(LogTemp, Log, TEXT("Searching for Public Sessions (Steam: %s)..."), bIsSteam ? TEXT("Yes") : TEXT("No"));
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Yellow, 
            bIsSteam ? TEXT("스팀 공개 방 목록 검색 중...") : TEXT("공개 방 목록 검색 중..."));
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

    if (bIsSessionSearchInProgress || (SessionSearch.IsValid() && SessionSearch->SearchState == EOnlineAsyncTaskState::InProgress))
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] FindPrivateRooms: Search already in progress, ignoring duplicate call."));
        return;
    }

    bIsSessionSearchInProgress = true;
    bIsSearchingPrivate = true;
    TargetPassword = CleanPass;
    TargetRoomName = CleanRoom;

    bool bIsSteam = IsUsingSteamSubsystem();
    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->MaxSearchResults = 100;
    SessionSearch->bIsLanQuery = !bIsSteam;

    if (bIsSteam)
    {
        SessionSearch->QuerySettings.Set(
            SEARCH_LOBBIES,
            true,
            EOnlineComparisonOp::Equals
        );
        SessionSearch->QuerySettings.Set(
            FName(TEXT("OPERATOR_APP")),
            FString(TEXT("Operator_Team6_CH4")),
            EOnlineComparisonOp::Equals
        );
    }

    UE_LOG(LogTemp, Log, TEXT("Searching for Private Sessions (Target Room: '%s', Pass: '%s', Steam: %s)..."),
        *TargetRoomName, *TargetPassword, bIsSteam ? TEXT("Yes") : TEXT("No"));

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Yellow, 
            bIsSteam ? TEXT("스팀 비공개 방 검색 중...") : TEXT("비공개 방 검색 중..."));
    }

    SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
}

void UTOGameInstance::OnFindSessionsComplete(
    bool bWasSuccessful
)
{
    bIsSessionSearchInProgress = false;
    bool bIsSteam = IsUsingSteamSubsystem();

    // 1. Steam AppID 480 환경 필터링: 우리 게임(OPERATOR_APP == Operator_Team6_CH4) 세션만 남김
    if (bIsSteam && SessionSearch.IsValid())
    {
        SessionSearch->SearchResults.RemoveAll([](const FOnlineSessionSearchResult& Item) {
            FString AppTag;
            if (Item.Session.SessionSettings.Get(FName(TEXT("OPERATOR_APP")), AppTag) ||
                Item.Session.SessionSettings.Get(FName(TEXT("OPERATOR_APP_s")), AppTag))
            {
                return !AppTag.Equals(TEXT("Operator_Team6_CH4"));
            }
            // Steam 쿼리(SearchSettings)에서 이미 OPERATOR_APP으로 필터링되어 반환된 결과이므로 보존
            return false;
        });
    }

    if (!bWasSuccessful || !SessionSearch.IsValid() || SessionSearch->SearchResults.Num() == 0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Failed to find sessions or 0 sessions found (Steam: %s)."),
            bIsSteam ? TEXT("Yes") : TEXT("No")
        );

        if (bIsSearchingPrivate)
        {
            bIsSearchingPrivate = false;
            if (GEngine)
            {
                if (bIsSteam)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
                        TEXT("일치하는 비공개 방을 찾지 못했습니다.\n호스트가 방을 개설했는지 또는 비밀번호를 다시 확인해주세요."));
                }
                else
                {
                    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
                        TEXT("방을 찾지 못했습니다.\n[하마치 이용 시] '방 이름' 칸에 호스트의 하마치 IP(25.x.x.x)를 입력하면 즉시 접속됩니다!"));
                }
            }
        }
        else
        {
            if (GEngine)
            {
                if (bIsSteam)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow, 
                        TEXT("스팀에서 열려 있는 방을 발견하지 못했습니다. (0개 발견)\n호스트가 방을 개설했는지 확인해주세요."));
                }
                else
                {
                    FString SavedIP;
                    if (GConfig)
                    {
                        GConfig->GetString(TEXT("Operator"), TEXT("LastHostIP"), SavedIP, GGameIni);
                    }
                    if (!SavedIP.IsEmpty())
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow, 
                            FString::Printf(TEXT("하마치 특성으로 자동 탐색 0개 발견\n목록에 [최근 호스트 IP: %s] 가 등록되었습니다. 참가 버튼을 누르면 즉시 입장됩니다!"), *SavedIP));
                    }
                    else
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow, 
                            TEXT("네트워크에서 방을 발견하지 못했습니다. (0개 발견)\n[하마치 안내] 하마치는 브로드캐스트를 차단하므로 [비공개 방 찾기]에서 호스트 IP(25.x.x.x)로 접속해주세요!"));
                    }
                }
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
        TEXT("Found %d sessions total (Steam: %s)."),
        Results.Num(),
        bIsSteam ? TEXT("Yes") : TEXT("No")
    );

    for (int32 i = 0; i < Results.Num(); ++i)
    {
        const FOnlineSessionSearchResult& Result = Results[i];
        FString RName, Pass, MType, HostIP;
        Result.Session.SessionSettings.Get(FName(TEXT("ROOM_NAME")), RName);
        RName = DecodeRoomNameFromSession(RName);
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
            RName = DecodeRoomNameFromSession(RName);
            Result.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), Pass);

            FString CleanPass = Pass.TrimStartAndEnd();
            if (CleanPass.Equals(TEXT("NONE"), ESearchCase::IgnoreCase))
            {
                CleanPass = TEXT("");
            }
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
                FString CleanP = Pass.TrimStartAndEnd();
                if (CleanP.Equals(TEXT("NONE"), ESearchCase::IgnoreCase))
                {
                    CleanP = TEXT("");
                }
                if (CleanP.Equals(CleanTargetPassword, ESearchCase::CaseSensitive))
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
            FString CleanP = Pass.TrimStartAndEnd();
            if (CleanP.Equals(TEXT("NONE"), ESearchCase::IgnoreCase))
            {
                CleanP = TEXT("");
            }
            if (CleanP.Equals(CleanTargetPassword, ESearchCase::CaseSensitive))
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
                if (bIsSteam)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
                        TEXT("일치하는 비공개 방을 찾지 못했습니다.\n방 이름 또는 비밀번호를 다시 확인해주세요."));
                }
                else
                {
                    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
                        TEXT("일치하는 방을 찾지 못했습니다.\n[하마치 이용 시] '방 이름' 칸에 호스트의 하마치 IP(25.x.x.x)를 입력하면 즉시 접속됩니다!"));
                }
            }
            return;
        }
    }

    // 2) 공개 방 검색(서버 목록) 모드:
    // 검색된 세션을 공개 방 우선으로 정렬하여 서버 목록(Scroll_ServerList)에 표시합니다.
    Results.Sort([](const FOnlineSessionSearchResult& A, const FOnlineSessionSearchResult& B) {
        FString MatchTypeA, MatchTypeB;
        A.Session.SessionSettings.Get(FName(TEXT("MATCH_TYPE")), MatchTypeA);
        B.Session.SessionSettings.Get(FName(TEXT("MATCH_TYPE")), MatchTypeB);
        FString PassA, PassB;
        A.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), PassA);
        B.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), PassB);

        bool bIsPrivateA = MatchTypeA.Equals(TEXT("Private"), ESearchCase::IgnoreCase) || (!PassA.IsEmpty() && !PassA.Equals(TEXT("NONE"), ESearchCase::IgnoreCase));
        bool bIsPrivateB = MatchTypeB.Equals(TEXT("Private"), ESearchCase::IgnoreCase) || (!PassB.IsEmpty() && !PassB.Equals(TEXT("NONE"), ESearchCase::IgnoreCase));
        return (!bIsPrivateA && bIsPrivateB); // 공개 방이 위쪽에 오도록 정렬
    });

    int32 PublicCount = 0;
    int32 PrivateCount = 0;
    for (const auto& Res : Results)
    {
        FString MType;
        Res.Session.SessionSettings.Get(FName(TEXT("MATCH_TYPE")), MType);
        FString Pass;
        Res.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), Pass);
        bool bIsPriv = MType.Equals(TEXT("Private"), ESearchCase::IgnoreCase) || (!Pass.IsEmpty() && !Pass.Equals(TEXT("NONE"), ESearchCase::IgnoreCase));
        if (bIsPriv)
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

    UClass* EntryClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, TEXT("/Game/UI/SubWidget/WBP_ServerEntry.WBP_ServerEntry_C"));
    if (!EntryClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] PopulateServerList: Failed to load WBP_ServerEntry class!"));
        return;
    }

    if (!SessionSearch.IsValid() || SessionSearch->SearchResults.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] PopulateServerList: No sessions to display."));

        // 하마치 등 LAN 브로드캐스트가 차단된 환경에서도 최근 호스트 IP가 있으면 서버 목록에 표시
        FString SavedIP;
        if (GConfig)
        {
            GConfig->GetString(TEXT("Operator"), TEXT("LastHostIP"), SavedIP, GGameIni);
        }
        if (!SavedIP.IsEmpty())
        {
            UUserWidget* EntryWidget = CreateWidget<UUserWidget>(this, EntryClass);
            if (EntryWidget)
            {
                if (FIntProperty* Prop = CastField<FIntProperty>(EntryWidget->GetClass()->FindPropertyByName(FName(TEXT("SessionIndex")))))
                {
                    Prop->SetPropertyValue_InContainer(EntryWidget, 9999);
                }
                if (UTextBlock* NameText = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(FName(TEXT("Text_ServerName")))))
                {
                    NameText->SetText(FText::FromString(FString::Printf(TEXT("[하마치 호스트] %s"), *SavedIP)));
                }
                if (UTextBlock* CountText = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(FName(TEXT("Text_PlayerCount")))))
                {
                    CountText->SetText(FText::FromString(TEXT("직접 접속")));
                }
                ScrollList->AddChild(EntryWidget);
                UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Added saved host IP entry to server list: %s"), *SavedIP);
            }
        }
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

        // 2. 텍스트 위젯 안전 바인딩:
        // WBP_ServerEntry 내부에는 Text_ServerName, Text_PlayerCount, Text_Join 세 개의 TextBlock이 존재합니다.
        // 디자이너에서 네이밍이 혼동되어 있더라도(예: Text_PlayerCount에 방이름 목업 "탐정들의 비밀 모임", Text_Join에 "4 / 6", Text_ServerName에 "입장")
        // 초기 텍스트 패턴을 분석하여 방이름, 인원수, 버튼 텍스트를 100% 올바른 위젯에 매핑합니다!
        UTextBlock* T1 = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(FName(TEXT("Text_ServerName"))));
        UTextBlock* T2 = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(FName(TEXT("Text_PlayerCount"))));
        UTextBlock* T3 = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(FName(TEXT("Text_Join"))));

        UTextBlock* NameWidget = nullptr;
        UTextBlock* CountWidget = nullptr;
        UTextBlock* BtnWidget = nullptr;

        TArray<UTextBlock*> AllBlocks;
        if (T1) AllBlocks.Add(T1);
        if (T2) AllBlocks.Add(T2);
        if (T3) AllBlocks.Add(T3);

        for (UTextBlock* TB : AllBlocks)
        {
            FString Raw = TB->GetText().ToString().TrimStartAndEnd();
            if (Raw.Contains(TEXT("입장")) || Raw.Contains(TEXT("참가")) || Raw.Contains(TEXT("Join")))
            {
                BtnWidget = TB;
            }
            else if (Raw.Contains(TEXT("/")) || Raw.Contains(TEXT("명")) || (Raw.IsNumeric() && Raw.Len() <= 2))
            {
                CountWidget = TB;
            }
            else if (Raw.Contains(TEXT("탐정")) || Raw.Contains(TEXT("모임")) || Raw.Contains(TEXT("방")))
            {
                NameWidget = TB;
            }
        }

        // 초기 텍스트로 확정되지 않은 경우 최적 폴백
        if (!NameWidget)
        {
            NameWidget = (T2 && T2 != BtnWidget && T2 != CountWidget) ? T2 :
                         ((T1 && T1 != BtnWidget && T1 != CountWidget) ? T1 : T2);
        }
        if (!CountWidget)
        {
            CountWidget = (T3 && T3 != NameWidget && T3 != BtnWidget) ? T3 :
                          ((T2 && T2 != NameWidget && T2 != BtnWidget) ? T2 : T3);
        }
        if (!BtnWidget)
        {
            BtnWidget = (T1 && T1 != NameWidget && T1 != CountWidget) ? T1 :
                        ((T3 && T3 != NameWidget && T3 != CountWidget) ? T3 : nullptr);
        }

        FString RoomTitle = GetFoundRoomName(i);
        int32 PlayerCount = GetFoundRoomPlayerCount(i);

        if (NameWidget)
        {
            NameWidget->SetText(FText::FromString(RoomTitle));
        }
        if (CountWidget)
        {
            CountWidget->SetText(FText::FromString(FString::Printf(TEXT("%d / 6"), PlayerCount)));
        }
        if (BtnWidget)
        {
            BtnWidget->SetText(FText::FromString(TEXT("입장")));
        }

        // 3. ScrollList에 엔트리 추가
        ScrollList->AddChild(EntryWidget);
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Added server entry [%d]: '%s' (%d/6)"), i, *RoomTitle, PlayerCount);
    }
}

void UTOGameInstance::JoinFoundSession(int32 Index)
{
    // [하마치/직접접속 가상 인덱스 처리]
    if (Index == 9999)
    {
        FString SavedIP;
        if (GConfig)
        {
            GConfig->GetString(TEXT("Operator"), TEXT("LastHostIP"), SavedIP, GGameIni);
        }
        if (!SavedIP.IsEmpty())
        {
            UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Joining saved host IP from server list: %s"), *SavedIP);
            JoinServerByIP(SavedIP);
            return;
        }
    }

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

    // 공개 방 목록에서 참가 시에도 Input_Nickname / ETB_PlayerName 확인하여 PlayerName 업데이트
    UpdatePlayerNameFromActiveMenu();

    JoinSessionInternal(SessionSearch->SearchResults[Index]);
}

void UTOGameInstance::JoinSessionInternal(const FOnlineSessionSearchResult& Result)
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] SessionInterface is invalid in JoinSessionInternal."));
        return;
    }

    if (!Result.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] Search result is invalid in JoinSessionInternal."));
        return;
    }

    // 기존 세션이 남아있다면 반드시 비동기 파기 완료 후 참가 (AlreadyInSession 오류 방지)
    auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
    if (ExistingSession != nullptr)
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Existing session found. Destroying session first before joining new session..."));
        PendingSessionAction = EPendingSessionAction::Join;
        PendingJoinSearchResult = Result;
        if (!SessionInterface->DestroySession(NAME_GameSession))
        {
            UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] DestroySession call failed synchronously. Joining session anyway..."));
            PendingSessionAction = EPendingSessionAction::None;
            LastJoinedSearchResult = Result;
            SessionInterface->JoinSession(0, NAME_GameSession, Result);
        }
        return;
    }

    LastJoinedSearchResult = Result;
    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Joining session: %s"), *Result.GetSessionIdStr());
    SessionInterface->JoinSession(
        0,
        NAME_GameSession,
        Result
    );
}

void UTOGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] OnDestroySessionComplete: %s (Success: %s)"),
        *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));

    EPendingSessionAction Action = PendingSessionAction;
    PendingSessionAction = EPendingSessionAction::None;

    if (Action == EPendingSessionAction::Create)
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Executing pending CreateSession..."));
        if (SessionInterface.IsValid())
        {
            SessionInterface->CreateSession(0, NAME_GameSession, PendingSessionSettings);
        }
    }
    else if (Action == EPendingSessionAction::Join)
    {
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Executing pending JoinSession..."));
        if (SessionInterface.IsValid() && PendingJoinSearchResult.IsValid())
        {
            LastJoinedSearchResult = PendingJoinSearchResult;
            SessionInterface->JoinSession(0, NAME_GameSession, PendingJoinSearchResult);
        }
    }
}

void UTOGameInstance::OnSessionUserInviteAccepted(
    const bool bWasSuccessful,
    const int32 ControllerId,
    FUniqueNetIdPtr UserId,
    const FOnlineSessionSearchResult& InviteResult
)
{
    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] OnSessionUserInviteAccepted - Success: %s, Valid: %s"),
        bWasSuccessful ? TEXT("true") : TEXT("false"),
        InviteResult.IsValid() ? TEXT("true") : TEXT("false"));

    if (bWasSuccessful && InviteResult.IsValid())
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("스팀 친구 초대를 수락했습니다. 세션에 접속합니다..."));
        }
        UpdatePlayerNameFromActiveMenu();
        JoinSessionInternal(InviteResult);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] OnSessionUserInviteAccepted failed or invite result is invalid (Success: %s, Valid: %s)"),
            bWasSuccessful ? TEXT("true") : TEXT("false"),
            InviteResult.IsValid() ? TEXT("true") : TEXT("false"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Red, TEXT("스팀 친구 초대를 수락할 수 없습니다 (방이 종료되었거나 유효하지 않은 초대)."));
        }
    }
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

    FoundRoomName = DecodeRoomNameFromSession(FoundRoomName);

    if (FoundRoomName.IsEmpty())
    {
        FoundRoomName = FString::Printf(TEXT("Room #%d"), Index + 1);
    }

    FString MatchType;
    Result.Session.SessionSettings.Get(FName(TEXT("MATCH_TYPE")), MatchType);
    FString Pass;
    Result.Session.SessionSettings.Get(FName(TEXT("ROOM_PASSWORD")), Pass);

    bool bIsPrivate = MatchType.Equals(TEXT("Private"), ESearchCase::IgnoreCase) ||
                      (!Pass.IsEmpty() && !Pass.Equals(TEXT("NONE"), ESearchCase::IgnoreCase));

    if (bIsPrivate)
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

    int32 MaxPlayers = Result.Session.SessionSettings.NumPublicConnections > 0 ? Result.Session.SessionSettings.NumPublicConnections : 6;
    int32 OpenConnections = Result.Session.NumOpenPublicConnections;
    int32 CurrentPlayers = MaxPlayers - OpenConnections;

    // 방을 개설한 호스트가 항상 방에 상주하고 있으므로 최소 1명 이상이어야 합니다.
    // Steam 매치메이킹 응답 지연으로 OpenConnections가 6으로 와서 6 - 6 = 0이 되더라도 최소 1명 보장!
    return FMath::Clamp(CurrentPlayers, 1, MaxPlayers);
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

    // 접속한 IP 주소 영구 저장 (포트 번호 제외한 순수 IP)
    FString PureIP = CleanAddress;
    int32 ColonIdx;
    if (PureIP.FindChar(':', ColonIdx))
    {
        PureIP = PureIP.Left(ColonIdx);
    }
    if (GConfig)
    {
        GConfig->SetString(TEXT("Operator"), TEXT("LastHostIP"), *PureIP, GGameIni);
        GConfig->Flush(false, GGameIni);
        UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] Saved LastHostIP to config: %s"), *PureIP);
    }


    if (!CleanAddress.Contains(TEXT(":")))
    {
        CleanAddress += TEXT(":7777");
    }

    UE_LOG(LogTemp, Log, TEXT("[TOGameInstance] JoinServerByIP: Connecting to %s"), *CleanAddress);

    if (APlayerController* PC = GetFirstLocalPlayerController())
    {
        FString TravelURL = CleanAddress;
        if (!PlayerName.IsEmpty())
        {
            TravelURL += FString::Printf(TEXT("?Name=%s"), *PlayerName);
        }
        PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[TOGameInstance] JoinServerByIP: Failed to get FirstLocalPlayerController."));
    }
}