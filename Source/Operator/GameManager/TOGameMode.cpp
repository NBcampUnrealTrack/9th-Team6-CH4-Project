// Fill out your copyright notice in the Description page of Project Settings.


#include "TOGameMode.h"
#include "../Operation/TOFormula.h"
#include "../Operation/TOTypes.h"
#include "../Operation/TOCardDeckComponent.h"
#include "TOPlayerController.h"
#include "TOPlayerState.h"
#include "EngineUtils.h"
#include "../Interactive/TOSpawnPoint.h"


// 생성자 (컴포넌트 생성 및 기본 페이즈 설정)
ATOGameMode::ATOGameMode()
{
    CardDeckComponent = CreateDefaultSubobject<UTOCardDeckComponent>(TEXT("CardDeckComponent"));
    CurrentGamePhase = ETOGamePhase::WaitingToStart;
    
    // 커스텀 Controller 및 PlayerState 등록
    // PlayerController 강제 할당하지않고
    // BP_TOGameMode - PlayerController class 를 BP_TOPlayerController로 적용
    PlayerStateClass = ATOPlayerState::StaticClass();
    
    // 스폰 구역(좌석) 확보
    SpawnPositions.SetNum(6);
}


// 접속자 처리 시작 전 인덱스 재활용 배열을 안전하게 초기화
void ATOGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    AvailableIndices.Empty();
    NextPlayerIndex = 0;
}


void ATOGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 레벨 내에 배치된 "TOSpawnPoint" 태그를 가진 액터들을 자동으로 찾아 Index 순서대로 정렬 (에디터 수동 할당도 가능)
    for (TActorIterator<ATOSpawnPoint> It(GetWorld()); It; ++It)
    {
        ATOSpawnPoint* SpawnPoint = *It;
        if (SpawnPoint && SpawnPositions.IsValidIndex(SpawnPoint->SpawnIndex))
        {
            SpawnPositions[SpawnPoint->SpawnIndex] = SpawnPoint;
        }
    }
}

// 로그인 함수
void ATOGameMode::PostLogin(APlayerController* NewPlayer)
{
    // 최대 인원 (6) 초과 접속 차단
    if (GetNumPlayers() > 6)
    {
        return;
    }

    ATOPlayerController* TOPC = Cast<ATOPlayerController>(NewPlayer);
    if (TOPC)
    {
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
    }
    TOPC->Multicast_UpdateMainUI();
    BroadcastLobbyState();
    Super::PostLogin(NewPlayer);
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
    // 6명 중 1명이 퇴장하여 5명이 되었을 때, 남은 준비 상태로 인해 자동으로 시작되는 현상 방지
    // 누군가 나가면 Ready 상태였던 모든 플레이어의 준비 상태를 false로 변경
    ResetAllPlayersReadyState();
    
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
    // 여기에 추가 내용 구현(라운드 종료 연출 나오고 몇초 뒤 이동 등)
    
    // 현재 세션 및 플레이어들을 그대로 유지하면서 게임 페이즈만 대기(로비) 상태로 변경
    CurrentGamePhase = ETOGamePhase::WaitingToStart;

    // 모든 플레이어의 Ready 상태를 false로 초기화
    ResetAllPlayersReadyState();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATOPlayerController* TOPC = Cast<ATOPlayerController>(It->Get());
        if (TOPC)
        {
            TOPC->Client_OnGameEnded();
        }
    }
    BroadcastLobbyState();
}

// CurrentGamePhase에 따라 필요한 보관함으로 분기 처리 (일괄 검증을 위해)
void ATOGameMode::SubmitPlayerInput(ATOPlayerController* SenderController, const FTOGuessAllInputData& FormulaData, const FTOGuessSingleInputData& SingleGuessData)
{
    if (!SenderController) return;

    int32 SenderIndex = SenderController->AssignedPlayerIndex;
    if (SubmittedPlayerIndices.Contains(SenderIndex)) return;

    if (CurrentGamePhase == ETOGamePhase::SubmittingFormulas)
    {
        SubmittedPlayerIndices.Add(SenderIndex);
        PendingFormulaSubmissions.Add(SenderIndex, FormulaData);
        CheckAllFormulasSubmitted();
    }
    else if (CurrentGamePhase == ETOGamePhase::GuessingCards)
    {
        SubmittedPlayerIndices.Add(SenderIndex);
        PendingGuessSubmissions.Add(SenderIndex, SingleGuessData);
        CheckAllFormulasSubmitted();
    }
}

