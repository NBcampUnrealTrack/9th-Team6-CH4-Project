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


//컴파일 에러 방지

int32 UTOFormula::EvaluateFormula(const TArray<int32>& Numbers, const TArray<ETOOperatorType>& Operators)
{
    return 0;
}

bool UTOFormula::VerifyAllAnswerWithMap(const FTOFormulaData& FormulaData, const FTOGuessAllInputData& InputData)
{
    return false;
}

bool UTOFormula::VerifySingleCard(const FTOFormulaData& FormulaData, const FTOGuessSingleInputData& InputData)
{
    return false;
}

TArray<FTOPlayerScoreData> UTOFormula::CalculateFinalRanks(TArray<FTOPlayerScoreData> ScoreList)
{
    return ScoreList;
}