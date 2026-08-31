#include "TOPlayerState.h"
#include "Net/UnrealNetwork.h"

void ATOPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 서버에서 값이 바뀌면 클라이언트들에게도 동기화되도록 설정
	DOREPLIFETIME(ATOPlayerState, SelectedCharacterID);
	DOREPLIFETIME(ATOPlayerState, bIsReadyToPlay);
}