// 현재 턴(CurrentGamePhase) 에 따라 검증하는 함수 알맞게 적용해주는 함수
void ATOGameMode::CheckAllFormulasSubmitted()
{
    if (bRoundHasWinner) return;
    int32 TotalPlayers = GetNumPlayers();

    if (SubmittedPlayerIndices.Num() >= TotalPlayers)
    {
        if (CurrentGamePhase == ETOGamePhase::SubmittingFormulas)
        {
            ProcessAllFormulaSubmissions();
        }
        else if (CurrentGamePhase == ETOGamePhase::GuessingCards)
        {
            ProcessAllGuessSubmissions();
        }
    }
}

// 
void ATOGameMode::ProcessAllFormulaSubmissions()
{
    TArray<int32> WinnersThisRound;

    for (const auto& Pair : PendingFormulaSubmissions)
    {
        if (UTOFormula::VerifyAllAnswerWithMap(CurrentServerCardData, Pair.Value))
        {
            WinnersThisRound.Add(Pair.Key);
        }
    }

    PendingFormulaSubmissions.Empty();
    SubmittedPlayerIndices.Empty();

    if (WinnersThisRound.Num() > 0)
    {
        bRoundHasWinner = true;
        int32 WinnerIndex = WinnersThisRound[0];

        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            ATOPlayerController* TOPC = Cast<ATOPlayerController>(It->Get());
            if (TOPC && WinnersThisRound.Contains(TOPC->AssignedPlayerIndex))
            {
                if (ATOPlayerState* TOPS = TOPC->GetPlayerState<ATOPlayerState>())
                {
                    TOPS->AddScorePoints(1);
                }
                TOPC->Multicast_UpdateMainUI();
                // 모든 클라이언트 화면에 브로드캐스트
                TOPC->Multicast_ShowCorrectNotice(WinnerIndex);
                break;
            }
        }
        //3초 연출 후 라운드 종료 타이머 세팅
        FTimerHandle NoticeTimerHandle;
        GetWorldTimerManager().SetTimer(NoticeTimerHandle, FTimerDelegate::CreateLambda([this, WinnerIndex]()
        {
            EndRound(WinnerIndex);
        }), 3.0f, false);
    }
    else
    {
        // 정답자가 없으면 카드 유추 단계로 전환 후 UI 전파
        CurrentGamePhase = ETOGamePhase::GuessingCards;
        BroadcastUIUpdate();
    }
}

void ATOGameMode::ProcessAllGuessSubmissions()
{
    for (const auto& Pair : PendingGuessSubmissions)
    {
        int32 PlayerIndex = Pair.Key;
        const FTOGuessSingleInputData& SingleGuessData = Pair.Value;

        bool bIsMatch = UTOFormula::VerifySingleCard(CurrentServerCardData, SingleGuessData);
        int32 RevealedVal = -999;

        if (bIsMatch)
        {
            AddRevealedAlphabetForPlayer(PlayerIndex, SingleGuessData.TargetAlphabet);
            for (const FTOPlayerCardData& Card : CurrentServerCardData.PlayerCards)
            {
                if (Card.PlayerAlphabet == SingleGuessData.TargetAlphabet)
                {
                    RevealedVal = Card.CardValue;
                    break;
                }
            }
        }

        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            ATOPlayerController* TOPC = Cast<ATOPlayerController>(It->Get());
            if (TOPC && TOPC->AssignedPlayerIndex == PlayerIndex)
            {
                TOPC->Client_ReceiveGuessResult(bIsMatch, SingleGuessData.TargetAlphabet, RevealedVal);
                break;
            }
        }
    }

    PendingGuessSubmissions.Empty();
    SubmittedPlayerIndices.Empty();

    // 유추 처리 완료 후 다시 정답 제출 단계로 전환 후 UI 전파
    CurrentGamePhase = ETOGamePhase::SubmittingFormulas;
    BroadcastUIUpdate();
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
        // 모든 클라이언트의 UI를 로비 -> 인게임 HUD로 전환 요청
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            ATOPlayerController* TOPC = Cast<ATOPlayerController>(It->Get());
            if (TOPC)
            {
                TOPC->Client_OnGameStarted();
            }
        }

        StartNewRound(CurrentPlayerCount);
    }
}

