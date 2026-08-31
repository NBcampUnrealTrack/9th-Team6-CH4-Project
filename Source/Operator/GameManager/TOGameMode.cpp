// Fill out your copyright notice in the Description page of Project Settings.


#include "TOGameMode.h"
#include "../Operation/TOFormula.h"
#include "../Operation/TOTypes.h"
#include "../Operation/TOCardDeckComponent.h"
#include "TOPlayerController.h"

// 생성자
ATOGameMode::ATOGameMode()
{
    CardDeckComponent = CreateDefaultSubobject<UTOCardDeckComponent>(TEXT("CardDeckComponent"));
    CurrentGamePhase = ETOGamePhase::WaitingToStart;
}

// 라운드 시작 함수
void ATOGameMode::StartNewRound(int32 PlayerCount)
{
    if (!CardDeckComponent) return;

    // 1. 상태를 '수식 제출 단계'로 변경
    CurrentGamePhase = ETOGamePhase::SubmittingFormulas;
    
    // 2. 공개된 알파벳 목록 초기화
    PlayerRevealedAlphabets.Empty();
    for (int32 i = 0; i < PlayerCount; i++)
    {
       FPlayerAlphabetList NewList;
       PlayerRevealedAlphabets.Add(i, NewList);
       
       // 점수판이 비어있다면 초기화
       if (!PlayerScores.Contains(i))
       {
          PlayerScores.Add(i, 0);
       }
    }

    // 3. 카드덱에서 새로운 수식 데이터 생성 (서버 정답지에 저장)
    CurrentServerCardData = CardDeckComponent->GenerateRoundFormula(PlayerCount);
}

// [Phase 1] 플레이어가 수식을 제출했을 때
void ATOGameMode::SubmitPlayerFormula(ATOPlayerController* SenderController, const FTOGuessAllInputData& GuessData)
{
    // 수식 제출 페이즈가 아니면 무시
    if (CurrentGamePhase != ETOGamePhase::SubmittingFormulas) return;

    // 팀원이 만든 전체 정답 검증 함수 호출
    bool bIsCorrect = UTOFormula::VerifyAllAnswerWithMap(CurrentServerCardData, GuessData);

    if (bIsCorrect)
    {
        // 정답을 맞췄을 때의 처리 (점수 부여 등)
    }
}

// [Phase 2] 타인의 카드 유추 제출
void ATOGameMode::SubmitPlayerGuess(ATOPlayerController* SenderController, const FTOGuessSingleInputData& SingleGuessData)
{
    // 유추 페이즈가 아니면 무시
    if (CurrentGamePhase != ETOGamePhase::GuessingCards) return;

    // 팀원이 만든 단일 카드 검증 함수 호출
    bool bIsMatch = UTOFormula::VerifySingleCard(CurrentServerCardData, SingleGuessData);

    if (bIsMatch)
    {
        // 유추 성공 시 처리
    }
}

// 특정 플레이어에게 전달할 UI 데이터 반환
//FTOPlayerUIData ATOGameMode::GetUIDataForPlayer(AController* TargetPlayer)
//{
    
//}

// 다른 플레이어의 카드 유추 성공 시 알파벳 추가
void ATOGameMode::AddRevealedAlphabetForPlayer(int32 TargetPlayerIndex, const FString& Alphabet)
{
    if (PlayerRevealedAlphabets.Contains(TargetPlayerIndex))
    {
       PlayerRevealedAlphabets[TargetPlayerIndex].Alphabets.AddUnique(Alphabet);
    }
}