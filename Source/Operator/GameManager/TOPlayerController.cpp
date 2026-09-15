#include "TOPlayerController.h"
#include "TOPlayerState.h"
#include "TOGameMode.h"
#include "TOGameState.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#include "../Character/TOCharacter.h"
#include "TOGameInstance.h"
#include "Components/Button.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"


ATOPlayerController::ATOPlayerController()
{
	bReplicates = true;
}


void ATOPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	// 로컬 플레이어(실제 내 화면/스피커)인 경우에만 대기실 BGM 재생
	if (IsLocalController() && WaitingBGMSound)
	{
		WaitingBGMComponent = UGameplayStatics::SpawnSound2D(this, WaitingBGMSound);
	}

	if (!IsLocalController()) return;

	// GameInstance에서 닉네임과 커스터마이징 정보 읽어서 서버로 동기화 전송
	if (UTOGameInstance* TOGI = Cast<UTOGameInstance>(GetGameInstance()))
	{
		Server_SetPlayerName(TOGI->GetPlayerName());
		Server_SetCustomization(TOGI->SelectedHairIndex, TOGI->SelectedTopIndex, TOGI->SelectedBottomIndex);

		if (ATOCharacter* LocalChar = Cast<ATOCharacter>(GetPawn()))
		{
			LocalChar->ApplyCustomization(TOGI->SelectedHairIndex, TOGI->SelectedTopIndex, TOGI->SelectedBottomIndex);
			LocalChar->UpdateNameTagWidget(TOGI->GetPlayerName());
		}
	}

	// Slate의 기본 Tab 키 UI 네비게이션을 비활성화하여 Tab 키가 항상 게임/컨트롤러 입력으로 전달되도록 설정
	if (FSlateApplication::IsInitialized())
	{
		TSharedRef<FNavigationConfig> NavConfig = MakeShared<FNavigationConfig>();
		NavConfig->bTabNavigation = false;
		FSlateApplication::Get().SetNavigationConfig(NavConfig);
	}

	// 레벨에 진입하면 WBP_InGameMain을 최상위 뷰포트에 생성하여 항시 유지 (Z-Order 10으로 설정하여 서브 위젯 위에 표시)
	if (InGameMainWidgetClass && !CurrentInGameMainWidget)
	{
		CurrentInGameMainWidget = CreateWidget<UUserWidget>(this, InGameMainWidgetClass);
		if (CurrentInGameMainWidget)
		{
			CurrentInGameMainWidget->AddToViewport(10);
			SetupChatInputBox();
		}
	}

	// 초기 상태는 로비(대기) 상태이므로 WBP_WaitingGame을 서브 위젯 영역에 생성
	Client_OnGameEnded_Implementation();

	// 초기 HUD 상태(SelfHitTestInvisible) 및 Input Mode(GameAndUI) 명시적 초기화
	SetHUDVisible(true);

	// 대기실 환경설정 버튼 등 자동 바인딩
	SetupWaitingGameBindings();
}

void ATOPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
	{
		if (bHasCachedPlayerName)
		{
			TOPS->SetPlayerNameString(CachedPlayerName);
		}
	}

	if (ATOCharacter* TOChar = Cast<ATOCharacter>(InPawn))
	{
		if (bHasCachedCustomization)
		{
			TOChar->ApplyCustomization(CachedHairIndex, CachedTopIndex, CachedBottomIndex);
		}
		else if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
		{
			TOChar->ApplyCustomization(TOPS->SelectedHairIndex, TOPS->SelectedTopIndex, TOPS->SelectedBottomIndex);
		}

		if (bHasCachedPlayerName)
		{
			TOChar->UpdateNameTagWidget(CachedPlayerName);
		}
		else if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
		{
			TOChar->UpdateNameTagWidget(TOPS->GetCustomPlayerName());
		}
	}
}

void ATOPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	if (ATOCharacter* TOChar = Cast<ATOCharacter>(P))
	{
		if (UTOGameInstance* TOGI = Cast<UTOGameInstance>(GetGameInstance()))
		{
			TOChar->ApplyCustomization(TOGI->SelectedHairIndex, TOGI->SelectedTopIndex, TOGI->SelectedBottomIndex);
			TOChar->UpdateNameTagWidget(TOGI->GetPlayerName());
		}
	}
}




