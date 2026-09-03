#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "../Operation/TOTypes.h"
#include "TOGameMode.generated.h"

class UTOCardDeckComponent;
class ATOPlayerController;

// 게임 진행 페이즈 정의
UENUM(BlueprintType)
enum class ETOGamePhase : uint8
{
    WaitingToStart,     // 대기 중
    SubmittingFormulas, // Phase 1: 수식 조합 및 제출 단계
    GuessingCards,      // Phase 2: 타인 카드 유추 단계
    RoundOver           // 라운드 종료 / 점수 정산
};


UCLASS()
class OPERATOR_API ATOGameMode : public AGameMode
{
    GENERATED_BODY()
    
    
public:
    
    // --------------------- 게임 기본 관리 및 라이프사이클 파트 ---------------------------
    
    ATOGameMode();
    
    // 플레이어 로그인/로그아웃 
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    
    // 라운드 시작 시 호출할 함수
    UFUNCTION(BlueprintCallable, Category = "GameMode")
    void StartNewRound(int32 PlayerCount);

    // 라운드 종료 처리
    UFUNCTION(BlueprintCallable, Category = "GameMode")
    void EndRound(int32 WinnerIndex);
        
    // ------------------- 네트워크 입력 및 데이터 제출 파트 -----------------------------
    
public:
    
    // 플레이어가 Ready 버튼을 눌렀을 때 호출하는 함수
    void SetPlayerReady(ATOPlayerController* TargetPC, bool bReady);
    
private:
    
    // 4명 이상 접속 여부에 따라 Ready 버튼 활성화 함수
    void BroadcastLobbyState();
    
    // 현재 접속한 전원이 Ready 상태인지 검사
    bool CheckAllPlayersReady();
    
    
    // ------------------- UI 데이터 전달 및 헬퍼 파트 -----------------------------
    
public:
    
    // 특정 플레이어에게 전달할 UI 데이터를 가져오는 함수
    UFUNCTION(BlueprintCallable, Category = "GameMode")
    FTOPlayerUIData GetUIDataForPlayer(AController* TargetPlayer);

    // 다른 플레이어의 카드 유추에 성공했을 때 호출하는 함수
    UFUNCTION(BlueprintCallable, Category = "GameMode")
    void AddRevealedAlphabetForPlayer(int32 TargetPlayerIndex, const FString& Alphabet);
    
    
    
    // ------------------- 게임 컴포넌트 및 핵심 데이터 파트 -----------------------------

protected:
    // 카드덱 컴포넌트 변수 선언 (서버 소유)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UTOCardDeckComponent> CardDeckComponent;

    // 현재 게임 페이즈 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameData")
    ETOGamePhase CurrentGamePhase;

    // 현재 라운드의 원본 수식 데이터를 서버 메모리에 저장해둘 변수 (TOTypes에 정의된 구조체 사용)
    UPROPERTY(BlueprintReadOnly, Category = "GameData")
    FTOFormulaData CurrentServerCardData; // 서버 전체 정답 데이터 저장소

    // 플레이어 (Index)별로 정답을 맞혀서 알고 있는 알파벳 목록 저장
    UPROPERTY(BlueprintReadOnly, Category = "GameData")
    TMap<int32, FTODiscoveredCardInfo> PlayerRevealedAlphabets;

    // 플레이어별 점수판 (PlayerIndex -> Score)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameData")
    TMap<int32, int32> PlayerScores;
    
    
    
    // ------------------- 제출 받은 내용 보관 파트 -----------------------------
    
    // 이번 라운드에 수식을 제출 완료한 플레이어 인덱스 목록
    UPROPERTY(BlueprintReadOnly, Category = "GameData")
    TSet<int32> SubmittedPlayerIndices;
    
    // 일괄 검증을 위한 각 페이즈 제출 데이터 임시 보관함
    UPROPERTY()
    TMap<int32, FTOGuessAllInputData> PendingFormulaSubmissions;

    UPROPERTY()
    TMap<int32, FTOGuessSingleInputData> PendingGuessSubmissions;

    
    
    // ------------------- 플레이어 정보 파트 -----------------------------
    
    // 순차 부여할 인덱스 카운터 (0 ~ 5)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameData")
    int32 NextPlayerIndex = 0;

    // 중간에 다른 플레이어가 나갔을 때 그 자리를 비우고, 또 다른 플레이어가 접속했을 때 재사용
    TArray<int32> AvailableIndices;
    
    // 라운드 승리자 발생 여부 플래그
    bool bRoundHasWinner = false;
    
    
    
    // ------------------- 정답/유추 검증 및 내부 처리 파트 -----------------------------
public:
    
    // 정답/카드 유추 제출 함수
    void SubmitPlayerInput(ATOPlayerController* SenderController, const FTOGuessAllInputData& FormulaData, const FTOGuessSingleInputData& SingleGuessData);
    
private:
    
    // 모든 플레이어가 수식을 다 냈는지 확인하고 일괄 검증 함수를 호출하는 함수
    void CheckAllFormulasSubmitted();
    
    // 정답/유추 제출 시 일괄 검증
    void ProcessAllFormulaSubmissions();
    void ProcessAllGuessSubmissions();
    
    // 변동 내용이 생길 때 모든 클라이언트의 UI를 일괄 갱신하는 함수
    void BroadcastUIUpdate();
};