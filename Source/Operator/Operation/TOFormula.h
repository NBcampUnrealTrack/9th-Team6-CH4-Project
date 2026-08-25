#pragma once

#include "CoreMinimal.h"
#include "TOTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TOFormula.generated.h"

UCLASS()
class OPERATOR_API UTOFormula : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	// 플레이어 시점 맞춤형 UI 데이터 생성 (내 카드: [숫자], 타인: 알파벳)
	UFUNCTION(BlueprintCallable, Category = "GameLogic|UI")
	static FTOPlayerUIData BuildUIDataForPlayer(const FTOFormulaData& FormulaData, int32 TargetPlayerIndex, const TArray<FString>& RevealedAlphabets);

	// 연산자 Enum을 문자열("+", "-", "*")로 변환하는 헬퍼 함수
	static FString GetOperatorString(ETOOperatorType Op);
    
	// N(플레이어의 수) 개 숫자와 N-1개 연산자 계산 (* 우선 연산)
	UFUNCTION(BlueprintCallable, Category = "GameLogic|Operation")
	static int32 EvaluateFormula(const TArray<int32>& Numbers, const TArray<ETOOperatorType>& Operators);

	// 전체 정답 검증
	UFUNCTION(BlueprintCallable, Category = "GameLogic|Verification")
	static bool VerifyAllAnswerWithMap(const FTOFormulaData& FormulaData, const FTOGuessAllInputData& InputData);

	// 단일 카드 유추 검증
	UFUNCTION(BlueprintCallable, Category = "GameLogic|Verification")
	static bool VerifySingleCard(const FTOFormulaData& FormulaData, const FTOGuessSingleInputData& InputData);
	
};