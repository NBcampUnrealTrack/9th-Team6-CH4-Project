#include "TOPlayerController.h"
#include "TOPlayerState.h"
#include "TOGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"

ATOPlayerController::ATOPlayerController()
{
	bReplicates = true;
}


void ATOPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 플레이어 진입 시 로비 UI 생성 및 기본 UI 모드 설정
	if (IsLocalController())
	{
		if (LobbyWidgetClass)
		{
			CurrentLobbyWidget = CreateWidget<UUserWidget>(this, LobbyWidgetClass);
			if (CurrentLobbyWidget)
			{
				CurrentLobbyWidget->AddToViewport();
			}
		}

		// 로비에서 마우스 커서 켜고 GameAndUI 모드 세팅
		bShowMouseCursor = true;
		SetInputMode(FInputModeGameAndUI());
	}
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
	// 인게임 HUD가 띄워져 있을 때만 토글 작동
	if (CurrentInGameHUDWidget)
	{
		SetHUDVisible(!bIsHUDVisible);
	}
}


// HUD On/Off에 따른 Input Mode & Cursor 변경 함수
void ATOPlayerController::SetHUDVisible(bool bVisible)
{
	bIsHUDVisible = bVisible;

	if (!CurrentInGameHUDWidget) return;

	if (bIsHUDVisible)
	{
		// [HUD On] UI 상호작용 가능 + 마우스 커서 노출
		CurrentInGameHUDWidget->SetVisibility(ESlateVisibility::Visible);
		bShowMouseCursor = true;
		
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		// [HUD Off] 시야 조작 가능 + 마우스 커서 제거
		CurrentInGameHUDWidget->SetVisibility(ESlateVisibility::Collapsed);
		bShowMouseCursor = false;
		
		SetInputMode(FInputModeGameOnly());
	}
}


// [추가] 서버로 Ready 상태 전송
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
	// 수신받은 NewUIData를 바인딩된 UUserWidget UI 요소에 적용하여 화면에 출력 예정
}


void ATOPlayerController::Client_OnGameStarted_Implementation()
{
	if (!IsLocalController()) return;

	if (CurrentLobbyWidget)
	{
		CurrentLobbyWidget->RemoveFromParent();
		CurrentLobbyWidget = nullptr;
	}

	if (InGameHUDWidgetClass && !CurrentInGameHUDWidget)
	{
		CurrentInGameHUDWidget = CreateWidget<UUserWidget>(this, InGameHUDWidgetClass);
		if (CurrentInGameHUDWidget)
		{
			CurrentInGameHUDWidget->AddToViewport();
		}
	}

	SetHUDVisible(true);
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