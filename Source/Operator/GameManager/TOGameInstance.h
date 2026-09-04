#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "TOGameInstance.generated.h"

UCLASS()
class OPERATOR_API UTOGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UTOGameInstance();

	virtual void Init() override;

	UFUNCTION(BlueprintCallable, Category = "Session")
	void CreateMySession();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinMySession();

protected:
	IOnlineSessionPtr SessionInterface;

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
};