// AssignedPlayerIndex 변수를 모든 클라이언트에 복제하도록 설정하는 로직
void ATOPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATOPlayerController, AssignedPlayerIndex);
}

void ATOPlayerController::Client_SwitchToInGameBGM_Implementation()
{
	// 1. 대기실 BGM 정지
	if (WaitingBGMComponent && WaitingBGMComponent->IsPlaying())
	{
		WaitingBGMComponent->Stop();
	}

	// 2. 인게임 BGM 재생
	if (InGameBGMSound && IsLocalController())
	{
		UGameplayStatics::PlaySound2D(this, InGameBGMSound);
	}
}

// IA 입력으로 호출되는 HUD 토글 함수
void ATOPlayerController::ToggleHUD()
{
	// 현재 가시성 상태의 반대로 토글
	SetHUDVisible(!bIsHUDVisible);
}


// HUD On/Off에 따른 Input Mode & Cursor 변경 함수
void ATOPlayerController::SetHUDVisible(bool bVisible)
{
	bIsHUDVisible = bVisible;

	const ESlateVisibility TargetVisibility = bIsHUDVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;

	if (CurrentInGameMainWidget)
	{
		CurrentInGameMainWidget->SetVisibility(TargetVisibility);
	}

	if (CurrentSubWidget)
	{
		CurrentSubWidget->SetVisibility(TargetVisibility);
	}

	bShowMouseCursor = bIsHUDVisible;

	if (bIsHUDVisible)
	{
		// UI 상호작용 및 게임 입력을 동시에 받도록 설정
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);

		SetupChatInputBox();
	}
	else
	{
		// [HUD Off] 시야 조작 가능 + 마우스 커서 제거
		SetInputMode(FInputModeGameOnly());
	}
}

bool ATOPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Event == IE_Pressed)
	{
		bool bIsTyping = false;
		if (FSlateApplication::IsInitialized())
		{
			TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
			if (FocusedWidget.IsValid())
			{
				const FString TypeName = FocusedWidget->GetTypeAsString();
				if (TypeName == TEXT("SEditableText") || TypeName == TEXT("SMultiLineEditableText") || TypeName == TEXT("SEditableTextBox"))
				{
					bIsTyping = true;
				}
			}
		}

		if (bIsTyping)
		{
			// 채팅 입력 중 ESC 키를 누르면 채팅창 포커스 해제
			if (Params.Key == EKeys::Escape)
			{
				UnfocusChatInput();
				return true;
			}
		}
		else
		{
			// Enter 키 입력 시 즉시 채팅창 포커스 활성화
			if (Params.Key == EKeys::Enter)
			{
				FocusChatInput();
				return true;
			}
			else if (Params.Key == EKeys::Tab)
			{
				ToggleHUD();
				return true;
			}
			else if (Params.Key == EKeys::B)
			{
				if (ATOCharacter* Char = Cast<ATOCharacter>(GetPawn()))
				{
					Char->TogglePerspective();
					return true;
				}
			}
		}
	}

	return Super::InputKey(Params);
}

void ATOPlayerController::Multicast_UpdateMainUI_Implementation()
{
	if (HasAuthority())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ATOPlayerController* PC = Cast<ATOPlayerController>(It->Get()))
			{
				PC->Client_UpdateMainUI();
			}
		}
	}
	else if (IsLocalController())
	{
		K2_UpdateMainUI();
	}
}

void ATOPlayerController::Client_UpdateMainUI_Implementation()
{
	if (IsLocalController())
	{
		K2_UpdateMainUI();
	}
}

void ATOPlayerController::Multicast_ShowCorrectNotice_Implementation(int32 WinnerPlayerIndex)
{
	if (HasAuthority())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ATOPlayerController* PC = Cast<ATOPlayerController>(It->Get()))
			{
				PC->Client_ShowCorrectNotice(WinnerPlayerIndex);
			}
		}
	}
	else if (IsLocalController())
	{
		K2_ShowCorrectNotice(WinnerPlayerIndex);
	}
}

