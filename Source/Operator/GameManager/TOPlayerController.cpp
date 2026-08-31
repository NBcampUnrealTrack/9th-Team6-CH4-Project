#include "TOPlayerController.h"
#include "TOPlayerState.h"
#include "TOGameMode.h"

void ATOPlayerController::Server_SubmitFormula_Implementation(const FString& Formula)
{
	// 서버 환경에서 GameMode를 가져와서 수식 제출 함수 호출
	if (AGameModeBase* AuthorityGameMode = GetWorld()->GetAuthGameMode())
	{
		// ATOGameMode로 캐스팅 후 수식 제출 처리
	}
}

void ATOPlayerController::Server_SubmitGuess_Implementation(int32 TargetPlayerIndex, const FString& GuessedValue)
{
	// 서버 환경에서 GameMode를 가져와서 유추 제출 처리
}

void ATOPlayerController::Server_SelectCharacter_Implementation(int32 CharacterID)
{
	// PlayerState를 가져와서 선택한 캐릭터 ID 저장
	if (ATOPlayerState* TOPS = GetPlayerState<ATOPlayerState>())
	{
		TOPS->SelectedCharacterID = CharacterID;
		TOPS->bIsReadyToPlay = true;
	}
}