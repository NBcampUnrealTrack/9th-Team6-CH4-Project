#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "TOGameState.generated.h"

UCLASS()
class OPERATOR_API ATOGameState : public AGameState
{
	GENERATED_BODY()

public:
	// 모든 클라이언트에게 채팅 메시지를 브로드캐스트(출력)하는 Multicast RPC
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BroadcastChatMessage(const FString& SenderName, const FString& Message);
};