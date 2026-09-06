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
	void CreateRoom();
	
	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinMySession();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinRoom();
	
	// =========================================================================
	// Customization Selection Data
	// =========================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Customization")
	int32 SelectedHairIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Customization")
	int32 SelectedTopIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Customization")
	int32 SelectedBottomIndex = 0;

protected:
	IOnlineSessionPtr SessionInterface;

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
};