// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "../Operation/TOTypes.h"
#include "TOPlayerController.generated.h"

UCLASS()
class OPERATOR_API ATOPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATOPlayerController();

	// 변수 네트워크 복제(Replication) 등록을 위한 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버로 캐릭터 선택을 요청하는 RPC
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Character")
	void Server_SelectCharacter(int32 CharacterID);
	
	// 서버로 수식 정답 제출을 요청하는 RPC
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Network")
	void Server_SubmitFormula(const FTOGuessAllInputData& InputData);
	
	// 서버로 타인 카드 유추 제출을 요청하는 RPC
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Network")
	void Server_SubmitGuess(const FTOGuessSingleInputData& InputData);
	
	// 서버로 다음 라운드 준비 상태를 전달하는 RPC
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Network")
	void Server_SelectNextRound();
	
	
	//----------------------------------------------------------------------------------------------------------
	
	
	// 서버에서 클라이언트로 가공된 UI 데이터를 전송하는 RPC
	UFUNCTION(Client, Reliable)
	void Client_UpdateFormulaUI(const FTOPlayerUIData& NewUIData);

	// 서버에서 클라이언트로 제출한 정답의 판정 결과를 전달하는 RPC
	UFUNCTION(Client, Reliable)
	void Client_ReceiveGuessResult(bool bIsCorrect, const FString& TargetAlphabet, int32 RevealedValue);

public:
	// 플레이어에게 할당된 고유 인덱스 번호 (네트워크 복제됨)
	// 사라지면 안되는 정보이기 때문에 TOTypes.h의 구조체 사용 X
	// 0~5까지 사용할 예정이기 때문에 -1로 초기화 (Null)
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "GameData")
	int32 AssignedPlayerIndex = -1;
	
public:
	// 화면에 게임 플레이 위젯을 생성하고 띄워주는 함수
	UFUNCTION(BlueprintCallable, Category = "TO|UI")
	void ShowGameWidget();

protected:
	// 생성할 위젯의 블루프린트 클래스 타입 (에디터에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "TO|UI")
	TSubclassOf<class UUserWidget> GameWidgetClass;

	// 실제로 생성된 위젯 객체 보관용
	UPROPERTY()
	class UUserWidget* CurrentGameWidget;
};