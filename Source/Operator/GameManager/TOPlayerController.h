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
	// --------------------- 게임(플레이어) 정보 파트 --------------------------- 

	ATOPlayerController();

	virtual void BeginPlay() override;

	// 변수 네트워크 복제(Replication) 등록을 위한 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 플레이어에게 할당된 고유 인덱스 번호 (네트워크 복제됨)
	// 사라지면 안되는 정보이기 때문에 TOTypes.h의 구조체 사용 X
	// 0~5까지 사용할 예정이기 때문에 -1로 초기화 (Null)
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "GameData")
	int32 AssignedPlayerIndex = -1;


	// ------------------- UI - Input Mode & On/Off 파트 -----------------------------

	// Character의 IA에서 호출할 인풋모드 변경시키는 함수
	UFUNCTION(BlueprintCallable, Category = "TO|UI")
	void ToggleHUD();

	// HUD On/Off (Input Mode 및 마우스 커서 제어)
	void SetHUDVisible(bool bVisible);


	// ------------------- Network - Server RPC 파트 -----------------------------

	// 로비 준비 상태 서버 전송
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Network")
	void Server_SetReady(bool bReady);

	// 캐릭터 선택 처리 요청
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Character")
	void Server_SelectCharacter(int32 CharacterID);

	// 단일 제출 버튼 클릭 시 호출되는 통합 Server RPC (정답 제출 / 카드 유추 겸용)
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Network")
	void Server_SubmitCurrentInput(const FTOGuessAllInputData& FormulaData, const FTOGuessSingleInputData& SingleGuessData);


	// ------------------- Network - Client RPC 파트 -----------------------------

	// 서버에서 클라이언트로 로비 준비 버튼 활성화/비활성화 상태 전달
	UFUNCTION(Client, Reliable)
	void Client_UpdateLobbyState(bool bCanEnableReady);

	// 게임 시작 시 로비 UI 제거 및 인게임 UI 전환 요청
	UFUNCTION(Client, Reliable)
	void Client_OnGameStarted();
	
	// 게임 종료 시 인게임 UI 제거 및 로비 UI 전환 요청
	UFUNCTION(Client, Reliable)
	void Client_OnGameEnded();

	// 수식 UI 및 페이즈 상태 갱신 요청
	UFUNCTION(Client, Reliable)
	void Client_UpdateFormulaUI(const FTOPlayerUIData& NewUIData);

	// 카드 유추 결과 수신 및 UI 연출 출력 요청
	UFUNCTION(Client, Reliable)
	void Client_ReceiveGuessResult(bool bIsCorrect, const FString& TargetAlphabet, int32 RevealedValue);


protected:
	// ------------------- UI - Class & Instance 파트 -----------------------------

	// 에디터에서 블루프린트 위젯을 할당하기 위한 클래스 변수
	UPROPERTY(EditDefaultsOnly, Category = "TO|UI")
	TSubclassOf<class UUserWidget> LobbyWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "TO|UI")
	TSubclassOf<class UUserWidget> InGameHUDWidgetClass;

	// 런타임에 생성된 위젯 인스턴스를 보관하기 위한 포인터 변수
	UPROPERTY()
	TObjectPtr<class UUserWidget> CurrentLobbyWidget;

	UPROPERTY()
	TObjectPtr<class UUserWidget> CurrentInGameHUDWidget;

	// 현재 HUD 표시 및 UI 입력 모드 상태 관리 플래그
	bool bIsHUDVisible = true;
};