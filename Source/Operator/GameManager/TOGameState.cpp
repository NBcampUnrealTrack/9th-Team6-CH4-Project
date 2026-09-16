#include "TOGameState.h"
#include "TOPlayerController.h"
#include "Kismet/GameplayStatics.h"

void ATOGameState::Multicast_BroadcastChatMessage_Implementation(const FString& SenderName, const FString& Message)
{
	// 월드내의 플레이어들 확인
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* LocalPC = World->GetFirstPlayerController();
	if (ATOPlayerController* TOPC = Cast<ATOPlayerController>(LocalPC))
	{
		// BP_TOPlayerController의 블루프린트 이벤트 호출
		TOPC->K2_AddChatMessageToUI(SenderName, Message);
	}
}