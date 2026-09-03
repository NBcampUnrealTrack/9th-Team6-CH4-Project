// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TOMenuGameMode.generated.h"

class UUserWidget;

/**
 * 메인 메뉴 전용 게임모드
 * 캐릭터 스폰을 방지하고 메뉴 전용 플레이어 컨트롤러를 연결합니다.
 */
UCLASS()
class OPERATOR_API ATOMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATOMenuGameMode();

protected:
	virtual void BeginPlay() override;

public:
	/** 메인 메뉴 화면에 띄울 위젯 클래스 (WBP_MainMenu 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Menu")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

	/** 생성된 메인 메뉴 위젯 인스턴스 */
	UPROPERTY(BlueprintReadOnly, Category = "Operator|Menu")
	TObjectPtr<UUserWidget> MainMenuWidgetInstance;
};
