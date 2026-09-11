#include "TOCardDeckComponent.h"
#include "TOFormula.h"

UTOCardDeckComponent::UTOCardDeckComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Alphabets 배열 기본값 초기화 (4~6명 플레이어용)
	Alphabets = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F") };
}

FTOFormulaData UTOCardDeckComponent::GenerateRoundFormula(int32 PlayerCount)
{
	FTOFormulaData NewFormula;
	
	if (Alphabets.Num() < 6)
	{
		Alphabets = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F") };
	}

	PlayerCount = FMath::Clamp(PlayerCount, 2, 6);
	// 라운드 데이터를 만드는 함수
	// 플레이어 수가 2미만이면 2로, 6초과면 6으로 안전하게 범위를 고정
	
	//EvaluateFormula 연산자에 넘겨줄 카드 숫자 전용 TArray 변수 선언
	TArray<int32> CardValues;
	
	// 인원수(4~6)에 맞춰 -3 ~ 3 무작위 카드 숫자 부여
	for (int32 i = 0; i < PlayerCount; i++)
	{
		FTOPlayerCardData CardData;
		CardData.PlayerIndex = i;
		CardData.PlayerAlphabet = Alphabets[i];
		CardData.CardValue = FMath::RandRange(-3, 3);

		NewFormula.PlayerCards.Add(CardData);
		CardValues.Add(CardData.CardValue);
		// PlayerIndex: 0, 1, 2, 3 번호 부여
		// PlayerAlphabet: Alphabets 배열에서 "A", "B", "C", "D" 순서대로 꺼내 부여
		// CardValue: -3에서 3 사이의 임의의 정수 무작위 추출 후 카드 목록(NewFormula.PlayerCards)에 저장
	}

	// (인원수 - 1)개 만큼 연산자 무작위 추출
	for (int32 i = 0; i < PlayerCount - 1; i++)
	{
		uint8 RandOp = FMath::RandRange(0, 2); // 0: +, 1: -, 2: *
		NewFormula.Operators.Add(static_cast<ETOOperatorType>(RandOp));
		//0~2 사이 숫자를 랜덤으로 뽑아 Add(+), Subtract(-), Multiply(*)로 변환 후 연산자 목록에 저장
	}

	// UTOFormula :: EvaluateFormula 함수 호출로 결과값 대입
	NewFormula.TargetResult = UTOFormula::EvaluateFormula(CardValues, NewFormula.Operators);
	
	// =========================================================
	// [로그 출력 로직] 생성된 원본 수식 상태 확인
	FString FormulaLogString = TEXT("");

	for (int32 i = 0; i < NewFormula.PlayerCards.Num(); ++i)
	{
		//  카드 정보 추가
		const FTOPlayerCardData& Card = NewFormula.PlayerCards[i];
		FormulaLogString += FString::Printf(TEXT("%s(%d)"), *Card.PlayerAlphabet, Card.CardValue);

		//  연산자 추가 (마지막 카드가 아닌 경우에만)
		if (i < NewFormula.Operators.Num())
		{
			switch (NewFormula.Operators[i])
			{
			case ETOOperatorType::Add:      FormulaLogString += TEXT(" + "); break;
			case ETOOperatorType::Subtract: FormulaLogString += TEXT(" - "); break;
			case ETOOperatorType::Multiply: FormulaLogString += TEXT(" * "); break;
			default: break;
			}
		}
	}

	// TargetResult 연결
	FormulaLogString += FString::Printf(TEXT(" = %d"), NewFormula.TargetResult);

	// 로그 출력
	UE_LOG(LogTemp, Warning, TEXT("[CardDeck] 라운드 수식 생성 완료 -> %s"), *FormulaLogString);
	// =========================================================
	

	return NewFormula;
}