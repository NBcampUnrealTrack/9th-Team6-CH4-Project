#include "TOPlayerController.h"
#include "TOPlayerState.h"
#include "TOGameMode.h"
#include "TOGameState.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"


ATOPlayerController::ATOPlayerController()
{
	bReplicates = true;
}


void ATOPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController()) return;

	// 레벨에 진입하면 WBP_InGameMain을 최상위 뷰포트에 생성하여 항시 유지
	if (InGameMainWidgetClass && !CurrentInGameMainWidget)
	{
		CurrentInGameMainWidget = CreateWidget<UUserWidget>(this, InGameMainWidgetClass);
		if (CurrentInGameMainWidget)
		{
			CurrentInGameMainWidget->AddToViewport();
			bShowMouseCursor = true;
		}
	}

	// 초기 상태는 로비(대기) 상태이므로 WBP_WaitingGame을 서브 위젯 영역에 생성
	Client_OnGameEnded_Implementation();
}


// AssignedPlayerIndex 변수를 모든 클라이언트에 복제하도록 설정하는 로직
void ATOPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATOPlayerController, AssignedPlayerIndex);
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

	if (!CurrentInGameMainWidget) return;

	if (bIsHUDVisible)
	{
		if (CurrentInGameMainWidget)
		{
			CurrentInGameMainWidget->SetVisibility(ESlateVisibility::Visible);
		}
		
		if (CurrentSubWidget)
		{
			CurrentSubWidget->SetVisibility(ESlateVisibility::Visible);
		}

		// 마우스 커서 활성화
		bShowMouseCursor = true;

		// UI 상호작용 및 게임 입력을 동시에 받도록 설정
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		// [HUD Off] 시야 조작 가능 + 마우스 커서 제거
		if (CurrentInGameMainWidget)
		{
			CurrentInGameMainWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (CurrentSubWidget)
		{
			CurrentSubWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		bShowMouseCursor = false;

		// 게임 전용 입력 모드로 전환 (마우스로 시야 조작 가능)
		SetInputMode(FInputModeGameOnly());
	}
}

void ATOPlayerController::Multicast_UpdateMainUI_Implementation()
{
	if (IsLocalController())
	{
		K2_UpdateMainUI();
	}
}

void ATOPlayerController::Multicast_ShowCorrectNotice_Implementation(int32 WinnerPlayerIndex)
{
	// 각 클라이언트 로컬에서 블루프린트 이벤트 호출
	K2_ShowCorrectNotice(WinnerPlayerIndex);
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
	
	// StartGame 위젯을 Viewport에 직접 생성하여 덮어 씌움
	if (StartGameWidgetClass)
	{
		CurrentSubWidget = CreateWidget<UUserWidget>(this, StartGameWidgetClass);
		if (CurrentSubWidget)
		{
			CurrentSubWidget->AddToViewport(); 
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

	// WaitingGame 위젯을 Viewport에 직접 생성하여 덮어 씌움
	if (WaitingGameWidgetClass)
	{
		CurrentSubWidget = CreateWidget<UUserWidget>(this, WaitingGameWidgetClass);
		if (CurrentSubWidget)
		{
			CurrentSubWidget->AddToViewport();
		}
	}
}

// 서버로부터 제출 결과를 수신하여 클라이언트에 성공/실패 연출 출력
void ATOPlayerController::Client_ReceiveGuessResult_Implementation(bool bIsCorrect, const FString& TargetAlphabet, int32 RevealedValue)
{
	if (bIsCorrect)
	{
		// UI에 정답 성공 연출 및 공개된 카드 정보 출력
	}
	else
	{
		// UI에 오답 연출 출력
	}
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





