#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TOPlayerState.generated.h"

UCLASS()
class OPERATOR_API ATOPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	// 선택한 캐릭터 ID
	UPROPERTY(Replicated, BlueprintReadWrite, Category = "Character")
	int32 SelectedCharacterID = -1;

	// 캐릭터 선택 및 준비 완료 여부
	UPROPERTY(Replicated, BlueprintReadWrite, Category = "Character")
	bool bIsReadyToPlay = false;

	// 멀티플레이어 동기화를 위한 변수 등록 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};