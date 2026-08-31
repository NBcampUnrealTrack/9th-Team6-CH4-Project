// Fill out your copyright notice in the Description page of Project Settings.


#include "TOGameMode.h"
#include "../Operation/TOFormula.h"
#include "../Operation/TOTypes.h"
#include "../Operation/TOCardDeckComponent.h"
#include "TOPlayerController.h"

// 생성자 (컴포넌트 생성 및 기본 페이즈 설정)
ATOGameMode::ATOGameMode()
{
    CardDeckComponent = CreateDefaultSubobject<UTOCardDeckComponent>(TEXT("CardDeckComponent"));
    CurrentGamePhase = ETOGamePhase::WaitingToStart;
}



// 라운드 시작 함수 (알파벳 정보 초기화 및 카드덱을 통한 새 수식 생성)
void ATOGameMode::StartNewRound(int32 PlayerCount)
{
    if (!CardDeckComponent) return;

    CurrentGamePhase = ETOGamePhase::SubmittingFormulas;

    PlayerRevealedAlphabets.Empty();
    for (int32 i = 0; i < PlayerCount; i++)
    {
        FTODiscoveredCardInfo NewInfo;
        PlayerRevealedAlphabets.Add(i, NewInfo);

        if (!PlayerScores.Contains(i))
        {
            PlayerScores.Add(i, 0);
        }
    }

    // TOCardDeckComponent를 통해 라운드 수식 생성
    CurrentServerCardData = CardDeckComponent->GenerateRoundFormula(PlayerCount);

    // 접속 중인 모든 플레이어 Controller에 최신 UI 데이터 브로드캐스트
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATOPlayerController* TOPC = Cast<ATOPlayerController>(It->Get());
        if (TOPC)
        {
            FTOPlayerUIData UIData = GetUIDataForPlayer(TOPC);
            TOPC->Client_UpdateFormulaUI(UIData);
        }
    }
}



// [Phase 1] 플레이어가 수식을 제출했을 때
void ATOGameMode::SubmitPlayerFormula(ATOPlayerController* SenderController, const FTOGuessAllInputData& GuessData)
{
    if (!SenderController || CurrentGamePhase != ETOGamePhase::SubmittingFormulas) return;

    int32 SenderIndex = SenderController->AssignedPlayerIndex;
    if (SubmittedPlayerIndices.Contains(SenderIndex)) return;    // 이미 제출한 플레이어면 중복 처리 방지

    // 제출 기록 추가
    SubmittedPlayerIndices.Add(SenderIndex);

    // 정답 검증 처리
    bool bIsCorrect = UTOFormula::VerifyAllAnswerWithMap(CurrentServerCardData, GuessData);
    if (bIsCorrect)
    {
        // 정답을 맞췄을 때의 처리 (점수 부여 등)
    }

    // 모든 플레이어가 제출했는지 체크
    SubmittedPlayerIndices.Add(SenderIndex);
    CheckAllFormulasSubmitted();
}



// [Phase 2] 타인의 카드 유추 제출
void ATOGameMode::SubmitPlayerGuess(ATOPlayerController* SenderController, const FTOGuessSingleInputData& SingleGuessData)
{
    if (!SenderController || CurrentGamePhase != ETOGamePhase::GuessingCards) return;

    int32 SenderIndex = SenderController->AssignedPlayerIndex;
    if (SubmittedPlayerIndices.Contains(SenderIndex)) return; // 이미 유추 제출함

    // 단일 카드 유추 검증
    bool bIsMatch = UTOFormula::VerifySingleCard(CurrentServerCardData, SingleGuessData);

    // 무효값을 둬서 유추 실패 시 안전 장치 구현
    int32 RevealedVal = -999;
    if (bIsMatch)
    {
        AddRevealedAlphabetForPlayer(SenderIndex, SingleGuessData.TargetAlphabet);

        for (const FTOPlayerCardData& Card : CurrentServerCardData.PlayerCards)
        {
            if (Card.PlayerAlphabet == SingleGuessData.TargetAlphabet)
            {
                RevealedVal = Card.CardValue;
                break;
            }
        }

        FTOPlayerUIData UpdatedUIData = GetUIDataForPlayer(SenderController);
        SenderController->Client_UpdateFormulaUI(UpdatedUIData);
    }

    SenderController->Client_ReceiveGuessResult(bIsMatch, SingleGuessData.TargetAlphabet, RevealedVal);

    // 제출한 플레이어 목록에 추가 후 전체 제출 여부 체크
    SubmittedPlayerIndices.Add(SenderIndex);
    CheckAllFormulasSubmitted();
}



// 대상 플레이어의 Controller에서 인덱스를 추출하여 해당 플레이어가 알고 있는 정보 기반의 UI 데이터 구축
FTOPlayerUIData ATOGameMode::GetUIDataForPlayer(AController* TargetPlayer)
{
    FTOPlayerUIData UIData;

    ATOPlayerController* TOPC = Cast<ATOPlayerController>(TargetPlayer);
    if (TOPC)
    {
        int32 TargetIndex = TOPC->AssignedPlayerIndex;

        TArray<FString> RevealedList;
        if (PlayerRevealedAlphabets.Contains(TargetIndex))
        {
            RevealedList = PlayerRevealedAlphabets[TargetIndex].RevealedAlphabets;
        }

        // TOFormula 라이브러리를 사용해 맞춤형 UI 데이터 빌드
        UIData = UTOFormula::BuildUIDataForPlayer(CurrentServerCardData, TargetIndex, RevealedList);
    }

    return UIData;
}

// 다른 플레이어의 카드 유추 성공 시 알파벳 추가
void ATOGameMode::AddRevealedAlphabetForPlayer(int32 TargetPlayerIndex, const FString& Alphabet)
{
    if (PlayerRevealedAlphabets.Contains(TargetPlayerIndex))
    {
       PlayerRevealedAlphabets[TargetPlayerIndex].RevealedAlphabets.AddUnique(Alphabet);
    }
}

void ATOGameMode::CheckAllFormulasSubmitted()
{
    int32 TotalPlayers = GetNumPlayers(); // 현재 접속 중인 플레이어 수

    // 모든 플레이어가 현재 페이즈의 제출/유추를 마쳤는지 확인
    if (SubmittedPlayerIndices.Num() >= TotalPlayers)
    {
        // 정답 제출을 모두 했을 때 -> 카드 유추 단계로 전환
        if (CurrentGamePhase == ETOGamePhase::SubmittingFormulas)
        {
            UE_LOG(LogTemp, Warning, TEXT("모든 플레이어 수식 제출 완료 -> 카드 유추 페이즈로 전환"));
            CurrentGamePhase = ETOGamePhase::GuessingCards;
            SubmittedPlayerIndices.Empty(); // 카드 유추 단계를 위해 목록 초기화
        }
        // 카드 유추를 모두 했을 때 -> 라운드 종료/정산으로 전환
        else if (CurrentGamePhase == ETOGamePhase::GuessingCards)
        {
            UE_LOG(LogTemp, Warning, TEXT("모든 플레이어 카드 유추 완료 -> 라운드 종료"));
            CurrentGamePhase = ETOGamePhase::RoundOver;
            SubmittedPlayerIndices.Empty(); // 다음라운드를 위해 목록 초기화
			
            // 라운드 종료 처리 로직 호출 (점수 정산, 다음 라운드 대기 등)
        }
    }
}
