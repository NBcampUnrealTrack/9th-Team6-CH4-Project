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
	DOREPLIFETIME(ATOPlayerState, PlayerScore);
	DOREPLIFETIME(ATOPlayerState, PlayerNameString);
}


void ATOPlayerState::AddScorePoints(int32 InScore)
{
	if (HasAuthority()) // 서버에서만 수정 가능
	{
		PlayerScore += InScore;
		SetScore(PlayerScore); // APlayerState 기본 제공 Score 변수도 동기화
	}
}

void ATOPlayerState::Server_SetPlayerName_Implementation(const FString& NewName)
{
	PlayerNameString = NewName;

	// 언리얼 기본 APlayerState의 PlayerName 속성도 함께 설정 (선택 사항)
	SetPlayerName(NewName);
}

// [클라이언트에서 실행] 서버로부터 닉네임 값이 동기화되면 자동으로 실행
void ATOPlayerState::OnRep_PlayerNameString()
{
	// 캐릭터 머리 위 UI나 스코어보드 닉네임 갱신 로직이 들어갈 위치입니다.
	UE_LOG(LogTemp, Log, TEXT("PlayerName Replicated: %s"), *PlayerNameString);
}