void ATOPlayerController::Client_ShowCorrectNotice_Implementation(int32 WinnerPlayerIndex)
{
	if (IsLocalController())
	{
		K2_ShowCorrectNotice(WinnerPlayerIndex);
	}
}


//  서버로 Ready 상태 전송
void ATOPlayerController::Server_SetReady_Implementation(bool bReady)
{
	ATOGameMode* GameMode = GetWorld()->GetAuthGameMode<ATOGameMode>();
	if (GameMode)
	{
		GameMode->SetPlayerReady(this, bReady);
	}
}


// 클라이언트가 선택한 캐릭터 ID를 서버로 전송하여 처리하는 로직
void ATOPlayerController::Server_SelectCharacter_Implementation(int32 CharacterID)
{
	ATOGameMode* GameMode = GetWorld()->GetAuthGameMode<ATOGameMode>();
	if (GameMode)
	{
		// 추후에 구현 예정 (서버 GameMode에 선택한 캐릭터 ID 반영)
	}
	
	// PlayerState를 가져와서 선택한 캐릭터 ID 저장
	if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
	{
		TOPS->SelectedCharacterID = CharacterID;
	}
}

void ATOPlayerController::Server_SetCustomization_Implementation(int32 InHairIndex, int32 InTopIndex, int32 InBottomIndex)
{
	CachedHairIndex = InHairIndex;
	CachedTopIndex = InTopIndex;
	CachedBottomIndex = InBottomIndex;
	bHasCachedCustomization = true;

	if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
	{
		TOPS->SetCustomization(InHairIndex, InTopIndex, InBottomIndex);
	}
	if (ATOCharacter* TOChar = Cast<ATOCharacter>(GetPawn()))
	{
		TOChar->ApplyCustomization(InHairIndex, InTopIndex, InBottomIndex);
	}
}

void ATOPlayerController::Server_SetPlayerName_Implementation(const FString& InPlayerName)
{
	CachedPlayerName = InPlayerName;
	bHasCachedPlayerName = true;

	if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
	{
		TOPS->SetPlayerNameString(InPlayerName);
	}

	if (ATOCharacter* TOChar = Cast<ATOCharacter>(GetPawn()))
	{
		TOChar->UpdateNameTagWidget(InPlayerName);
	}
}

void ATOPlayerController::InitPlayerState()
{
	Super::InitPlayerState();

	if (HasAuthority())
	{
		if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
		{
			if (bHasCachedCustomization)
			{
				TOPS->SetCustomization(CachedHairIndex, CachedTopIndex, CachedBottomIndex);
			}
			if (bHasCachedPlayerName)
			{
				TOPS->SetPlayerNameString(CachedPlayerName);
			}
		}
	}
}



// 제출 버튼 클릭 시 호출되는 통합 RPC 구현
void ATOPlayerController::Server_SubmitCurrentInput_Implementation(const FTOGuessAllInputData& FormulaData, const FTOGuessSingleInputData& SingleGuessData)
{
	ATOGameMode* GameMode = GetWorld()->GetAuthGameMode<ATOGameMode>();
	if (GameMode)
	{
		// GameMode가 현재 페이즈(정답 제출/카드 유추 제출)에 맞춰 알맞은 입력 데이터를 처리하도록 전달
		GameMode->SubmitPlayerInput(this, FormulaData, SingleGuessData);
	}
}


void ATOPlayerController::Client_UpdateLobbyState_Implementation(bool bCanEnableReady)
{
	// Lobby UI의 Ready 버튼 활성화 상태 구현 예정
}


void ATOPlayerController::Client_UpdateFormulaUI_Implementation(const FTOPlayerUIData& NewUIData)
{
	// 페이즈 변경에 따른 StartGame 위젯 가시성 제어
	UpdateStartGamePhaseVisibility(NewUIData.CurrentPhase);

	K2_OnUpdateFormulaUI(NewUIData);
}


