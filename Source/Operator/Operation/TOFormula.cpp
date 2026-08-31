#include "TOFormula.h"

// 플레이어 시점 맞춤형 UI 데이터 생성
FTOPlayerUIData UTOFormula::BuildUIDataForPlayer(const FTOFormulaData& FormulaData, int32 TargetPlayerIndex, const TArray<FString>& RevealedAlphabets)
{
    // FTOFormulaData& formulaData: 라운드의 전체 수식 정보(카드 값, 연산자 등)를 담은 원본 데이터
    // TargetPlayerIndex: 이 UI 데이터를 할당 받을 플레이어의 번호
    // RevealedAlphabets: 이전 턴에 플레이어 카드 유추에 성공해서 개인 화면에 공개된 알파벳 목록
    FTOPlayerUIData UIData;
    UIData.TargetResult = FormulaData.TargetResult;
    // UI에 전달할 최종 반환 구조체 UIData를 생성 , 연산 결과값 대입
    
    FString BuiltFormula = "";
    int32 TotalCount = FormulaData.PlayerCards.Num();
    // 화면에 띄울 수식 텍스트를 담을 빈 문자열(BuiltFormula) 생성 , 전체 플레이어(카드) 개수 취합
    
    for (int32 i = 0; i < TotalCount; i++)
    {
        const FTOPlayerCardData& Card = FormulaData.PlayerCards[i];

        // 내 카드 -> [숫자]
        if (Card.PlayerIndex == TargetPlayerIndex)
        {
            BuiltFormula += FString::Printf(TEXT("%d"), Card.CardValue);
            UIData.MyAlphabet = Card.PlayerAlphabet;
            UIData.MyCardValue = Card.CardValue;
            // 내 화면의 내 카드(알파벳)를 수식 텍스트에 숫자로 변경
        }
        
        // 유추 성공한 타인 카드 -> [숫자]
        else if (RevealedAlphabets.Contains(Card.PlayerAlphabet))
        {
            BuiltFormula += FString::Printf(TEXT("%d"), Card.CardValue);
            // 타인의 카드 중 유추에 성공한 알파벳 목록(RevealedAlphabets)에 포함되어 있다면 숫자로 변경
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
    
    // 완성된 텍스트(BuiltFormula)를 UIData.FormulaDisplayText에 넣어서 리턴
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





// 전체 정답 검증 
bool UTOFormula::VerifyAllAnswerWithMap(const FTOFormulaData& FormulaData, const FTOGuessAllInputData& InputData)
{
    // 플레이어가 입력한 정답을 구조체에 대입
    const TMap<FString, int32>& GuessedMap = InputData.GuessedPlayerValues;
    // 서버가 가진 카드 목록(정답)을 확인 
    for (const FTOPlayerCardData& RealCard : FormulaData.PlayerCards)
    {   
        // 제출한 정답에 누락한 내용이 있을 때 (A값을 입력 안하고 제출했을 때)
        if (!GuessedMap.Contains(RealCard.PlayerAlphabet))
        {
            return false;
        }
        // 제출한 정답에 틀린 내용이 있을 때
        if (GuessedMap[RealCard.PlayerAlphabet] != RealCard.CardValue)
        {
            return false;
        }
    }

    return true; // 모두 일치하면 정답
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
