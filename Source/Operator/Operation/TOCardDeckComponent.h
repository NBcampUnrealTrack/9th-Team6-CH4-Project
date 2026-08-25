#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TOTypes.h"
#include "TOCardDeckComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OPERATOR_API UTOCardDeckComponent : public UActorComponent
{
	GENERATED_BODY()

public:    
	UTOCardDeckComponent();

	// 인원수(4~6명)를 받아 무작위 카드 및 수식 데이터 생성
	UFUNCTION(BlueprintCallable, Category = "GameLogic|Deck")
	FTOFormulaData GenerateRoundFormula(int32 PlayerCount);

private:
	
	// 플레이어의 Index 에 맞게 대입 될 알파벳 배열
	const TArray<FString> Alphabets = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F") };
};