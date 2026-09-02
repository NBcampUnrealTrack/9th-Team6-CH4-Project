#include "TOPlayerController.h"
#include "TOPlayerState.h"
#include "TOGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"

ATOPlayerController::ATOPlayerController()
{
	bReplicates = true;
}


// AssignedPlayerIndex 변수를 모든 클라이언트에 복제하도록 설정하는 로직
void ATOPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATOPlayerController, AssignedPlayerIndex);
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
		TOPS->bIsReadyToPlay = true;
	}
}


// 클라이언트가 제출한 수식을 받아서 서버의 GameMode 처리 로직으로 전달
void ATOPlayerController::Server_SubmitFormula_Implementation(const FTOGuessAllInputData& InputData)
{
	ATOGameMode* GameMode = GetWorld()->GetAuthGameMode<ATOGameMode>();
	if (GameMode)
	{
		GameMode->SubmitPlayerFormula(this, InputData);
	}
}


void ATOPlayerController::Server_SubmitGuess_Implementation(const FTOGuessSingleInputData& InputData)
{
	ATOGameMode* GameMode = GetWorld()->GetAuthGameMode<ATOGameMode>();
	if (GameMode)
	{
		GameMode->SubmitPlayerGuess(this, InputData);
	}
}

void ATOPlayerController::Server_SelectNextRound_Implementation()
{
	ATOGameMode* GameMode = GetWorld()->GetAuthGameMode<ATOGameMode>();
	if (GameMode)
	{
		// 라운드 준비 수락 처리 로직 진행
	}
}


void ATOPlayerController::Client_UpdateFormulaUI_Implementation(const FTOPlayerUIData& NewUIData)
{
	// 수신받은 NewUIData를 바인딩된 UUserWidget UI 요소에 적용하여 화면에 출력
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

void ATOPlayerController::ShowGameWidget()
{
	// 로컬 플레이어 화면에만 위젯을 띄움 (서버가 아닐 때, 혹은 로컬 컨트롤러일 때)
	if (IsLocalController() && GameWidgetClass)
	{
		CurrentGameWidget = CreateWidget<UUserWidget>(this, GameWidgetClass);
		if (CurrentGameWidget)
		{
			CurrentGameWidget->AddToViewport();
            
			// 마우스 커서 활성화 (UI 조작을 위해)
			bShowMouseCursor = true;
			SetInputMode(FInputModeUIOnly());
		}
	}
}