// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TOMenuPlayerController.generated.h"


UCLASS()
class OPERATOR_API ATOMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATOMenuPlayerController();

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Operator|Menu")
	void SetMenuInputMode();

	UFUNCTION(BlueprintCallable, Category = "Operator|Menu")
	void SetGameAndUIInputMode();
};
