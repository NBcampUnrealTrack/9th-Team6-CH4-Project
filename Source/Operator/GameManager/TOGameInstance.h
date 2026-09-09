#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"

class FOnlineSessionSearch;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomsFound);

#include "TOGameInstance.generated.h"

UCLASS()
class OPERATOR_API UTOGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UTOGameInstance();

	virtual void Init() override;

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FOnRoomsFound OnRoomsFound;

	UFUNCTION(BlueprintCallable, Category = "Session")
	int32 GetFoundRoomCount() const;

	UFUNCTION(BlueprintCallable, Category = "Session")
	FString GetFoundRoomName(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category = "Session")
	int32 GetFoundRoomPlayerCount(int32 Index) const;
	
	UFUNCTION(BlueprintCallable, Category = "Session")
	void CreateMySession();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void CreateRoom();
    
	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinMySession();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinRoom();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void FindRooms();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinFoundSession(int32 Index);
    
	// =========================================================================
	// Customization Selection Data
	// =========================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|PlayerInfo")
	FString PlayerName = TEXT("Player");
	
	//getter/setter
	void SetPlayerName(FString& InMyPlayerName){PlayerName = InMyPlayerName;};
	FString GetPlayerName() { return PlayerName; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Customization")
	int32 SelectedHairIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Customization")
	int32 SelectedTopIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Customization")
	int32 SelectedBottomIndex = 0;

protected:
	IOnlineSessionPtr SessionInterface;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnFindSessionsComplete(bool bWasSuccessful);
};