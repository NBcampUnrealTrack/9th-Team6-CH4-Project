#pragma once

#include "CoreMinimal.h"
#include "TOTypes.generated.h"

// -------------------------------------------------------------------------
// Enum (연산자, 턴 상태)

UENUM(BlueprintType)
enum class ETOOperatorType : uint8
{
    Add         UMETA(DisplayName = "+"),
    Subtract    UMETA(DisplayName = "-"),
    Multiply    UMETA(DisplayName = "*")
};

UENUM(BlueprintType)
enum class ETOAnswerStage : uint8
{
    GuessAllAnswer      UMETA(DisplayName = "전체 정답 도전"),
    GuessSingleCard     UMETA(DisplayName = "타인 카드 1개 유추")
};






// -------------------------------------------------------------------------
// 기본 게임 수식 데이터 (서버 보관용)

USTRUCT(BlueprintType)
struct FTOPlayerCardData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PlayerIndex = 0; // 0=A, 1=B, 2=C, 3=D, 4=E, 5=F

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlayerAlphabet; // "A", "B", ...

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CardValue = 0; // -3 ~ 3
};

USTRUCT(BlueprintType)
struct FTOFormulaData
{
    GENERATED_BODY()

    // 참가한 플레이어들의 카드 (4~6개)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FTOPlayerCardData> PlayerCards;

    
    // 연산자 목록 (플레이어 수 - 1 개)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<ETOOperatorType> Operators;

    
    // 최종 연산 결과값 (??)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TargetResult = 0;
};





// -------------------------------------------------------------------------
// UI 및 입력 전달용 데이터 (클라이언트/서버 통신용)

USTRUCT(BlueprintType)
struct FTOPlayerUIData
{
    GENERATED_BODY()

    // 화면에 띄울 수식 텍스트 (예: "[2] + B * C - D + E = 15")
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString FormulaDisplayText;

    // 최종 연산 결과값 (??)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TargetResult = 0;

    // 내 알파벳 및 내 카드 숫자
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MyAlphabet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MyCardValue = 0;
};

// RPC 전송을 위한 Key-Value 쌍 구조체
USTRUCT(BlueprintType)
struct FTOGuessedPair
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Alphabet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GuessedValue = 0;
};

// 전체 정답 제출 시 UI에서 넘어오는 데이터 구조체
USTRUCT(BlueprintType)
struct FTOGuessAllInputData
{
    GENERATED_BODY()

    // 제출하는 플레이어 Index (0=A, 1=B ...)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SubmittingPlayerIndex = 0;

    // RPC 지원을 위해 TMap 대신 TArray<FTOGuessedPair> 사용
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FTOGuessedPair> GuessedPlayerValues;
};

// 단일 카드 유추 시 UI에서 넘어오는 데이터 구조체
USTRUCT(BlueprintType)
struct FTOGuessSingleInputData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SubmittingPlayerIndex = 0; // 제출자 Index

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetAlphabet; // 유추할 대상 알파벳 (예: "C")

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GuessedValue = 0; // 입력한 숫자 (예: 2)
};

// 플레이어가 게임 진행 중 밝혀낸 타인의 카드 정보
USTRUCT(BlueprintType)
struct FTODiscoveredCardInfo
{
    GENERATED_BODY()

    // 이미 맞춰서 공개된 알파벳 목록 (예: ["C", "E"])
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RevealedAlphabets;
};






// -------------------------------------------------------------------------
// 점수 및 순위 데이터 (GameState)

USTRUCT(BlueprintType)
struct FTOPlayerScoreData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PlayerIndex = 0; // 0=A, 1=B, ...

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlayerName; // "A", "B", ...

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Score = 0; // 누적 점수

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Rank = 0; // 최종 순위 (1위, 2위...)
};