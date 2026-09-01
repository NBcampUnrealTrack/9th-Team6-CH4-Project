#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TOPlayerState.generated.h"

UCLASS()
class OPERATOR_API ATOPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	
	//생성자
	ATOPlayerState();
	
	// 선택한 캐릭터 ID (외형 변경 등)
	UPROPERTY(Replicated, BlueprintReadWrite, Category = "Character")
	int32 SelectedCharacterID = -1;

	// 캐릭터 선택 및 준비 완료 여부
	UPROPERTY(Replicated, BlueprintReadWrite, Category = "Character")
	bool bIsReadyToPlay = false;

	// 멀티플레이어 동기화를 위한 변수 등록 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	
	//  Getters / Setters
	void SetAssignedPlayerIndex(int32 InIndex) { AssignedPlayerIndex = InIndex; }
	int32 GetAssignedPlayerIndex() const { return AssignedPlayerIndex; }

	void SetPlayerAlphabet(const FString& InAlphabet) { PlayerAlphabet = InAlphabet; }
	FString GetPlayerAlphabet() const { return PlayerAlphabet; }

	void AddScorePoints(int32 InScore);
	int32 GetPlayerScore() const { return PlayerScore; }
	
	
protected:
	// 플레이어 고유 인덱스 (0~5)
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "TO|PlayerState")
	int32 AssignedPlayerIndex = -1;

	// [신규 추가] 라운드별 할당된 알파벳 ("A", "B", ...)
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "TO|PlayerState")
	FString PlayerAlphabet;

	// [신규 추가] 플레이어 누적 점수
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "TO|PlayerState")
	int32 PlayerScore = 0;
};