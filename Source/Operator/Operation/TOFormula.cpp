#include "TOFormula.h"

// 플레이어 시점 맞춤형 UI 데이터 생성
FTOPlayerUIData UTOFormula::BuildUIDataForPlayer(const FTOFormulaData& FormulaData, int32 TargetPlayerIndex, const TArray<FString>& RevealedAlphabets)
{
    FTOPlayerUIData UIData;
    UIData.TargetResult = FormulaData.TargetResult;
    UIData.PlayerCards = FormulaData.PlayerCards; // 카드 배열 복사
    UIData.Operators = FormulaData.Operators;     // 연산자 배열 복사
    UIData.RevealedAlphabets = RevealedAlphabets; // 공개된 알파벳 전달
    
    FString BuiltFormula = "";
    int32 TotalCount = FormulaData.PlayerCards.Num();
    
    for (int32 i = 0; i < TotalCount; i++)
    {
        const FTOPlayerCardData& Card = FormulaData.PlayerCards[i];

        // 내 카드 -> [숫자]
        if (Card.PlayerIndex == TargetPlayerIndex)
        {
            BuiltFormula += FString::Printf(TEXT("%d"), Card.CardValue);
            UIData.MyAlphabet = Card.PlayerAlphabet;
            UIData.MyCardValue = Card.CardValue;
        }
        // 유추 성공한 타인 카드 -> [숫자]
        else if (RevealedAlphabets.Contains(Card.PlayerAlphabet))
        {
            BuiltFormula += FString::Printf(TEXT("%d"), Card.CardValue);
        }
        // 아직 모르는 타인 카드 -> 알파벳 (A, B, C...)
        else
        {
            BuiltFormula += Card.PlayerAlphabet;
        }

        // 연산자 연결 (+, -, *)
        if (i < FormulaData.Operators.Num())
        {
            BuiltFormula += FString::Printf(TEXT(" %s "), *GetOperatorString(FormulaData.Operators[i]));
        }
    }

    BuiltFormula += FString::Printf(TEXT(" = %d"), FormulaData.TargetResult);
    UIData.FormulaDisplayText = BuiltFormula;

    return UIData;
}

// Enum(연산자) -> FString 변환 헬퍼 함수
FString UTOFormula::GetOperatorString(ETOOperatorType Op)
{
    switch (Op)
    {
    case ETOOperatorType::Add:      return TEXT("+");
    case ETOOperatorType::Subtract: return TEXT("-");
    case ETOOperatorType::Multiply: return TEXT("*");
    default:                        return TEXT("?");
    }
}


// A 연산자 B 연산자 C 연산자 D (연산자  E 연산자 F ) 의 계산 식 
int32 UTOFormula::EvaluateFormula(const TArray<int32>& Numbers, const TArray<ETOOperatorType>& Operators)
{
    if (Numbers.Num() != Operators.Num() + 1 || Numbers.Num() == 0)
    {
        return 0;
    }

    TArray<int32> Nums = Numbers; //플레이어 들에게 분배 된 카드의 숫자들
    TArray<ETOOperatorType> Ops = Operators; // 무작위로 만들어진 연산자

    // 곱하기(*) 우선 연산
    for (int32 i = 0; i < Ops.Num(); )
    {
        if (Ops[i] == ETOOperatorType::Multiply)
        {
            Nums[i] = Nums[i] * Nums[i + 1];
            Nums.RemoveAt(i + 1);
            Ops.RemoveAt(i);
        }
        else
        {
            i++;
        }
    }

    // 덧셈(+) 및 뺄셈(-) 순차 연산
    int32 Result = Nums[0];
    for (int32 i = 0; i < Ops.Num(); i++)
    {
        if (Ops[i] == ETOOperatorType::Add)
        {
            Result += Nums[i + 1];
        }
        else if (Ops[i] == ETOOperatorType::Subtract)
        {
            Result -= Nums[i + 1];
        }
    }

    return Result;
}





// 전체 정답 검증 (이미 공개된 알파벳 슬롯은 검증 통과 처리)
bool UTOFormula::VerifyAllAnswerWithMap(const FTOFormulaData& FormulaData, const FTOGuessAllInputData& InputData, const TArray<FString>& RevealedAlphabets)
{
    // 서버가 가진 실제 카드 정답 순회
    for (const FTOPlayerCardData& RealCard : FormulaData.PlayerCards)
    {
        // 1. 제출자 본인의 카드는 검증 대상에서 제외
        if (RealCard.PlayerIndex == InputData.SubmittingPlayerIndex)
        {
            continue;
        }

        // 2. 이미 유추 성공하여 공개된 알파벳(Slot으로 변환된 카드)은 이미 정답이 확인되었으므로 통과
        if (RevealedAlphabets.Contains(RealCard.PlayerAlphabet))
        {
            continue;
        }

        // 3. 아직 공개되지 않은 알파벳의 경우: 제출한 입력값에서 일치하는지 확인
        const FTOGuessedPair* FoundPair = InputData.GuessedPlayerValues.FindByPredicate(
            [&RealCard](const FTOGuessedPair& Pair)
            {
                return Pair.Alphabet == RealCard.PlayerAlphabet;
            });

        // 해당 알파벳 입력값이 누락되었거나 숫자가 틀린 경우 실패
        if (!FoundPair || FoundPair->GuessedValue != RealCard.CardValue)
        {
            return false;
        }
    }

    return true; // 모두 일치하거나 모든 알파벳이 이미 밝혀진 경우(알파벳 없으면) 정답/승리!
}





// 단일 카드 유추 검증
bool UTOFormula::VerifySingleCard(const FTOFormulaData& FormulaData, const FTOGuessSingleInputData& InputData)
{   // 서버가 가진 카드 목록(정답)을 확인 
    for (const FTOPlayerCardData& RealCard : FormulaData.PlayerCards)
    {
        // 플레이어가 제출한 알파벳이 서버의 알파벳과 일치하는지 확인
        if (RealCard.PlayerAlphabet == InputData.TargetAlphabet)
        {   // 알파벳(다른 플레이어의 카드)의 정답과 내가 제출한 정답과 일치하면 true, 틀리면 false
            return RealCard.CardValue == InputData.GuessedValue;
        }
    }

    return false;
}