void ATOPlayerController::Client_OnGameStarted_Implementation()
{
	if (!IsLocalController() || !CurrentInGameMainWidget) return;

	// 기존 서브 위젯(WaitingGame) 제거
	if (CurrentSubWidget)
	{
		CurrentSubWidget->RemoveFromParent();
		CurrentSubWidget = nullptr;
	}
	
	// StartGame 위젯을 Viewport에 직접 생성하여 덮어 씌움 (Z-Order 0: InGameMainWidget(10) 뒤에 배치)
	if (StartGameWidgetClass)
	{
		CurrentSubWidget = CreateWidget<UUserWidget>(this, StartGameWidgetClass);
		if (CurrentSubWidget)
		{
			CurrentSubWidget->AddToViewport(0); 
			CurrentSubWidget->SetVisibility(bIsHUDVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

			// 게임 시작 시 기본 페이즈 1(수식 제출)이므로 버튼 및 텍스트 블록 숨김 처리
			UpdateStartGamePhaseVisibility(ETOGamePhase::SubmittingFormulas);
		}
	}
}

void ATOPlayerController::Client_OnGameEnded_Implementation()
{
	if (!IsLocalController() || !CurrentInGameMainWidget) return;

	// 기존 서브 위젯(StartGame) 제거
	if (CurrentSubWidget)
	{
		CurrentSubWidget->RemoveFromParent();
		CurrentSubWidget = nullptr;
	}

	// WaitingGame 위젯을 Viewport에 직접 생성하여 덮어 씌움 (Z-Order 0: InGameMainWidget(10) 뒤에 배치)
	if (WaitingGameWidgetClass)
	{
		CurrentSubWidget = CreateWidget<UUserWidget>(this, WaitingGameWidgetClass);
		if (CurrentSubWidget)
		{
			CurrentSubWidget->AddToViewport(0);
			CurrentSubWidget->SetVisibility(bIsHUDVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

			// 대기실 환경설정 버튼 등 자동 바인딩
			SetupWaitingGameBindings();
		}
	}
}

void Client_ReceiveGuessResult(bool bIsMatch, const FString& TargetAlphabet, int32 RevealedVal)
	{
		
	}
	
// 클라이언트가 입력한 메시지를 서버로 전송
void ATOPlayerController::Server_SendChatMessage_Implementation(const FString& Message)
{
	FString TrimmedMessage = Message.TrimStartAndEnd();
	if (TrimmedMessage.IsEmpty()) return;
    
	// 닉네임 추출
	FString SenderName = TEXT("Unknown");
	if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
	{
		SenderName = TOPS->GetCustomPlayerName();
		if (SenderName.IsEmpty())
		{
			SenderName = FString::Printf(TEXT("Player %d"), TOPS->GetAssignedPlayerIndex());
		}
	}
	
	// 서버에 접속된 클라이언트들이 각각 멀티캐스트를 호출
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ATOPlayerController* TargetPC = Cast<ATOPlayerController>(It->Get()))
		{
			TargetPC->Multicast_BroadcastChatMessage(SenderName, TrimmedMessage);
		}
	}
}


// 서버가 모든 클라이언트에게 채팅 메시지를 전파
void ATOPlayerController::Multicast_BroadcastChatMessage_Implementation(const FString& SenderName, const FString& Message)
{
	if (IsLocalController())
	{
		K2_AddChatMessageToUI(SenderName, Message);
	}
}

UEditableTextBox* ATOPlayerController::FindChatInputBox() const
{
	if (!CurrentInGameMainWidget) return nullptr;

	TFunction<UEditableTextBox*(UUserWidget*)> SearchWidgetTree;
	SearchWidgetTree = [&SearchWidgetTree](UUserWidget* InWidget) -> UEditableTextBox*
	{
		if (!InWidget || !InWidget->WidgetTree) return nullptr;

		UEditableTextBox* Fallback = nullptr;
		TArray<UWidget*> AllWidgets;
		InWidget->WidgetTree->GetAllWidgets(AllWidgets);

		for (UWidget* Widget : AllWidgets)
		{
			if (UEditableTextBox* Box = Cast<UEditableTextBox>(Widget))
			{
				const FString BoxName = Box->GetName().ToLower();
				if (BoxName.Contains(TEXT("chat")) || BoxName.Contains(TEXT("msg")) || BoxName.Contains(TEXT("input")))
				{
					return Box;
				}
				if (!Fallback)
				{
					Fallback = Box;
				}
			}
			else if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(Widget))
			{
				if (UEditableTextBox* FoundInChild = SearchWidgetTree(ChildUserWidget))
				{
					return FoundInChild;
				}
			}
		}
		return Fallback;
	};

	return SearchWidgetTree(CurrentInGameMainWidget);
}