bool ATOGameMode::CheckAllPlayersReady()
{
    int32 ValidPlayerCount = 0;
    int32 ReadyPlayerCount = 0;

    // 접속해 있는 유효한 PlayerController들을 순회하며 카운트
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATOPlayerController* TOPC = Cast<ATOPlayerController>(It->Get());
        // 유효한 플레이어 컨트롤러 및 PlayerState 확인
        if (TOPC && TOPC->AssignedPlayerIndex != -1)
        {
            ATOPlayerState* TOPS = TOPC->GetPlayerState<ATOPlayerState>();
            if (TOPS)
            {
                ValidPlayerCount++;

                // Ready 상태인 플레이어 카운트
                if (TOPS->bIsReadyToPlay)
                {
                    ReadyPlayerCount++;
                }
            }
        }
    }
    const int32 MinRequiredPlayers = 4; 

    if (ValidPlayerCount < MinRequiredPlayers || ValidPlayerCount > 6)
    {
        return false;
    }

    // 3. 접속한 모든 유저가 Ready를 눌렀는지 검증
    return (ValidPlayerCount == ReadyPlayerCount);
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
            TOPC->Client_UpdateLobbyState(bCanEnableReady);
        }
    }
}


void ATOGameMode::ResetAllPlayersReadyState()
{
    // 현재 서버에 접속되어 있는 모든 플레이어 컨트롤러 순회
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATOPlayerController* TOPC = Cast<ATOPlayerController>(It->Get());
        if (TOPC)
        {
            // 각 플레이어의 PlayerState 접근
            ATOPlayerState* TOPS = TOPC->GetPlayerState<ATOPlayerState>();
            if (TOPS)
            {
                // Ready 플래그를 false(준비 해제)로 변경
                TOPS->bIsReadyToPlay = false;
            }
        }
    }
}




void ATOGameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);

    ATOPlayerController* TOPC = Cast<ATOPlayerController>(NewPlayer);
    if (!TOPC) return;

    int32 PlayerIndex = TOPC->AssignedPlayerIndex;
    if (PlayerIndex < 0) return;

    AActor* TargetSpawnPoint = GetSpawnPointForIndex(PlayerIndex);

    if (!TargetSpawnPoint)
    {
        for (TActorIterator<ATOSpawnPoint> It(GetWorld()); It; ++It)
        {
            ATOSpawnPoint* SpawnPoint = *It;
            if (SpawnPoint && SpawnPoint->SpawnIndex == PlayerIndex)
            {
                TargetSpawnPoint = SpawnPoint;
                break;
            }
        }
    }
    
    if (TargetSpawnPoint)
    {
        APawn* TargetPawn = TOPC->GetPawn();
        if (TargetPawn)
        {
            FVector TargetLocation = TargetSpawnPoint->GetActorLocation();
            FRotator TargetRotation = TargetSpawnPoint->GetActorRotation();

            // ETeleportType::TeleportPhysics로 충돌을 무시하고 스폰 포인트에 강력 고정
            TargetPawn->SetActorLocationAndRotation(
                TargetLocation, 
                TargetRotation, 
                false, 
                nullptr, 
                ETeleportType::TeleportPhysics
            );

            // Controller 시선(Control Rotation) 동기화
            TOPC->SetControlRotation(TargetRotation);
            TOPC->ClientSetRotation(TargetRotation, true);
        }
    }
}

AActor* ATOGameMode::GetSpawnPointForIndex(int32 PlayerIndex)
{
    if (SpawnPositions.IsValidIndex(PlayerIndex) && SpawnPositions[PlayerIndex] != nullptr)
    {
        return SpawnPositions[PlayerIndex];
    }
    return nullptr;
}