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
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"


ATOPlayerController::ATOPlayerController()
{
	bReplicates = true;

	static ConstructorHelpers::FClassFinder<UUserWidget> PlayerScoreWidgetFinder(TEXT("/Game/UI/InGame/SubWidget/WBP_PlayerScore.WBP_PlayerScore_C"));
	if (PlayerScoreWidgetFinder.Succeeded())
	{
		PlayerScoreWidgetClass = PlayerScoreWidgetFinder.Class;
	}
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

	// 방 입장 시 메인 UI 및 스코어보드 갱신
	Client_UpdateMainUI();

	// 방 입장 직후 PlayerState 복제 지연에 대비한 추가 갱신 타이머
	FTimerHandle UpdateMainUITimer1;
	GetWorldTimerManager().SetTimer(UpdateMainUITimer1, this, &ATOPlayerController::Client_UpdateMainUI, 0.5f, false);
	FTimerHandle UpdateMainUITimer2;
	GetWorldTimerManager().SetTimer(UpdateMainUITimer2, this, &ATOPlayerController::Client_UpdateMainUI, 1.5f, false);
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
		UpdateScoreBoardUI();
	}
}

void ATOPlayerController::Client_UpdateMainUI_Implementation()
{
	if (IsLocalController())
	{
		K2_UpdateMainUI();
		UpdateScoreBoardUI();
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
		Client_ShowCorrectNotice(WinnerPlayerIndex);
	}
}

void ATOPlayerController::Client_ShowCorrectNotice_Implementation(int32 WinnerPlayerIndex)
{
	if (IsLocalController())
	{
		K2_ShowCorrectNotice(WinnerPlayerIndex);

		const FString WinnerNickname = GetPlayerNicknameByIndex(WinnerPlayerIndex);

		auto UpdateNoticeWidgetNickname = [this, WinnerNickname]()
		{
			for (TObjectIterator<UUserWidget> It; It; ++It)
			{
				UUserWidget* Widget = *It;
				if (Widget && Widget->GetWorld() == GetWorld())
				{
					const FString WName = Widget->GetName();
					const FString CName = Widget->GetClass()->GetName();
					if (WName.Contains(TEXT("CorrectAnswerNotice")) || CName.Contains(TEXT("CorrectAnswerNotice")))
					{
						if (UTextBlock* TB = Cast<UTextBlock>(Widget->GetWidgetFromName(FName(TEXT("TB_PlayerName")))))
						{
							TB->SetText(FText::FromString(WinnerNickname));
						}
						else if (Widget->WidgetTree)
						{
							TArray<UWidget*> AllWidgets;
							Widget->WidgetTree->GetAllWidgets(AllWidgets);
							for (UWidget* W : AllWidgets)
							{
								if (UTextBlock* TextW = Cast<UTextBlock>(W))
								{
									if (TextW->GetName().Equals(TEXT("TB_PlayerName"), ESearchCase::IgnoreCase))
									{
										TextW->SetText(FText::FromString(WinnerNickname));
										break;
									}
								}
							}
						}
					}
				}
			}
		};

		// 1) 즉시 반영
		UpdateNoticeWidgetNickname();

		// 2) 다음 틱에도 한 번 더 반영 (위젯 초기화 지연 방지)
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(UpdateNoticeWidgetNickname));
		}
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

			// 이전 게임/라운드 유추 입력값 초기화
			ResetSingleGuessUI();

			// 게임 시작 시 기본 페이즈 1(수식 제출) 가시성 적용 및 숫자 카드 바인딩
			UpdateStartGamePhaseVisibility(ETOGamePhase::SubmittingFormulas);
			SetupNumberCardBindings();
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

	NumberCardButtonMap.Empty();

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
		AdjustChatTextLayout();
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
	
	ResetSingleGuessUI();
	K2_OnReceiveGuessResult(bIsMatch, TargetAlphabet, RevealedVal);
}