void ATOPlayerController::SetupChatInputBox()
{
	if (UEditableTextBox* ChatBox = FindChatInputBox())
	{
		ChatBox->SetVisibility(ESlateVisibility::Visible);
		ChatBox->SetIsEnabled(true);
		ChatBox->SetIsReadOnly(false);
		ChatBox->OnTextCommitted.RemoveDynamic(this, &ATOPlayerController::HandleChatCommitted);
		ChatBox->OnTextCommitted.AddDynamic(this, &ATOPlayerController::HandleChatCommitted);
	}
}

void ATOPlayerController::FocusChatInput()
{
	SetupChatInputBox();

	if (UEditableTextBox* ChatBox = FindChatInputBox())
	{
		ChatBox->SetVisibility(ESlateVisibility::Visible);
		ChatBox->SetIsEnabled(true);
		ChatBox->SetIsReadOnly(false);
		ChatBox->SetUserFocus(this);
		ChatBox->SetKeyboardFocus();

		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(ChatBox->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
}

void ATOPlayerController::UnfocusChatInput()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ATOPlayerController::HandleChatCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		const FString Message = Text.ToString().TrimStartAndEnd();
		if (!Message.IsEmpty())
		{
			Server_SendChatMessage(Message);
		}

		if (UEditableTextBox* ChatBox = FindChatInputBox())
		{
			ChatBox->SetText(FText::GetEmpty());
		}

		UnfocusChatInput();
	}
	else if (CommitMethod == ETextCommit::OnCleared)
	{
		UnfocusChatInput();
	}
}


// 단일카드 유추 했을 때 호출
void ATOPlayerController::Client_ReceiveGuessResult_Implementation(bool bIsMatch, const FString& TargetAlphabet, int32 RevealedVal)
{
	UE_LOG(LogTemp, Warning, TEXT("[Client RPC] bIsMatch: %s, Alphabet: %s, Val: %d"), 
		bIsMatch ? TEXT("TRUE") : TEXT("FALSE"), *TargetAlphabet, RevealedVal);
	
	K2_OnReceiveGuessResult(bIsMatch, TargetAlphabet, RevealedVal);
}

void ATOPlayerController::UpdateStartGamePhaseVisibility(ETOGamePhase Phase)
{
	if (!IsLocalController() || !CurrentSubWidget) return;

	// 페이즈 1 (SubmittingFormulas): 안 보이게 (Collapsed)
	// 페이즈 2 (GuessingCards): 보이게 (Visible)
	const ESlateVisibility TargetVisibility = (Phase == ETOGamePhase::GuessingCards) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	static const TArray<FName> TargetWidgetNames = {
		FName(TEXT("Btn_SelectNumber")),
		FName(TEXT("Btn_SelectPlayer")),
		FName(TEXT("TextBlock_5")),
		FName(TEXT("TextBlock 5")),
		FName(TEXT("TextBlock5"))
	};

	for (const FName& WidgetName : TargetWidgetNames)
	{
		if (UWidget* FoundWidget = CurrentSubWidget->GetWidgetFromName(WidgetName))
		{
			FoundWidget->SetVisibility(TargetVisibility);
		}
	}

	if (CurrentSubWidget->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		CurrentSubWidget->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* W : AllWidgets)
		{
			if (W)
			{
				const FString WName = W->GetName();
				if (WName.Equals(TEXT("Btn_SelectNumber"), ESearchCase::IgnoreCase) ||
					WName.Equals(TEXT("Btn_SelectPlayer"), ESearchCase::IgnoreCase) ||
					WName.Equals(TEXT("TextBlock_5"), ESearchCase::IgnoreCase) ||
					WName.Equals(TEXT("TextBlock5"), ESearchCase::IgnoreCase))
				{
					W->SetVisibility(TargetVisibility);
				}
			}
		}
	}
}

