#include "TOPlayerState.h"
#include "Net/UnrealNetwork.h"


ATOPlayerState::ATOPlayerState()
{
	bReplicates = true;
	AssignedPlayerIndex = -1;
	PlayerAlphabet = TEXT("");
	PlayerScore = 0;
}


void ATOPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 서버에서 값이 바뀌면 클라이언트들에게도 동기화되도록 설정
	DOREPLIFETIME(ATOPlayerState, SelectedCharacterID);
	DOREPLIFETIME(ATOPlayerState, bIsReadyToPlay);
	
	DOREPLIFETIME(ATOPlayerState, AssignedPlayerIndex);
	DOREPLIFETIME(ATOPlayerState, PlayerAlphabet);
	DOREPLIFETIME(ATOPlayerState, PlayerScore)
}


void ATOPlayerState::AddScorePoints(int32 InScore)
{
	//if (HasAuthority()) // 서버에서만 수정 가능
	//{
		//PlayerScore += InScore;
		//SetScore(PlayerScore); // APlayerState 기본 제공 Score 변수도 동기화
	//}
}