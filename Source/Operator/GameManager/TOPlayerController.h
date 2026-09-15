// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Framework/SlateDelegates.h"
#include "Components/AudioComponent.h"
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
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;
	

	// 변수 네트워크 복제(Replication) 등록을 위한 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	// 플레이어에게 할당된 고유 인덱스 번호 (네트워크 복제됨)
	// 사라지면 안되는 정보이기 때문에 TOTypes.h의 구조체 사용 X
	// 0~5까지 사용할 예정이기 때문에 -1로 초기화 (Null)
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "GameData")
	int32 AssignedPlayerIndex = -1;

	
	// 게임모드에서 호출해 줄 BGM 전환 함수 (Client RPC)
	UFUNCTION(Client, Reliable)
	void Client_SwitchToInGameBGM();
	
	// --------------------- BGM -------------------------------------------
	
	// 대기실 BGM 컴포넌트 레퍼런스
	UPROPERTY()
	UAudioComponent* WaitingBGMComponent;

	// 대기실 BGM 에셋
	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	USoundBase* WaitingBGMSound;

	// 인게임 BGM 에셋
	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	USoundBase* InGameBGMSound;
	

	// ------------------- UI - Input Mode & On/Off 파트 -----------------------------

	// Character의 IA에서 호출할 인풋모드 변경시키는 함수
	UFUNCTION(BlueprintCallable, Category = "TO|UI")
	void ToggleHUD();

	// HUD On/Off (Input Mode 및 마우스 커서 제어)
	void SetHUDVisible(bool bVisible);

	virtual bool InputKey(const struct FInputKeyEventArgs& Params) override;


	// ------------------- Network - Server RPC 파트 -----------------------------

	// 로비 준비 상태 서버 전송
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Network")
	void Server_SetReady(bool bReady);

	// 캐릭터 선택 처리 요청
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Character")
	void Server_SelectCharacter(int32 CharacterID);

	// 커스터마이징 정보 서버 전송
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Customization")
	void Server_SetCustomization(int32 InHairIndex, int32 InTopIndex, int32 InBottomIndex);

	// 플레이어 닉네임 서버 전송
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|PlayerInfo")
	void Server_SetPlayerName(const FString& InPlayerName);

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
	void Client_ReceiveGuessResult(bool bIsMatch, const FString& TargetAlphabet, int32 RevealedVal);

	UFUNCTION(BlueprintImplementableEvent, Category = "TO|UI")
	void K2_OnReceiveGuessResult(bool bIsMatch, const FString& TargetAlphabet, int32 RevealedVal);
	
	// 정답 연출 (전체 클라이언트 브로드캐스트용 Client RPC)
	UFUNCTION(Client, Reliable)
	void Client_ShowCorrectNotice(int32 WinnerPlayerIndex);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ShowCorrectNotice(int32 WinnerPlayerIndex);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "TO|UI")
	void K2_ShowCorrectNotice(int32 WinnerPlayerIndex);

	// 메인 위젯 갱신 요청 (전체 클라이언트 브로드캐스트용 Client RPC)
	UFUNCTION(Client, Reliable)
	void Client_UpdateMainUI();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_UpdateMainUI();

	UFUNCTION(BlueprintImplementableEvent, Category = "TO|UI")
	void K2_UpdateMainUI();

	// WBP_StartGame 페이즈별 위젯 가시성 설정 (Phase 1: 숨김, Phase 2: 표시)
	UFUNCTION(BlueprintCallable, Category = "TO|UI")
	void UpdateStartGamePhaseVisibility(ETOGamePhase Phase);

	// InGameMainWidget 표시/숨김 설정 (환경설정 등 전체화면 팝업 시 유용)
	UFUNCTION(BlueprintCallable, Category = "TO|UI")
	void SetInGameMainWidgetVisibility(bool bVisible);

	// 환경설정 위젯 열기: InGameMainWidget을 숨기고, 환경설정이 닫힐 때 자동으로 복구
	UFUNCTION(BlueprintCallable, Category = "TO|UI")
	void OpenSettingsWidget(UUserWidget* SettingsWidgetInstance);

protected:
	FTimerHandle SettingsWidgetMonitorTimerHandle;
	TWeakObjectPtr<UUserWidget> MonitoredSettingsWidget;
	void MonitorSettingsWidgetClosed();
	UUserWidget* FindActiveSettingsWidget() const;

	UFUNCTION()
	void OnSettingsButtonClicked();

	void SetupWaitingGameBindings();
	void SetupNumberCardBindings();

	UFUNCTION()
	void OnCardNumberSelected(int32 SelectedNumber);

	UFUNCTION()
	void OnAnyNumberCardButtonClicked();

	UPROPERTY()
	TMap<TWeakObjectPtr<class UButton>, int32> NumberCardButtonMap;
	
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

	virtual void InitPlayerState() override;

public:
	bool bHasCachedCustomization = false;
	int32 CachedHairIndex = 0;
	int32 CachedTopIndex = 0;
	int32 CachedBottomIndex = 0;

	bool bHasCachedPlayerName = false;
	FString CachedPlayerName;

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

	// 채팅창 자동 포커스 및 상호작용
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void FocusChatInput();

	UFUNCTION(BlueprintCallable, Category = "Chat")
	void UnfocusChatInput();

	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SetupChatInputBox();

	UFUNCTION(BlueprintCallable, Category = "Chat")
	class UEditableTextBox* FindChatInputBox() const;

	UFUNCTION()
	void HandleChatCommitted(const FText& Text, ETextCommit::Type CommitMethod);

};