void ATOPlayerController::UpdateStartGamePhaseVisibility(ETOGamePhase Phase)
{
	if (!IsLocalController() || !CurrentSubWidget) return;

	// 페이즈 전환 시 이전 단일카드 유추 입력값(NumberTXT, PlayerTXT, SelectedSingleNumber 등) 초기화
	ResetSingleGuessUI();

	// Phase 1: SubmittingFormulas (정답 제출 턴 / 수식 영역 맞추기)
	// - Btn_SelectPlayer, Btn_SelectNumber, TextBlock_5: 비활성화 (Collapsed)
	// - FormulaBox: 활성화 (Visible, 버튼 인터랙션 가능)
	// - Btn_FormulaAreaClose: Visible, Btn_FormulaAreaOpen: Collapsed
	// - TargetPlayerArea: Collapsed
	//
	// Phase 2: GuessingCards (단일 카드 / 플레이어 카드 추측 라운드)
	// - Btn_SelectPlayer, Btn_SelectNumber, TextBlock_5: 활성화 (Visible, 정상 활성화 및 클릭 가능)
	// - FormulaBox: 출력은 되지만 버튼의 기능은 비활성화 상태 (HitTestInvisible, 연산식 확인 용)
	// - Btn_FormulaAreaClose: Collapsed, Btn_FormulaAreaOpen: Collapsed

	const bool bIsGuessingPhase = (Phase == ETOGamePhase::GuessingCards);

	const ESlateVisibility SelectButtonsVisibility = bIsGuessingPhase ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	const ESlateVisibility FormulaBoxVisibility = bIsGuessingPhase ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible;

	static const TArray<FName> SelectWidgetNames = {
		FName(TEXT("Btn_SelectNumber")),
		FName(TEXT("Btn_SelectPlayer")),
		FName(TEXT("TextBlock_5")),
		FName(TEXT("TextBlock 5")),
		FName(TEXT("TextBlock5"))
	};

	for (const FName& WidgetName : SelectWidgetNames)
	{
		if (UWidget* FoundWidget = CurrentSubWidget->GetWidgetFromName(WidgetName))
		{
			FoundWidget->SetVisibility(SelectButtonsVisibility);
		}
	}

	if (UWidget* FormulaArea = CurrentSubWidget->GetWidgetFromName(FName(TEXT("FormulaArea"))))
	{
		FormulaArea->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (UWidget* FormulaBox = CurrentSubWidget->GetWidgetFromName(FName(TEXT("FormulaBox"))))
	{
		FormulaBox->SetVisibility(FormulaBoxVisibility);
	}
	if (UWidget* BtnClose = CurrentSubWidget->GetWidgetFromName(FName(TEXT("Btn_FormulaAreaClose"))))
	{
		BtnClose->SetVisibility(bIsGuessingPhase ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (UWidget* BtnOpen = CurrentSubWidget->GetWidgetFromName(FName(TEXT("Btn_FormulaAreaOpen"))))
	{
		BtnOpen->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!bIsGuessingPhase)
	{
		if (UWidget* TargetPlayerArea = CurrentSubWidget->GetWidgetFromName(FName(TEXT("TargetPlayerArea"))))
		{
			TargetPlayerArea->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (CurrentSubWidget->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		CurrentSubWidget->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* W : AllWidgets)
		{
			if (!W) continue;
			const FString WName = W->GetName();
			if (WName.Equals(TEXT("Btn_SelectNumber"), ESearchCase::IgnoreCase) ||
				WName.Equals(TEXT("Btn_SelectPlayer"), ESearchCase::IgnoreCase) ||
				WName.Equals(TEXT("TextBlock_5"), ESearchCase::IgnoreCase) ||
				WName.Equals(TEXT("TextBlock5"), ESearchCase::IgnoreCase))
			{
				W->SetVisibility(SelectButtonsVisibility);
			}
			else if (WName.Equals(TEXT("FormulaArea"), ESearchCase::IgnoreCase))
			{
				W->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
			else if (WName.Equals(TEXT("FormulaBox"), ESearchCase::IgnoreCase))
			{
				W->SetVisibility(FormulaBoxVisibility);
			}
			else if (WName.Equals(TEXT("Btn_FormulaAreaClose"), ESearchCase::IgnoreCase))
			{
				W->SetVisibility(bIsGuessingPhase ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
			}
			else if (WName.Equals(TEXT("Btn_FormulaAreaOpen"), ESearchCase::IgnoreCase))
			{
				W->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	// 페이즈 전환 시 숫자 카드 자동 바인딩 갱신
	SetupNumberCardBindings();
}

FString ATOPlayerController::GetPlayerNicknameByIndex(int32 PlayerIndex) const
{
	if (UWorld* World = GetWorld())
	{
		if (AGameStateBase* GS = World->GetGameState())
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (ATOPlayerState* TOPS = Cast<ATOPlayerState>(PS))
				{
					if (TOPS->GetAssignedPlayerIndex() == PlayerIndex)
					{
						const FString Nick = TOPS->GetCustomPlayerName();
						if (!Nick.IsEmpty())
						{
							return Nick;
						}
						return TOPS->GetPlayerName();
					}
				}
			}

			if (GS->PlayerArray.IsValidIndex(PlayerIndex))
			{
				if (ATOPlayerState* TOPS = Cast<ATOPlayerState>(GS->PlayerArray[PlayerIndex]))
				{
					const FString Nick = TOPS->GetCustomPlayerName();
					if (!Nick.IsEmpty())
					{
						return Nick;
					}
					return TOPS->GetPlayerName();
				}
			}
		}
	}

	return FString::Printf(TEXT("Player %d"), PlayerIndex + 1);
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

void ATOPlayerController::SetupNumberCardBindings()
{
	if (!CurrentSubWidget || !CurrentSubWidget->WidgetTree) return;

	NumberCardButtonMap.Empty();

	TArray<UWidget*> AllWidgets;
	CurrentSubWidget->WidgetTree->GetAllWidgets(AllWidgets);

	for (UWidget* Widget : AllWidgets)
	{
		UUserWidget* UserWidget = Cast<UUserWidget>(Widget);
		if (UserWidget && UserWidget->GetClass()->GetName().Contains(TEXT("NumberCard")))
		{
			// 1. WBP_NumberCard의 OnNumberSelected 델리게이트 바인딩
			if (FMulticastInlineDelegateProperty* DelProp = CastField<FMulticastInlineDelegateProperty>(UserWidget->GetClass()->FindPropertyByName(FName(TEXT("OnNumberSelected")))))
			{
				FMulticastScriptDelegate* ScriptDel = DelProp->GetPropertyValuePtr_InContainer(UserWidget);
				if (ScriptDel)
				{
					FScriptDelegate Delegate;
					Delegate.BindUFunction(this, FName(TEXT("OnCardNumberSelected")));
					ScriptDel->AddUnique(Delegate);
				}
			}

			// 2. 내부 Button_69 (또는 첫 번째 UButton) 가져오기 및 CardNumber 프로퍼티 매핑
			int32 CardNum = 0;
			if (FIntProperty* Prop = CastField<FIntProperty>(UserWidget->GetClass()->FindPropertyByName(FName(TEXT("CardNumber")))))
			{
				CardNum = Prop->GetPropertyValue_InContainer(UserWidget);
			}

			if (UserWidget->WidgetTree)
			{
				TArray<UWidget*> CardSubWidgets;
				UserWidget->WidgetTree->GetAllWidgets(CardSubWidgets);
				for (UWidget* SubW : CardSubWidgets)
				{
					if (UButton* Btn = Cast<UButton>(SubW))
					{
						if (!NumberCardButtonMap.Contains(Btn))
						{
							NumberCardButtonMap.Add(Btn, CardNum);
							Btn->OnClicked.RemoveDynamic(this, &ATOPlayerController::OnAnyNumberCardButtonClicked);
							Btn->OnClicked.AddDynamic(this, &ATOPlayerController::OnAnyNumberCardButtonClicked);
						}
						break;
					}
				}
			}
		}
	}
}

void ATOPlayerController::OnCardNumberSelected(int32 SelectedNumber)
{
	if (!CurrentSubWidget) return;

	// WBP_StartGame의 NumberTXT 텍스트 즉시 갱신
	UTextBlock* NumberTXT = Cast<UTextBlock>(CurrentSubWidget->GetWidgetFromName(FName(TEXT("NumberTXT"))));
	if (!NumberTXT && CurrentSubWidget->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		CurrentSubWidget->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* W : AllWidgets)
		{
			if (UTextBlock* TB = Cast<UTextBlock>(W))
			{
				if (TB->GetName().Equals(TEXT("NumberTXT"), ESearchCase::IgnoreCase))
				{
					NumberTXT = TB;
					break;
				}
			}
		}
	}
	if (NumberTXT)
	{
		NumberTXT->SetText(FText::AsNumber(SelectedNumber));
	}

	// SelectedSingleNumber 프로퍼티가 있으면 갱신
	if (FIntProperty* SelProp = CastField<FIntProperty>(CurrentSubWidget->GetClass()->FindPropertyByName(FName(TEXT("SelectedSingleNumber")))))
	{
		SelProp->SetPropertyValue_InContainer(CurrentSubWidget, SelectedNumber);
	}

	// WBP_StartGame의 SelectNumber 함수가 있으면 호출하여 내부 이벤트 처리
	if (UFunction* SelectNumberFunc = CurrentSubWidget->FindFunction(FName(TEXT("SelectNumber"))))
	{
		struct FSelectNumberParams { int32 SelectedNumber; };
		FSelectNumberParams Params;
		Params.SelectedNumber = SelectedNumber;
		CurrentSubWidget->ProcessEvent(SelectNumberFunc, &Params);
	}
}

void ATOPlayerController::OnAnyNumberCardButtonClicked()
{
	if (!CurrentSubWidget) return;

	int32 ClickedCardNum = -1;
	for (auto& Pair : NumberCardButtonMap)
	{
		UButton* Btn = Pair.Key.Get();
		if (Btn && (Btn->IsHovered() || Btn->IsPressed()))
		{
			ClickedCardNum = Pair.Value;
			break;
		}
	}

	if (ClickedCardNum < 0)
	{
		for (auto& Pair : NumberCardButtonMap)
		{
			UButton* Btn = Pair.Key.Get();
			if (Btn && Btn->GetCachedWidget().IsValid() && Btn->GetCachedWidget()->IsHovered())
			{
				ClickedCardNum = Pair.Value;
				break;
			}
		}
	}

	if (ClickedCardNum >= 0)
	{
		OnCardNumberSelected(ClickedCardNum);
	}
}

void ATOPlayerController::UpdateScoreBoardUI()
{
	if (!IsLocalController() || !CurrentInGameMainWidget) return;

	UVerticalBox* PlayerScoreBox = Cast<UVerticalBox>(CurrentInGameMainWidget->GetWidgetFromName(FName(TEXT("PlayerScore"))));
	if (!PlayerScoreBox && CurrentInGameMainWidget->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		CurrentInGameMainWidget->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* W : AllWidgets)
		{
			if (UVerticalBox* VB = Cast<UVerticalBox>(W))
			{
				if (VB->GetName().Equals(TEXT("PlayerScore"), ESearchCase::IgnoreCase))
				{
					PlayerScoreBox = VB;
					break;
				}
			}
		}
	}

	if (!PlayerScoreBox) return;

	UWorld* World = GetWorld();
	if (!World) return;

	AGameStateBase* GS = World->GetGameState();
	if (!GS) return;

	if (!PlayerScoreWidgetClass)
	{
		PlayerScoreWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/InGame/SubWidget/WBP_PlayerScore.WBP_PlayerScore_C"));
	}

	// 유효한 ATOPlayerState 수집
	TArray<ATOPlayerState*> Players;
	for (APlayerState* RawPS : GS->PlayerArray)
	{
		if (ATOPlayerState* TOPS = Cast<ATOPlayerState>(RawPS))
		{
			Players.Add(TOPS);
		}
	}

	// AssignedPlayerIndex 순으로 정렬 (0, 1, 2...)
	Players.Sort([](const ATOPlayerState& A, const ATOPlayerState& B) {
		return A.GetAssignedPlayerIndex() < B.GetAssignedPlayerIndex();
	});

	// 각 플레이어별로 WBP_PlayerScore 갱신 또는 추가
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		ATOPlayerState* PS = Players[i];
		if (!PS) continue;

		FString Nickname = PS->GetCustomPlayerName();
		if (Nickname.IsEmpty())
		{
			Nickname = PS->GetPlayerName();
		}
		if (Nickname.IsEmpty())
		{
			Nickname = FString::Printf(TEXT("Player %d"), PS->GetAssignedPlayerIndex() + 1);
		}
		const int32 Score = PS->GetPlayerScore();

		UUserWidget* ChildWidget = nullptr;
		if (i < PlayerScoreBox->GetChildrenCount())
		{
			ChildWidget = Cast<UUserWidget>(PlayerScoreBox->GetChildAt(i));
		}
		else if (PlayerScoreWidgetClass)
		{
			ChildWidget = CreateWidget<UUserWidget>(this, PlayerScoreWidgetClass);
			if (ChildWidget)
			{
				PlayerScoreBox->AddChild(ChildWidget);
			}
		}

		if (ChildWidget)
		{
			UTextBlock* PlayerText = Cast<UTextBlock>(ChildWidget->GetWidgetFromName(FName(TEXT("PlayerText"))));
			UTextBlock* ScoreText = Cast<UTextBlock>(ChildWidget->GetWidgetFromName(FName(TEXT("ScoreText"))));

			if (!PlayerText || !ScoreText)
			{
				if (ChildWidget->WidgetTree)
				{
					TArray<UWidget*> ChildWidgets;
					ChildWidget->WidgetTree->GetAllWidgets(ChildWidgets);
					for (UWidget* W : ChildWidgets)
					{
						if (UTextBlock* TB = Cast<UTextBlock>(W))
						{
							if (!PlayerText && TB->GetName().Equals(TEXT("PlayerText"), ESearchCase::IgnoreCase))
							{
								PlayerText = TB;
							}
							else if (!ScoreText && TB->GetName().Equals(TEXT("ScoreText"), ESearchCase::IgnoreCase))
							{
								ScoreText = TB;
							}
						}
					}
				}
			}

			if (PlayerText)
			{
				PlayerText->SetText(FText::FromString(Nickname));
			}
			if (ScoreText)
			{
				ScoreText->SetText(FText::AsNumber(Score));
			}

			// 혹시 WBP_PlayerScore 자체에 SetPlayerScoreInfo 블루프린트 함수가 있다면 그것도 호출
			if (UFunction* SetInfoFunc = ChildWidget->FindFunction(FName(TEXT("SetPlayerScoreInfo"))))
			{
				struct FPlayerScoreParams
				{
					FText PlayerName;
					int32 InScore;
				};
				FPlayerScoreParams Params;
				Params.PlayerName = FText::FromString(Nickname);
				Params.InScore = Score;
				ChildWidget->ProcessEvent(SetInfoFunc, &Params);
			}
		}
	}

	// 불필요한 남은 위젯 제거
	while (PlayerScoreBox->GetChildrenCount() > Players.Num())
	{
		PlayerScoreBox->RemoveChildAt(PlayerScoreBox->GetChildrenCount() - 1);
	}
}

void ATOPlayerController::ResetSingleGuessUI()
{
	if (!IsLocalController() || !CurrentSubWidget) return;

	// 1. NumberTXT 텍스트 초기화
	UTextBlock* NumberTXT = Cast<UTextBlock>(CurrentSubWidget->GetWidgetFromName(FName(TEXT("NumberTXT"))));
	if (!NumberTXT && CurrentSubWidget->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		CurrentSubWidget->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* W : AllWidgets)
		{
			if (UTextBlock* TB = Cast<UTextBlock>(W))
			{
				if (TB->GetName().Equals(TEXT("NumberTXT"), ESearchCase::IgnoreCase))
				{
					NumberTXT = TB;
					break;
				}
			}
		}
	}
	if (NumberTXT)
	{
		NumberTXT->SetText(FText::GetEmpty());
	}

	// 2. PlayerTXT 텍스트 초기화
	UTextBlock* PlayerTXT = Cast<UTextBlock>(CurrentSubWidget->GetWidgetFromName(FName(TEXT("PlayerTXT"))));
	if (!PlayerTXT && CurrentSubWidget->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		CurrentSubWidget->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* W : AllWidgets)
		{
			if (UTextBlock* TB = Cast<UTextBlock>(W))
			{
				if (TB->GetName().Equals(TEXT("PlayerTXT"), ESearchCase::IgnoreCase))
				{
					PlayerTXT = TB;
					break;
				}
			}
		}
	}
	if (PlayerTXT)
	{
		PlayerTXT->SetText(FText::GetEmpty());
	}

	// 3. SelectedSingleNumber / SelectedNumber 프로퍼티 -1로 초기화
	if (FIntProperty* SelProp = CastField<FIntProperty>(CurrentSubWidget->GetClass()->FindPropertyByName(FName(TEXT("SelectedSingleNumber")))))
	{
		SelProp->SetPropertyValue_InContainer(CurrentSubWidget, -1);
	}
	if (FIntProperty* SelProp2 = CastField<FIntProperty>(CurrentSubWidget->GetClass()->FindPropertyByName(FName(TEXT("SelectedNumber")))))
	{
		SelProp2->SetPropertyValue_InContainer(CurrentSubWidget, -1);
	}

	// 4. SelectedAlphabet 프로퍼티 빈 문자열로 초기화
	if (FStrProperty* AlphaProp = CastField<FStrProperty>(CurrentSubWidget->GetClass()->FindPropertyByName(FName(TEXT("SelectedAlphabet")))))
	{
		AlphaProp->SetPropertyValue_InContainer(CurrentSubWidget, FString());
	}
}

void ATOPlayerController::AdjustChatTextLayout()
{
	if (!IsLocalController() || !CurrentInGameMainWidget) return;

	UVerticalBox* ChattingBox = Cast<UVerticalBox>(CurrentInGameMainWidget->GetWidgetFromName(FName(TEXT("ChattingBox"))));
	if (!ChattingBox && CurrentInGameMainWidget->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		CurrentInGameMainWidget->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* W : AllWidgets)
		{
			if (UVerticalBox* VB = Cast<UVerticalBox>(W))
			{
				if (VB->GetName().Equals(TEXT("ChattingBox"), ESearchCase::IgnoreCase))
				{
					ChattingBox = VB;
					break;
				}
			}
		}
	}

	if (!ChattingBox) return;

	// 최대 50개 유지
	while (ChattingBox->GetChildrenCount() > 50)
	{
		ChattingBox->RemoveChildAt(0);
	}

	// 모든 채팅 메시지 위젯의 폰트 크기를 살짝 축소하고 AutoWrapText 활성화
	for (int32 i = 0; i < ChattingBox->GetChildrenCount(); ++i)
	{
		if (UUserWidget* MsgWidget = Cast<UUserWidget>(ChattingBox->GetChildAt(i)))
		{
			if (MsgWidget->WidgetTree)
			{
				TArray<UWidget*> AllWidgets;
				MsgWidget->WidgetTree->GetAllWidgets(AllWidgets);
				for (UWidget* W : AllWidgets)
				{
					if (UTextBlock* TB = Cast<UTextBlock>(W))
					{
						TB->SetAutoWrapText(true);
						FSlateFontInfo FontInfo = TB->GetFont();
						if (FontInfo.Size > 12.0f)
						{
							FontInfo.Size = 12.0f;
							TB->SetFont(FontInfo);
						}
					}
				}
			}
		}
	}
}
