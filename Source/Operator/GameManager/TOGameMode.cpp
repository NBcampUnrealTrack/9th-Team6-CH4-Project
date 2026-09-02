// Fill out your copyright notice in the Description page of Project Settings.


#include "TOGameMode.h"
#include "../Operation/TOFormula.h"
#include "../Operation/TOTypes.h"
#include "../Operation/TOCardDeckComponent.h"
#include "TOPlayerController.h"
#include "TOPlayerState.h"

// 생성자 (컴포넌트 생성 및 기본 페이즈 설정)
ATOGameMode::ATOGameMode()
{
    CardDeckComponent = CreateDefaultSubobject<UTOCardDeckComponent>(TEXT("CardDeckComponent"));
    CurrentGamePhase = ETOGamePhase::WaitingToStart;
    
    // 커스텀 Controller 및 PlayerState 등록
    PlayerControllerClass = ATOPlayerController::StaticClass();
    PlayerStateClass = ATOPlayerState::StaticClass();
}


// 로그인 함수
void ATOGameMode::PostLogin(APlayerController* NewPlayer)
{
    // 최대 인원 (6) 초과 접속 차단
    if (GetNumPlayers() > 6)
    {
        return;
    }
    
    Super::PostLogin(NewPlayer);

    ATOPlayerController* TOPC = Cast<ATOPlayerController>(NewPlayer);
    if (!TOPC) return;

    int32 AssignedIndex = -1;

    // 이탈자 재활용 인덱스가 없으면 순차 증가 (0~5)
    if (AvailableIndices.Num() > 0)
    {
        AssignedIndex = AvailableIndices.Pop();
    }
    else
    {
        AssignedIndex = NextPlayerIndex++;
    }

    // Controller에 할당
    TOPC->AssignedPlayerIndex = AssignedIndex;

    // PlayerState에 할당 (모든 클라이언트에 Replication 동기화)
    if (ATOPlayerState* TOPS = TOPC->GetPlayerState<ATOPlayerState>())
    {
        TOPS->SetAssignedPlayerIndex(AssignedIndex);
        TOPS->bIsReadyToPlay = false; // 기본 Unready
    }
    BroadcastLobbyState();
}


// 로그아웃 함수 
void ATOGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    ATOPlayerController* TOPC = Cast<ATOPlayerController>(Exiting);
    if (TOPC && TOPC->AssignedPlayerIndex != -1)
    {
        // 퇴장한 유저의 인덱스를 재활용 배열에 등록
        AvailableIndices.Push(TOPC->AssignedPlayerIndex);
    }
    BroadcastLobbyState();
}


// 라운드 시작 함수 (알파벳 정보 초기화 및 카드덱을 통한 새 수식 생성)
void ATOGameMode::StartNewRound(int32 PlayerCount)
{
    if (!CardDeckComponent) return;

    CurrentGamePhase = ETOGamePhase::SubmittingFormulas;

    bRoundHasWinner = false;
    SubmittedPlayerIndices.Empty();
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
    for (const FTOPlayerCardData& Card : CurrentServerCardData.PlayerCards)
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            ATOPlayerController* TOPC = Cast<ATOPlayerController>(It->Get());
            if (TOPC && TOPC->AssignedPlayerIndex == Card.PlayerIndex)
            {
                if (ATOPlayerState* TOPS = TOPC->GetPlayerState<ATOPlayerState>())
                {
                    TOPS->SetPlayerAlphabet(Card.PlayerAlphabet);
                }
                break;
            }
        }
    }
    // 접속 중인 모든 플레이어 Controller에 최신 UI 데이터 브로드캐스트
    BroadcastUIUpdate();
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
        // 정답 시 승리 플래그 설정 및 점수 부여 후 라운드 종료
        bRoundHasWinner = true;
        
        if (ATOPlayerState* TOPS = SenderController->GetPlayerState<ATOPlayerState>())
        {
            TOPS->AddScorePoints(1);
        }

        EndRound(SenderIndex);
        return;
    }

    // 오답 시 전원 제출 완료 여부 확인
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

