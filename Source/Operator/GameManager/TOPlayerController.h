// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TOPlayerController.generated.h"

UCLASS()
class OPERATOR_API ATOPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 서버로 수식 제출을 요청하는 함수 (Server RPC)
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Network")
	void Server_SubmitFormula(const FString& Formula);

	// 서버로 타인 카드 유추 제출을 요청하는 함수 (Server RPC)
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Network")
	void Server_SubmitGuess(int32 TargetPlayerIndex, const FString& GuessedValue);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TO|Character")
	void Server_SelectCharacter(int32 CharacterID);
	
};