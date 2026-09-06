// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TOMenuPlayerController.generated.h"

/**
 * 메인 메뉴 전용 플레이어 컨트롤러
 * 마우스 커서 표시 및 UI 입력 모드를 처리합니다.
 */
UCLASS()
class OPERATOR_API ATOMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATOMenuPlayerController();

protected:
	virtual void BeginPlay() override;

public:
	/** UI 전용 입력 모드로 설정 (마우스 커서 활성화) */
	UFUNCTION(BlueprintCallable, Category = "Operator|Menu")
	void SetMenuInputMode();

	/** 게임/UI 혼합 입력 모드로 설정 */
	UFUNCTION(BlueprintCallable, Category = "Operator|Menu")
	void SetGameAndUIInputMode();
	
	
protected:
	
	UPROPERTY(EditDefaultsOnly, Category = "TO|UI")
	TSubclassOf<class UUserWidget> MainMenuWidgetClass;
};
