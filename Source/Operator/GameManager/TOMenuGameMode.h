// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TOMenuGameMode.generated.h"

class UUserWidget;

UCLASS()
class OPERATOR_API ATOMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATOMenuGameMode();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Menu")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "Operator|Menu")
	TObjectPtr<UUserWidget> MainMenuWidgetInstance;
};