void ATOPlayerController::SetInGameMainWidgetVisibility(bool bVisible)
{
	if (CurrentInGameMainWidget)
	{
		CurrentInGameMainWidget->SetVisibility((bVisible && bIsHUDVisible) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void ATOPlayerController::OpenSettingsWidget(UUserWidget* SettingsWidgetInstance)
{
	if (!SettingsWidgetInstance) return;

	SetInGameMainWidgetVisibility(false);

	if (!SettingsWidgetInstance->IsInViewport())
	{
		SettingsWidgetInstance->AddToViewport(100);
	}

	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(SettingsWidgetInstance->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	MonitoredSettingsWidget = SettingsWidgetInstance;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettingsWidgetMonitorTimerHandle);
		World->GetTimerManager().SetTimer(SettingsWidgetMonitorTimerHandle, this, &ATOPlayerController::MonitorSettingsWidgetClosed, 0.05f, true);
	}
}

UUserWidget* ATOPlayerController::FindActiveSettingsWidget() const
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld) return nullptr;

	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (IsValid(Widget) && Widget->GetWorld() == CurrentWorld)
		{
			if (Widget->GetClass()->GetName().Contains(TEXT("Settings")) && Widget->IsInViewport())
			{
				return Widget;
			}
		}
	}
	return nullptr;
}

void ATOPlayerController::MonitorSettingsWidgetClosed()
{
	if (!MonitoredSettingsWidget.IsValid())
	{
		MonitoredSettingsWidget = FindActiveSettingsWidget();
	}

	// 감시 중인 위젯이 사라졌거나 뷰포트에서 제거된 경우
	if (!MonitoredSettingsWidget.IsValid() || !MonitoredSettingsWidget->IsInViewport())
	{
		// 화면에 다른 Settings 위젯이 아직 남아있는지 확인
		if (FindActiveSettingsWidget() == nullptr)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(SettingsWidgetMonitorTimerHandle);
			}
			MonitoredSettingsWidget = nullptr;

			// InGameMainWidget 다시 켜기
			SetInGameMainWidgetVisibility(true);

			// 입력 모드 복구
			if (bIsHUDVisible)
			{
				FInputModeGameAndUI InputMode;
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				InputMode.SetHideCursorDuringCapture(false);
				SetInputMode(InputMode);
				bShowMouseCursor = true;
			}
		}
	}
}

void ATOPlayerController::OnSettingsButtonClicked()
{
	SetInGameMainWidgetVisibility(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettingsWidgetMonitorTimerHandle);
		World->GetTimerManager().SetTimer(SettingsWidgetMonitorTimerHandle, this, &ATOPlayerController::MonitorSettingsWidgetClosed, 0.05f, true);
	}
}

void ATOPlayerController::SetupWaitingGameBindings()
{
	if (!CurrentSubWidget) return;

	UButton* SettingsBtn = Cast<UButton>(CurrentSubWidget->GetWidgetFromName(FName(TEXT("Btn_Settings"))));
	if (!SettingsBtn)
	{
		SettingsBtn = Cast<UButton>(CurrentSubWidget->GetWidgetFromName(FName(TEXT("Btn_Setting"))));
	}
	if (!SettingsBtn && CurrentSubWidget->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		CurrentSubWidget->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* W : AllWidgets)
		{
			if (UButton* B = Cast<UButton>(W))
			{
				const FString BName = B->GetName();
				if (BName.Contains(TEXT("Setting"), ESearchCase::IgnoreCase))
				{
					SettingsBtn = B;
					break;
				}
			}
		}
	}

	if (SettingsBtn)
	{
		SettingsBtn->OnClicked.RemoveDynamic(this, &ATOPlayerController::OnSettingsButtonClicked);
		SettingsBtn->OnClicked.AddDynamic(this, &ATOPlayerController::OnSettingsButtonClicked);
	}
}