void ATOGameMode::EndRound(int32 WinnerIndex)
{
    CurrentGamePhase = ETOGamePhase::RoundOver;
    SubmittedPlayerIndices.Empty();

    BroadcastUIUpdate();

    // 3초 후 다음 라운드 시작 (현재 접속 플레이어 수 인자로 전달)
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this]()
    {
        StartNewRound(GetNumPlayers());
    }, 3.0f, false);
}

// 단계 전환 및 제출 검증 함수
void ATOGameMode::CheckAllFormulasSubmitted()
{
    if (bRoundHasWinner) return; // 이미 정답자가 나온 경우 무시
    int32 TotalPlayers = GetNumPlayers(); // 현재 접속 중인 플레이어 수

    // 모든 플레이어가 현재 페이즈의 제출/유추를 마쳤는지 확인
    if (SubmittedPlayerIndices.Num() >= TotalPlayers)
    {
        // 정답 제출을 모두 했을 때 -> 카드 유추 단계로 전환
        if (CurrentGamePhase == ETOGamePhase::SubmittingFormulas)
        {
            CurrentGamePhase = ETOGamePhase::GuessingCards;
            SubmittedPlayerIndices.Empty(); // 카드 유추 단계를 위해 목록 초기화
            BroadcastUIUpdate();
        }
        // 카드 유추를 모두 했을 때 -> 라운드 종료/정산으로 전환
        else if (CurrentGamePhase == ETOGamePhase::GuessingCards)
        {
            CurrentGamePhase = ETOGamePhase::SubmittingFormulas;
            SubmittedPlayerIndices.Empty(); // 다음라운드를 위해 목록 초기화
            BroadcastUIUpdate();
        }
    }
}

// 모든 플레이어의 UI 업데이트 함수
void ATOGameMode::BroadcastUIUpdate()
{
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

// 플레이어가 Ready 버튼을 눌렀을 때 처리 (TOPlayerController 등에서 호출 가능)
void ATOGameMode::SetPlayerReady(ATOPlayerController* TargetPC, bool bReady)
{
    if (!TargetPC || CurrentGamePhase != ETOGamePhase::WaitingToStart) return;

    // 4명 미만일 때는 준비 안 됨
    int32 CurrentPlayerCount = GetNumPlayers();
    if (CurrentPlayerCount < 4) return;

    // PlayerState의 Ready 상태 갱신
    if (ATOPlayerState* TOPS = TargetPC->GetPlayerState<ATOPlayerState>())
    {
        TOPS->bIsReadyToPlay = bReady;
    }

    // 4명 이상 접속 중이고 전원 Ready를 눌렀다면 게임 시작
    if (CheckAllPlayersReady())
    {
        StartNewRound(CurrentPlayerCount);
    }
}

bool ATOGameMode::CheckAllPlayersReady()
{
    int32 CurrentPlayerCount = GetNumPlayers();

    // 4명 미만이거나 6명 초과 시 시작 불가
    if (CurrentPlayerCount < 4 || CurrentPlayerCount > 6)
    {
        return false;
    }

    // 모든 플레이어 컨트롤러를 순회하며 Ready 상태 확인
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATOPlayerController* TOPC = Cast<ATOPlayerController>(It->Get());
        if (TOPC)
        {
            ATOPlayerState* TOPS = TOPC->GetPlayerState<ATOPlayerState>();
            // PlayerState가 없거나 Ready를 안 한 플레이어가 있으면 false
            if (!TOPS || !TOPS->bIsReadyToPlay)
            {
                return false;
            }
        }
    }

    return true; // 전원 Ready 완료
}

void ATOGameMode::BroadcastLobbyState()
{
    // 4명 이상 6명 이하일 때만 Ready 버튼에 불이 들어오도록 bool 설정
    bool bCanEnableReady = (GetNumPlayers() >= 4 && GetNumPlayers() <= 6);

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATOPlayerController* TOPC = Cast<ATOPlayerController>(It->Get());
        if (TOPC)
        {
            // TOPlayerController의 ClientRPC 호출 -> UI 버튼 활성화/비활성화 처리
            // TOPC-> 준비버튼 클릭해서 준비하는 함수 (bCanEnableReady);
        }
    }
}