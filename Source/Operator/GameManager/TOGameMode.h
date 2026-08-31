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

USTRUCT(BlueprintType)
struct FPlayerAlphabetList
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "GameData")
    TArray<FString> Alphabets;
};

UCLASS()
class OPERATOR_API ATOGameMode : public AGameMode
{
    GENERATED_BODY()
public:
    ATOGameMode();
    
    // 라운드 시작 시 호출할 함수
    UFUNCTION(BlueprintCallable, Category = "GameMode")
    void StartNewRound(int32 PlayerCount);

    // [Phase 1] 플레이어가 정답(수식)을 제출했을 때 (팀원 구조체 반영)
    UFUNCTION(BlueprintCallable, Category = "GameMode|Gameplay")
    void SubmitPlayerFormula(ATOPlayerController* SenderController, const FTOGuessAllInputData& GuessData);

    // [Phase 2] 플레이어가 단일 카드를 유추하여 제출했을 때 (팀원 구조체 반영)
    UFUNCTION(BlueprintCallable, Category = "GameMode|Gameplay")
    void SubmitPlayerGuess(ATOPlayerController* SenderController, const FTOGuessSingleInputData& SingleGuessData);

    // 특정 플레이어에게 전달할 UI 데이터를 가져오는 함수
    //UFUNCTION(BlueprintCallable, Category = "GameMode")
    //FTOPlayerUIData GetUIDataForPlayer(AController* TargetPlayer);

    // 다른 플레이어의 카드 유추에 성공했을 때 호출하는 함수
    UFUNCTION(BlueprintCallable, Category = "GameMode")
    void AddRevealedAlphabetForPlayer(int32 TargetPlayerIndex, const FString& Alphabet);

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
    TMap<int32, FPlayerAlphabetList> PlayerRevealedAlphabets;

    // 플레이어별 점수판 (PlayerIndex -> Score)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameData")
    TMap<int32, int32> PlayerScores;

private:
    // 모든 플레이어가 수식을 다 냈는지 검사하고 다음 단계로 넘어가는 내부 함수
    void CheckAllFormulasSubmitted();
};