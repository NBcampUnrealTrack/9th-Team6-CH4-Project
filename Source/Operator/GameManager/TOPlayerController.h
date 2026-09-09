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

	UFUNCTION(BlueprintImplementableEvent, Category = "TO|UI")
	void K2_OnUpdateFormulaUI(const FTOPlayerUIData& NewUIData);
	
	// 수식 UI 및 페이즈 상태 갱신 요청
	UFUNCTION(Client, Reliable)
	void Client_UpdateFormulaUI(const FTOPlayerUIData& NewUIData);

	// 카드 유추 결과 수신 및 UI 연출 출력 요청
	UFUNCTION(Client, Reliable)
	void Client_ReceiveGuessResult(bool bIsCorrect, const FString& TargetAlphabet, int32 RevealedValue);


	// ------------------- UI - Class & Instance 파트 -----------------------------
protected:
	
	// 최상위 메인 위젯 (WBP_InGameMain Class & Instance)
	UPROPERTY(EditDefaultsOnly, Category = "TO|UI")
	TSubclassOf<UUserWidget> InGameMainWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "TO|UI")
	UUserWidget* CurrentInGameMainWidget;

	// 교체될 서브 위젯 클래스들
	UPROPERTY(EditDefaultsOnly, Category = "TO|UI")
	TSubclassOf<UUserWidget> WaitingGameWidgetClass; // WBP_WaitingGame

	UPROPERTY(EditDefaultsOnly, Category = "TO|UI")
	TSubclassOf<UUserWidget> StartGameWidgetClass;   // WBP_StartGame

	// 현재 띄워져 있는 서브 위젯 보관용 포인터
	UPROPERTY(BlueprintReadOnly, Category = "TO|UI")
	UUserWidget* CurrentSubWidget;

	// 3. 채팅 메시지 전용 소형 위젯 클래스
	UPROPERTY(EditDefaultsOnly, Category = "TO|UI")
	TSubclassOf<UUserWidget> ChatMessageWidgetClass; // WBP_ChatMessage
	
	// 현재 HUD 표시 및 UI 입력 모드 상태 관리 플래그
	bool bIsHUDVisible = true;
	
	// -----------------------인게임 채팅 관련 파트 -----------------------------
public:
	//  채팅 전송 (Client -> Server)
	UFUNCTION(Server, BlueprintCallable, Reliable)
	void Server_SendChatMessage(const FString& Message);

	// 채팅 브로드캐스트 (Server -> All Clients)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BroadcastChatMessage(const FString& SenderName, const FString& Message);

	// 클라이언트 로컬 UI에 채팅 메시지 출력해주는 함수
	UFUNCTION(BlueprintImplementableEvent, Category = "Chat")
	void K2_AddChatMessageToUI(const FString& SenderName, const FString& Message);

};