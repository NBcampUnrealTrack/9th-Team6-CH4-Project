#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TOSpawnPoint.generated.h"

UCLASS()
class OPERATOR_API ATOSpawnPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	ATOSpawnPoint();

	// 몇 번 플레이어가 스폰될 자리인지 지정 (0 ~ 5)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TO|Spawn")
	int32 SpawnIndex = 0;
	
	// 에디터에서 회전 방향과 위치를 보기 쉽게 화살표 컴포넌트 추가
	UPROPERTY(VisibleAnywhere, Category = "TO|Spawn")
	class UArrowComponent* ArrowComponent;
};