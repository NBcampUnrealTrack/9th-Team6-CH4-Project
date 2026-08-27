// Fill out your copyright notice in the Description page of Project Settings.


#include "TOGameMode.h"
#include "../Operation/TOCardDeckComponent.h"
#include "../Operation/TOFormula.h"

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
	SubmittedFormulasThisTurn.Empty();

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

	// 3. 카드덱에서 새로운 수식 데이터 생성
	CurrentFormulaData = CardDeckComponent->GenerateRoundFormula(PlayerCount);
}

// 플레이어가 수식을 제출했을 때
void ATOGameMode::SubmitPlayerFormula(int32 PlayerIndex, const FString& SubmittedFormula)
{
	// 수식 제출 페이즈가 아니면 무시
	if (CurrentGamePhase != ETOGamePhase::SubmittingFormulas) return;

	// 이번 턴 제출 목록에 저장
	SubmittedFormulasThisTurn.Add(PlayerIndex, SubmittedFormula);

	// 모든 플레이어가 다 냈는지 검사하는 함수 호출
	// CheckAllFormulasSubmitted();
}

// [Phase 2] 타인의 카드 유추 제출
void ATOGameMode::SubmitPlayerGuess(int32 SourcePlayerIndex, int32 TargetPlayerIndex, const FString& GuessedValue)
{
	// TODO: 유추 정답 체크 및 점수/알파벳 공개 로직 구현
}

// 특정 플레이어에게 전달할 UI 데이터 반환
FTOPlayerUIData ATOGameMode::GetUIDataForPlayer(int32 TargetPlayerIndex)
{
	FTOPlayerUIData UIData;
	// TODO: UI 데이터 채워서 반환
	return UIData;
}

// 다른 플레이어의 카드 유추 성공 시 알파벳 추가
void ATOGameMode::AddRevealedAlphabetForPlayer(int32 TargetPlayerIndex, const FString& Alphabet)
{
	if (PlayerRevealedAlphabets.Contains(TargetPlayerIndex))
	{
		PlayerRevealedAlphabets[TargetPlayerIndex].Alphabets.AddUnique(Alphabet);
	}
}