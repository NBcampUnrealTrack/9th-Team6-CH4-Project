#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TOPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerCustomizationChanged, int32, HairIndex, int32, TopIndex, int32, BottomIndex);

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

// --------------------------닉네임 파트 --------------------------------
public:
	// 동기화될 닉네임 변수 (서버에서 변경 시 OnRep_PlayerNameString 호출)
	UPROPERTY(ReplicatedUsing = OnRep_PlayerNameString, BlueprintReadOnly, Category = "TO|PlayerState")
	FString PlayerNameString;

	// 클라이언트가 서버로 닉네임을 전송하는 Server RPC
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|PlayerState")
	void Server_SetPlayerName(const FString& NewName);

	void SetPlayerNameString(const FString& NewName);


	// 닉네임이 동기화될 때 클라이언트에서 호출될 함수 (UI/머리 위 닉네임 갱신용)
	UFUNCTION()
	void OnRep_PlayerNameString();

	// 닉네임 Getter
	UFUNCTION(BlueprintCallable, Category = "TO|PlayerState")
	FString GetCustomPlayerName() const { return PlayerNameString; }

// -------------------------- 커스터마이징 동기화 파트 --------------------------
public:
	UPROPERTY(ReplicatedUsing = OnRep_Customization, BlueprintReadOnly, Category = "TO|Customization")
	int32 SelectedHairIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Customization, BlueprintReadOnly, Category = "TO|Customization")
	int32 SelectedTopIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Customization, BlueprintReadOnly, Category = "TO|Customization")
	int32 SelectedBottomIndex = 0;

	UPROPERTY(BlueprintAssignable, Category = "TO|Customization")
	FOnPlayerCustomizationChanged OnPlayerCustomizationChanged;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Customization")
	void Server_SetCustomization(int32 InHairIndex, int32 InTopIndex, int32 InBottomIndex);

	UFUNCTION()
	void OnRep_Customization();

	void SetCustomization(int32 InHairIndex, int32 InTopIndex, int32 InBottomIndex);

	UFUNCTION(BlueprintCallable, Category = "TO|Customization")
	void GetCustomization(int32& OutHairIndex, int32& OutTopIndex, int32& OutBottomIndex) const
	{
		OutHairIndex = SelectedHairIndex;
		OutTopIndex = SelectedTopIndex;
		OutBottomIndex = SelectedBottomIndex;
	}
};